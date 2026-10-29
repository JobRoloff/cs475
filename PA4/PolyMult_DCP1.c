#ifdef TRACE
#define _GNU_SOURCE           // sched_getcpu
#endif

#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "kernel.h"
#include "accumulate.h"

// TRACE build: per thread, its CPU at region start, the leaves it ran and
// the time spent in them, and which thread ran the single block; per call,
// the phases from kernel entry to exit. Output goes to stderr, so the csv on
// stdout is unchanged.
#ifdef TRACE
#ifndef _OPENMP
#error "TRACE needs an OpenMP build"
#endif
#include <omp.h>
#include <sched.h>

#define TR_MAX 256

static long tr_leaves;
static double tr_secs, tr_first, tr_last;   // first leaf start, last leaf end
#pragma omp threadprivate(tr_leaves, tr_secs, tr_first, tr_last)

static int tr_cpu[TR_MAX];
static long tr_lv[TR_MAX];
static double tr_ms[TR_MAX], tr_first_all[TR_MAX], tr_last_all[TR_MAX];
static double tr_arrive[TR_MAX];             // when each thread entered the region
static int tr_single, tr_nthr;
static double tr_tg_end;
static double tr_bar, tr_sbar, tr_merge_end;  // TRED=3: zeroing barrier, single's barrier, merge end
static int tr_root;                           // thread that ran the root node's task

// The root is the only subproblem at (0, 0) whose outputs span all of c.
#define TR_ROOT(x, i0, na, j0, nb) \
    if ((i0) == 0 && (j0) == 0 && (na) + (nb) - 1 == (x)->out_len) tr_root = omp_get_thread_num()
#else
#define TR_ROOT(x, i0, na, j0, nb) ((void)0)
#endif

// Leaf size when --leaf-size is not given: BKQ's best single-core BS in PA3.
#define LEAF_DEFAULT 1280

// Task cutoff when --task-threshold is not given, in leaves per side: a
// subproblem of <= 2 x 2 leaves runs as one task.
#define CUTOFF_DEFAULT 2

#define JAM_R  8              // rows of a fused per pass (unroll-and-jam)
#define IL_U   3              // independent 8-float vectors per pass
#define NCOPY  4              // shifted copies of each b block (shifts 0..3)
#define PAD    (JAM_R - 1)    // zeros on each side of a b block

#define STR_(x) #x
#define STR(x)  STR_(x)

// Reduction list items. The checking build accumulates into comp too, so
// each thread needs a private comp alongside its private c.
// TRED=1/2 combine comp with plain + alongside c. That rule is unapproved:
// accumulate.h defines no combine, and summing the float c copies loses the
// merge's own rounding. TRED=3 uses the rule derived from c - comp below.
#ifdef CHECKING
#define RED_ITEMS(n) c[0:(n)], comp[0:(n)]
#else
#define RED_ITEMS(n) c[0:(n)]
#endif

// Where the reduction lives.
//   TRED=1 (default): taskgroup task_reduction inside the single.
//   TRED=2: reduction(task, +: ...) on the parallel region; the taskgroup
//           stays, without a clause.
//   TRED=3: no OpenMP reduction clauses; one hand-allocated private copy
//           per thread, picked by omp_get_thread_num() where a sequential
//           subtree runs, summed into c by an omp for after the single.
// RACE build (negative control): no reduction clauses at all, so the same
// task tree writes the shared c (and comp) directly.
#ifndef TRED
#define TRED 1
#endif
#if TRED != 1 && TRED != 2 && TRED != 3
#error "TRED must be 1, 2 or 3"
#endif

// How a node above the task cutoff runs its children.
//   SPAWN=1 (default): every child is a task; no taskwait.
//   SPAWN=2: every child is a task, then taskwait.
//   SPAWN=3: all children but the last are tasks, the last runs directly in
//            the current task, then taskwait.
// SPAWN=2/3 exist for TRED=3 only.
#ifndef SPAWN
#define SPAWN 1
#endif
#if SPAWN != 1 && SPAWN != 2 && SPAWN != 3
#error "SPAWN must be 1, 2 or 3"
#endif
#if SPAWN != 1 && !(TRED == 3 || (TRED == 1 && SPAWN == 3))
#error "SPAWN=2/3 are implemented for TRED=3, plus SPAWN=3 with TRED=1"
#endif

// NOTG=1 (TRED=2/3 only): no taskgroup around the root. The single's
// implicit barrier (TRED=3) or the region's end (TRED=2) waits for the tasks.
// ROOTDIRECT=1: the single thread runs the root node itself (dc_node)
// instead of creating it as a task. For TRED=1 its direct leaves write the
// original c, which the taskgroup's merge adds the private copies into; for
// TRED=2 they write the single thread's implicit-task copy.
#ifndef NOTG
#define NOTG 0
#endif
#ifndef ROOTDIRECT
#define ROOTDIRECT 0
#endif
#if NOTG && TRED == 1
#error "NOTG needs TRED=2 or 3: TRED=1's reduction is the taskgroup's"
#endif

#if TRED == 3
#include <assert.h>
#ifdef _OPENMP
#include <omp.h>
#define DC_TID()      omp_get_thread_num()
#define DC_NTHREADS() omp_get_num_threads()
#define DC_MAXTHR()   omp_get_max_threads()
#else
#define DC_TID()      0
#define DC_NTHREADS() 1
#define DC_MAXTHR()   1
#endif
#endif

#ifdef RACE
#define PAR_RED
#define TG_RED
#define TASK_RED
#elif TRED == 3
#define PAR_RED
#define TG_RED
#define TASK_RED
#elif TRED == 2
#define PAR_RED  reduction(task, +: RED_ITEMS(out_len))
#define TG_RED
#define TASK_RED in_reduction(+: RED_ITEMS(x->out_len))
#else
#define PAR_RED
#define TG_RED   task_reduction(+: RED_ITEMS(out_len))
#define TASK_RED in_reduction(+: RED_ITEMS(x->out_len))
#endif

// Only non-default switches are listed, so the default build keeps the
// string earlier DCP1 rows were recorded with.
#if TRED == 1
#define BP_TRED ""
#else
#define BP_TRED ";tred=" STR(TRED)
#endif
#if SPAWN == 1
#define BP_SPAWN ""
#else
#define BP_SPAWN ";spawn=" STR(SPAWN)
#endif
#if NOTG
#define BP_NOTG ";notg=1"
#else
#define BP_NOTG ""
#endif
#if ROOTDIRECT
#define BP_RD ";rootdirect=1"
#else
#define BP_RD ""
#endif

const char *kernel_build_params(void) {
    return "r=" STR(JAM_R) ";u=" STR(IL_U) BP_TRED BP_SPAWN BP_NOTG BP_RD;
}

// Everything the recursion carries unchanged from the top-level call.
// c (and comp) are the shared originals: they only name the reduction list
// items when a task is created, and are never written through.
typedef struct {
    const float *a;
    float *c;
    int out_len;
    int leaf;             // leaf size L: b blocks start at multiples of L
    long cut;             // task cutoff, in elements per side (cutoff * L)
    int clen;             // floats per shifted copy (multiple of 16)
    const float *bcopy;   // NCOPY shifted, padded copies of every b block
#ifdef CHECKING
    double *comp;
#endif
#if TRED == 3
    float *copies;        // one private copy of c per thread, stride apart
    size_t stride;        // floats per copy (multiple of 16)
#ifdef CHECKING
    double *ccopies;      // one private comp per thread, same stride
#endif
#endif
} dc_ctx_t;

// The output a task writes: its own private copy, read inside the task.
typedef struct {
    float *c;
#ifdef CHECKING
    double *comp;
#endif
} dc_out_t;

// BKQ's final kernel over rows [i0, i0 + na) of a and the b block at j0
// (len elements), accumulating into o.c at i0 + j0.
__attribute__((optimize("no-associative-math")))
static void leaf(const dc_ctx_t *restrict x, dc_out_t o, int i0, int na, int j0, int len) {
    const float *restrict a_al = (const float *)__builtin_assume_aligned(x->a, 16);
    // A private copy of c need not be 16-byte aligned, so no alignment is
    // assumed here; h below is taken from the actual address instead.
    float *restrict c_al = o.c;
#ifdef CHECKING
    double *restrict comp = o.comp;
#endif
    int out_len = x->out_len;
    int ilast = i0 + na - 1;

    const float *bcopy = x->bcopy + (size_t)(j0 / x->leaf) * NCOPY * x->clen;

    // Row groups start at i0 + multiples of JAM_R, so the first output of
    // every group has the same address mod 16 as &c[i0 + j0]. h scalar
    // outputs bring c to a 16-byte boundary; vector passes start at m = h.
    int h = (int)((16 - ((uintptr_t)&c_al[i0 + j0] & 15)) & 15) / (int)sizeof(float);

    // Row r reads P[m + PAD - r]. Pick the copy that puts that window
    // on a 16-byte boundary when m == h (mod 4); rows r and r+4 share it.
    const float *brow[JAM_R];
    for (int r = 0; r < JAM_R; r++) {
        int s = (r - PAD - h) & 3;
        brow[r] = bcopy + s * x->clen + PAD - r + s;
    }

    // Rows past ilast are zero-padded rows of a, so the last group runs
    // the same code; outputs are clamped to c's length.
    for (int i = i0; i <= ilast; i += JAM_R) {
        float ar[JAM_R];
        for (int r = 0; r < JAM_R; r++) {
            ar[r] = (i + r <= ilast) ? a_al[i + r] : 0.0f;
        }

        int k0 = i + j0;
        int nout = len + JAM_R - 1;
        if (nout > out_len - k0) {
            nout = out_len - k0;
        }

        int m = 0;

        // Head: up to 3 scalar outputs to align c to 16 bytes
        int hcnt = (h < nout) ? h : nout;
        for (; m < hcnt; m++) {
            for (int r = 0; r < JAM_R; r++) {
                BPM_ACC(c_al, comp, k0 + m, ar[r] * brow[r][m]);
            }
        }

#ifndef CHECKING
        __m256 va[JAM_R];
        for (int r = 0; r < JAM_R; r++) {
            va[r] = _mm256_set1_ps(ar[r]);
        }
#endif

        // Body: IL_U interleaved vectors (24 outputs) per pass
        for (; m + IL_U * 8 <= nout; m += IL_U * 8) {
#ifdef CHECKING
            for (int l = 0; l < IL_U * 8; l++) {
                for (int r = 0; r < JAM_R; r++) {
                    BPM_ACC(c_al, comp, k0 + m + l, ar[r] * brow[r][m + l]);
                }
            }
#else
            __m256 vc0 = _mm256_loadu_ps(&c_al[k0 + m]);
            __m256 vc1 = _mm256_loadu_ps(&c_al[k0 + m + 8]);
            __m256 vc2 = _mm256_loadu_ps(&c_al[k0 + m + 16]);

#pragma GCC unroll 8
            for (int r = 0; r < JAM_R; r++) {
                vc0 = _mm256_fmadd_ps(va[r], _mm256_loadu_ps(&brow[r][m]), vc0);
                vc1 = _mm256_fmadd_ps(va[r], _mm256_loadu_ps(&brow[r][m + 8]), vc1);
                vc2 = _mm256_fmadd_ps(va[r], _mm256_loadu_ps(&brow[r][m + 16]), vc2);
            }

            _mm256_storeu_ps(&c_al[k0 + m], vc0);
            _mm256_storeu_ps(&c_al[k0 + m + 8], vc1);
            _mm256_storeu_ps(&c_al[k0 + m + 16], vc2);
#endif
        }

        // Cleanup: remaining whole 8-float vectors
        for (; m + 8 <= nout; m += 8) {
#ifdef CHECKING
            for (int l = 0; l < 8; l++) {
                for (int r = 0; r < JAM_R; r++) {
                    BPM_ACC(c_al, comp, k0 + m + l, ar[r] * brow[r][m + l]);
                }
            }
#else
            __m256 vc = _mm256_loadu_ps(&c_al[k0 + m]);

#pragma GCC unroll 8
            for (int r = 0; r < JAM_R; r++) {
                vc = _mm256_fmadd_ps(va[r], _mm256_loadu_ps(&brow[r][m]), vc);
            }

            _mm256_storeu_ps(&c_al[k0 + m], vc);
#endif
        }

        // Leftover: fewer than 8 outputs, masked
        if (m < nout) {
#ifdef CHECKING
            for (int l = 0; l < nout - m; l++) {
                for (int r = 0; r < JAM_R; r++) {
                    BPM_ACC(c_al, comp, k0 + m + l, ar[r] * brow[r][m + l]);
                }
            }
#else
            __m256i mask = _mm256_cmpgt_epi32(_mm256_set1_epi32(nout - m),
                                              _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7));
            __m256 vc = _mm256_maskload_ps(&c_al[k0 + m], mask);

#pragma GCC unroll 8
            for (int r = 0; r < JAM_R; r++) {
                vc = _mm256_fmadd_ps(va[r], _mm256_loadu_ps(&brow[r][m]), vc);
            }

            _mm256_maskstore_ps(&c_al[k0 + m], mask, vc);
#endif
        }
    }
}

// Length of the first half of an n-element range, rounded to whole leaf
// blocks so every split point stays on a multiple of L (n > L, so both
// halves are non-empty).
static int half(int n, int L) {
    int nblk = (n + L - 1) / L;
    return (nblk / 2) * L;
}

// Split the longer range; split both when within a factor of 2. Fills the
// children's ranges and sets *sa / *sb to whether a / b was split.
static void split(int L, int i0, int na, int j0, int nb,
                  int alo[2], int alen[2], int blo[2], int blen[2],
                  int *sa, int *sb) {
    int split_a = na > L;
    int split_b = nb > L;
    if (split_a && split_b) {
        if (na > 2 * nb) {
            split_b = 0;
        } else if (nb > 2 * na) {
            split_a = 0;
        }
    }

    int ha = split_a ? half(na, L) : na;
    int hb = split_b ? half(nb, L) : nb;
    alo[0] = i0; alo[1] = i0 + ha; alen[0] = ha; alen[1] = na - ha;
    blo[0] = j0; blo[1] = j0 + hb; blen[0] = hb; blen[1] = nb - hb;
    *sa = split_a;
    *sb = split_b;
}

// Sequential recursion, as in DCQ: a[i0 .. i0+na) * b[j0 .. j0+nb), added
// into o.c at i0 + j0. i0 and j0 are always multiples of L, so a leaf's b
// range is exactly one padded block.
static void dc(const dc_ctx_t *restrict x, dc_out_t o, int i0, int na, int j0, int nb) {
    int L = x->leaf;
    if (na <= L && nb <= L) {
#ifdef TRACE
        double t0 = omp_get_wtime();
#endif
        leaf(x, o, i0, na, j0, nb);
#ifdef TRACE
        double t1 = omp_get_wtime();
        if (tr_leaves == 0) {
            tr_first = t0;
        }
        tr_last = t1;
        tr_secs += t1 - t0;
        tr_leaves++;
#endif
        return;
    }

    int alo[2], alen[2], blo[2], blen[2], split_a, split_b;
    split(L, i0, na, j0, nb, alo, alen, blo, blen, &split_a, &split_b);

    for (int p = 0; p <= split_a; p++) {
        for (int q = 0; q <= split_b; q++) {
            dc(x, o, alo[p], alen[p], blo[q], blen[q]);
        }
    }
}

#if SPAWN != 1 || ROOTDIRECT
static void dc_task(const dc_ctx_t *restrict x, int i0, int na, int j0, int nb);

// Runs one subproblem in the current task (or, with ROOTDIRECT, in the
// single block), writing through o: for TRED=1/2 the c (and comp) that the
// caller read in its own task region; TRED=3 ignores o and picks the
// executing thread's copy. Above the cutoff it creates its children
// (SPAWN=3: runs the last one itself) and, with SPAWN=2/3, waits for them.
// TRED=3 has no races between siblings, so that taskwait is not needed for
// correctness: it is there so the creating thread runs its own child tasks
// while it waits, instead of leaving them all to the team.
static void dc_node(const dc_ctx_t *restrict x, dc_out_t o, int i0, int na, int j0, int nb) {
    TR_ROOT(x, i0, na, j0, nb);
    if (na > x->cut || nb > x->cut) {
        int alo[2], alen[2], blo[2], blen[2], split_a, split_b;
        split(x->leaf, i0, na, j0, nb, alo, alen, blo, blen, &split_a, &split_b);

        for (int p = 0; p <= split_a; p++) {
            for (int q = 0; q <= split_b; q++) {
#if SPAWN == 3
                if (p == split_a && q == split_b) {
                    continue;   // the last child, run below
                }
#endif
                dc_task(x, alo[p], alen[p], blo[q], blen[q]);
            }
        }
#if SPAWN == 3
        dc_node(x, o, alo[split_a], alen[split_a], blo[split_b], blen[split_b]);
#endif

#if SPAWN != 1
#pragma omp taskwait
#endif
    } else {
#if TRED == 3
        // The executing thread's own copy, read now: the sequential subtree
        // has no task scheduling points.
        size_t off = (size_t)DC_TID() * x->stride;
        o.c = x->copies + off;
#ifdef CHECKING
        o.comp = x->ccopies + off;
#endif
#endif
        dc(x, o, i0, na, j0, nb);
    }
}

// One task for this subproblem, participating in the reduction (TRED=1/2).
static void dc_task(const dc_ctx_t *restrict x, int i0, int na, int j0, int nb) {
#if TRED != 3
    // The shared originals, named here only so the clause can find the
    // reduction; inside the task these names refer to the executing
    // thread's private copies.
    float *c = x->c;
#ifdef CHECKING
    double *comp = x->comp;
#endif
#endif

#pragma omp task TASK_RED
    {
#if TRED == 3
        dc_out_t o = {0};   // unused: TRED=3 picks the copy by thread
#else
        // Read c (and comp) inside the task: the private copy.
        dc_out_t o = {
            .c = c,
#ifdef CHECKING
            .comp = comp,
#endif
        };
#endif
        dc_node(x, o, i0, na, j0, nb);
    }
}

#else
// Creates one task for this subproblem, participating in the enclosing
// taskgroup's reduction. Above the cutoff the task only creates a task per
// child; at or below it, the task runs the sequential recursion on the
// executing thread's private copy.
static void dc_task(const dc_ctx_t *restrict x, int i0, int na, int j0, int nb) {
#if TRED != 3
    // The shared originals, named here only so the clause can find the
    // taskgroup's reduction; inside the task these names refer to the
    // executing thread's private copies.
    float *c = x->c;
#ifdef CHECKING
    double *comp = x->comp;
#endif
#endif

#pragma omp task TASK_RED
    {
        TR_ROOT(x, i0, na, j0, nb);
        if (na > x->cut || nb > x->cut) {
            int alo[2], alen[2], blo[2], blen[2], split_a, split_b;
            split(x->leaf, i0, na, j0, nb, alo, alen, blo, blen, &split_a, &split_b);

            for (int p = 0; p <= split_a; p++) {
                for (int q = 0; q <= split_b; q++) {
                    dc_task(x, alo[p], alen[p], blo[q], blen[q]);
                }
            }
        } else {
#if TRED == 3
            // The executing thread's own copy, read now: the sequential
            // subtree has no task scheduling points, so the thread cannot
            // change before it returns.
            size_t off = (size_t)DC_TID() * x->stride;
            dc_out_t o = {
                .c = x->copies + off,
#ifdef CHECKING
                .comp = x->ccopies + off,
#endif
            };
#else
            // Read c (and comp) inside the task: the private copy.
            dc_out_t o = {
                .c = c,
#ifdef CHECKING
                .comp = comp,
#endif
            };
#endif
            dc(x, o, i0, na, j0, nb);
        }
    }
}
#endif

void kernel_multiply(const float *restrict a, int dA,
                     const float *restrict b, int dB,
                     float *restrict c,
                     const bpm_tune_t *restrict tune) {
    int L = (tune->leaf_size > 0) ? tune->leaf_size : LEAF_DEFAULT;
    int cutoff = (tune->task_threshold > 0) ? tune->task_threshold : CUTOFF_DEFAULT;
    int out_len = dA + dB + 1;
#ifdef TRACE
    double tr_t_entry = omp_get_wtime();
#endif

    // A partial row group writes c[k] += 0 past its own range; with
    // L % JAM_R == 0 only the last block of a can have one.
    if (L % JAM_R != 0) {
        fprintf(stderr, "DCP1: --leaf-size %d rejected: must be a multiple of %d\n", L, JAM_R);
        exit(1);
    }

    // TRED=3's merge assigns every c[k], so c is not zeroed there.
#if TRED != 3
    for (int k = 0; k < out_len; k++) {
        c[k] = 0.0f;
    }
#endif

    BPM_COMP_DECL(comp, out_len);

    // One shifted copy: PAD + L + PAD of padded block, + up to 3 shift, + slack
    // so a full 8-float load past the last output only ever reads zeros.
    // Rounded to 16 floats so every copy starts on a 64-byte boundary.
    int clen = ((L + 2 * PAD + NCOPY + 16) + 15) & ~15;
    int nblk = (dB + L) / L;   // ceil((dB + 1) / L)
    size_t bytes = (size_t)nblk * NCOPY * clen * sizeof(float);

    // Built once: NCOPY copies of every zero-padded L-block of b.
    // Padded block P[x] = b[j0 + x - PAD] for PAD <= x < PAD + len, else 0.
    // Copy s holds P shifted right by s floats: copy_s[y] = P[y - s].
    float *bcopy = (float *)aligned_alloc(64, bytes);
    memset(bcopy, 0, bytes);
    for (int blk = 0; blk < nblk; blk++) {
        int j0 = blk * L;
        int len = (j0 + L <= dB + 1) ? L : (dB + 1 - j0);
        for (int s = 0; s < NCOPY; s++) {
            float *cs = bcopy + ((size_t)blk * NCOPY + s) * clen;
            for (int x = 0; x < len; x++) {
                cs[PAD + s + x] = b[j0 + x];
            }
        }
    }

    dc_ctx_t ctx = {
        .a = a, .c = c, .out_len = out_len,
        .leaf = L, .cut = (long)cutoff * L, .clen = clen, .bcopy = bcopy,
#ifdef CHECKING
        .comp = comp,
#endif
    };

#if TRED == 3
    // nthr private copies of c, allocated per call. Each copy's stride is a
    // multiple of 16 floats, so every copy starts on a 64-byte boundary.
    int nthr = DC_MAXTHR();
    size_t stride = ((size_t)out_len + 15) & ~(size_t)15;
    float *copies = (float *)aligned_alloc(64, (size_t)nthr * stride * sizeof(float));
    if (copies == NULL) {
        fprintf(stderr, "DCP1: allocating %d private copies of c failed\n", nthr);
        exit(1);
    }
    ctx.copies = copies;
    ctx.stride = stride;
#ifdef CHECKING
    double *ccopies = (double *)aligned_alloc(64, (size_t)nthr * stride * sizeof(double));
    if (ccopies == NULL) {
        fprintf(stderr, "DCP1: allocating %d private copies of comp failed\n", nthr);
        exit(1);
    }
    ctx.ccopies = ccopies;
#endif

    // The merge works on whole cache lines of c: slice 0 runs up to c's first
    // 64-byte boundary (lead floats), every later slice is 16 floats.
    int lead = (int)(((64 - ((uintptr_t)c & 63)) & 63) / sizeof(float));
    if (lead > out_len) {
        lead = out_len;
    }
    int nslice = 1 + (out_len - lead + 15) / 16;
#endif

#ifdef TRACE
    double tr_t_par = omp_get_wtime();
#endif

#pragma omp parallel PAR_RED
    {
#ifdef TRACE
        double tr_t_arrive = omp_get_wtime();
        int tid = omp_get_thread_num();
        tr_leaves = 0;
        tr_secs = 0.0;
        if (tid < TR_MAX) {
            tr_arrive[tid] = tr_t_arrive;
            tr_cpu[tid] = sched_getcpu();
        }
#endif

#if TRED == 3
        // Each thread zeroes its own copy, so its pages are first touched
        // on that thread's core.
        {
            assert(DC_NTHREADS() == nthr);
            size_t off = (size_t)DC_TID() * stride;
            assert(((uintptr_t)(copies + off) & 15) == 0);
            memset(copies + off, 0, stride * sizeof(float));
#ifdef CHECKING
            memset(ccopies + off, 0, stride * sizeof(double));
#endif
        }
#pragma omp barrier
#ifdef TRACE
        if (tid == 0) {
            tr_bar = omp_get_wtime();
        }
#endif
#endif

#pragma omp single
        {
#ifdef TRACE
            tr_single = tid;
            tr_nthr = omp_get_num_threads();
#endif

#if !NOTG
#pragma omp taskgroup TG_RED
#endif
#if ROOTDIRECT
            {
                // c (and comp) here: the original for TRED=1, this thread's
                // implicit-task copy for TRED=2, unused for TRED=3.
                dc_out_t o = {
                    .c = c,
#ifdef CHECKING
                    .comp = comp,
#endif
                };
                dc_node(&ctx, o, 0, dA + 1, 0, dB + 1);
            }
#else
            dc_task(&ctx, 0, dA + 1, 0, dB + 1);
#endif

#ifdef TRACE
            tr_tg_end = omp_get_wtime();
#endif
        }

        // single's barrier: every task has finished before this point.
#ifdef TRACE
        if (tid < TR_MAX) {
            tr_lv[tid] = tr_leaves;
            tr_ms[tid] = tr_secs * 1e3;
            tr_first_all[tid] = tr_first;
            tr_last_all[tid] = tr_last;
        }
#endif

#if TRED == 3
#ifdef TRACE
        if (tid == 0) {
            tr_sbar = omp_get_wtime();
        }
#endif

        // Merge: c[k] = sum over t of copy_t[k]. The checking build keeps the
        // Kahan invariant (precise value ~= c - comp): accumulate.h defines
        // no combine, so this rule is derived from that invariant --
        //   total = sum_t (copy_t[k] - comp_t[k])   (in double)
        //   c[k] = (float)total,  comp[k] = c[k] - total.
#pragma omp for schedule(static)
        for (int s = 0; s < nslice; s++) {
            int k_lo = (s == 0) ? 0 : lead + 16 * (s - 1);
            int k_hi = lead + 16 * s;
            if (k_hi > out_len) {
                k_hi = out_len;
            }
            for (int k = k_lo; k < k_hi; k++) {
#ifdef CHECKING
                double total = 0.0;
                for (int t = 0; t < nthr; t++) {
                    total += (double)copies[(size_t)t * stride + k] - ccopies[(size_t)t * stride + k];
                }
                c[k] = (float)total;
                comp[k] = (double)c[k] - total;
#else
                float sum = 0.0f;
                for (int t = 0; t < nthr; t++) {
                    sum += copies[(size_t)t * stride + k];
                }
                c[k] = sum;
#endif
            }
        }

#ifdef TRACE
        if (tid == 0) {
            tr_merge_end = omp_get_wtime();
        }
#endif
#endif
    }

#ifdef TRACE
    double tr_t_reg_end = omp_get_wtime();
#endif

#if TRED == 3
    free(copies);
#ifdef CHECKING
    free(ccopies);
#endif
#endif
    free(bcopy);
    BPM_COMP_FREE(comp);

#ifdef TRACE
    double tr_t_exit = omp_get_wtime();
    double first = 0.0, last = 0.0;
    int seen = 0;
    int n = (tr_nthr < TR_MAX) ? tr_nthr : TR_MAX;
    double arrived = tr_arrive[0];
    for (int t = 1; t < n; t++) {
        if (tr_arrive[t] > arrived) arrived = tr_arrive[t];
    }

    fprintf(stderr, "TRACE call nthreads=%d single_tid=%d tred=%d root_tid=%d\n", tr_nthr, tr_single, TRED, tr_root);
    for (int t = 0; t < n; t++) {
        fprintf(stderr, "TRACE thr tid=%d cpu=%d leaves=%ld leaf_ms=%.3f single=%d\n",
                t, tr_cpu[t], tr_lv[t], tr_ms[t], t == tr_single);
        if (tr_lv[t] > 0) {
            if (!seen || tr_first_all[t] < first) first = tr_first_all[t];
            if (!seen || tr_last_all[t] > last) last = tr_last_all[t];
            seen = 1;
        }
    }
#if TRED == 3
    fprintf(stderr, "TRACE phase3 setup_ms=%.3f start_to_all_arrived_ms=%.3f "
                    "zeroing_ms=%.3f barrier_to_first_leaf_ms=%.3f leaf_span_ms=%.3f "
                    "last_leaf_to_single_barrier_ms=%.3f merge_ms=%.3f "
                    "merge_end_to_region_end_ms=%.3f free_ms=%.3f total_ms=%.3f\n",
            (tr_t_par - tr_t_entry) * 1e3, (arrived - tr_t_par) * 1e3,
            (tr_bar - arrived) * 1e3, (first - tr_bar) * 1e3, (last - first) * 1e3,
            (tr_sbar - last) * 1e3, (tr_merge_end - tr_sbar) * 1e3,
            (tr_t_reg_end - tr_merge_end) * 1e3, (tr_t_exit - tr_t_reg_end) * 1e3,
            (tr_t_exit - tr_t_entry) * 1e3);
#else
    fprintf(stderr, "TRACE phase setup_ms=%.3f start_to_first_leaf_ms=%.3f "
                    "start_to_all_arrived_ms=%.3f all_arrived_to_first_leaf_ms=%.3f "
                    "leaf_span_ms=%.3f last_leaf_to_tg_end_ms=%.3f "
                    "tg_end_to_region_end_ms=%.3f free_ms=%.3f total_ms=%.3f\n",
            (tr_t_par - tr_t_entry) * 1e3, (first - tr_t_par) * 1e3,
            (arrived - tr_t_par) * 1e3, (first - arrived) * 1e3,
            (last - first) * 1e3, (tr_tg_end - last) * 1e3,
            (tr_t_reg_end - tr_tg_end) * 1e3, (tr_t_exit - tr_t_reg_end) * 1e3,
            (tr_t_exit - tr_t_entry) * 1e3);
#endif
#endif
}
