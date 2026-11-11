#ifdef TRACE
#define _GNU_SOURCE           // sched_getcpu
#endif

#include <immintrin.h>
#include <stdio.h>
#include <string.h>
#include "kernel.h"
#include "accumulate.h"

// DCP2: DCQ parallelised with taskwait only. There is no reduction of any
// kind: every task writes the shared c (and comp), and races are avoided by
// ordering the children of each split.
//
// TRACE build: per thread, its CPU at region start, the leaves it ran, the
// time spent in them, whether it ran the single block, and the time it spent
// inside taskwait. Output goes to stderr, so the csv on stdout is unchanged.
#ifdef TRACE
#ifndef _OPENMP
#error "TRACE needs an OpenMP build"
#endif
#include <omp.h>
#include <sched.h>

#define TR_MAX 256

static long tr_leaves;
static double tr_secs;
static int tr_tw_depth;       // taskwaits currently open on this thread
static long tr_tw_count;      // taskwaits entered (all depths)
static double tr_tw_enter;    // when the outermost open taskwait was entered
static double tr_tw_secs;     // time with at least one taskwait open
static double tr_tw_leaf;     // leaf time run while a taskwait was open
#pragma omp threadprivate(tr_leaves, tr_secs, tr_tw_depth, tr_tw_count, \
                          tr_tw_enter, tr_tw_secs, tr_tw_leaf)

static int tr_cpu[TR_MAX];
static long tr_lv[TR_MAX], tr_twn[TR_MAX];
static double tr_ms[TR_MAX], tr_twms[TR_MAX], tr_twleafms[TR_MAX];
static int tr_single, tr_nthr;

// A thread inside a taskwait may run other tasks that reach their own
// taskwait, so waits nest on one thread. Only the outermost one is timed:
// tw_ms is wall time with at least one taskwait open, never counted twice.
// Tasks are tied, so a taskwait is always left on the thread that entered it.
static void tr_tw_begin(void) {
    tr_tw_count++;
    if (tr_tw_depth++ == 0) {
        tr_tw_enter = omp_get_wtime();
    }
}

static void tr_tw_end(void) {
    if (--tr_tw_depth == 0) {
        tr_tw_secs += omp_get_wtime() - tr_tw_enter;
    }
}

#define TASKWAIT()                  \
    do {                            \
        tr_tw_begin();              \
        _Pragma("omp taskwait")     \
        tr_tw_end();                \
    } while (0)
#else
#define TASKWAIT() _Pragma("omp taskwait")
#endif

// Leaf size when --leaf-size is not given: BKQ's best single-core BS in PA3.
#define LEAF_DEFAULT 1280

// Task cutoff when --task-threshold is not given, in leaves per side: tasks
// go down to single leaves, so the code matches the span analysis.
#define CUTOFF_DEFAULT 1

#define JAM_R  8              // rows of a fused per pass (unroll-and-jam)
#define IL_U   3              // independent 8-float vectors per pass
#define NCOPY  4              // shifted copies of each b block (shifts 0..3)
#define PAD    (JAM_R - 1)    // zeros on each side of a b block

#define STR_(x) #x
#define STR(x)  STR_(x)

// Race controls (negative controls, separate binaries; must fail the gate):
//   RACE=1: no taskwait: LL and HH are tasks, then LH and HL run
//           immediately, while LL and HH may still be running.
//   RACE=2: the taskwait stays, but LH and HL run as two tasks together,
//           then a second taskwait.
#ifdef RACE
#if RACE != 1 && RACE != 2
#error "RACE must be 1 or 2"
#endif
#define BP_RACE ";race=" STR(RACE)
#else
#define BP_RACE ""
#endif

// HHDIRECT=1 (DCP2HD): in stage 1, only LL is a task; HH runs directly in
// the current task, then taskwait. Default 0 is DCP2 as first built.
// HHDIRECT=2 (DCP2HD2): as 1, but HH is an undeferred task (task if(0)):
// the current thread runs it at once, but its taskwaits wait only for
// HH's own children, not for the parent's LL.
// With HHDIRECT=1 or 2, RACE=1 drops the taskwait (LH can start while LL
// may still run); RACE=2 is not defined for them.
#ifndef HHDIRECT
#define HHDIRECT 0
#endif
#if HHDIRECT != 0 && HHDIRECT != 1 && HHDIRECT != 2
#error "HHDIRECT must be 0, 1 or 2"
#endif
#if HHDIRECT && defined(RACE) && RACE != 1
#error "HHDIRECT=1 and 2 have only the RACE=1 control"
#endif

// Only non-default switches are listed, so the default build keeps the
// string earlier DCP2 rows were recorded with.
#if HHDIRECT
#define BP_HHD ";hhdirect=" STR(HHDIRECT)
#else
#define BP_HHD ""
#endif

const char *kernel_build_params(void) {
    return "r=" STR(JAM_R) ";u=" STR(IL_U) BP_HHD BP_RACE;
}

// Everything the recursion carries unchanged from the top-level call.
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
} dc_ctx_t;

// BKQ's final kernel over rows [i0, i0 + na) of a and the b block at j0
// (len elements), accumulating into c at i0 + j0.
__attribute__((optimize("no-associative-math")))
static void leaf(const dc_ctx_t *restrict x, int i0, int na, int j0, int len) {
    const float *restrict a_al = (const float *)__builtin_assume_aligned(x->a, 16);
    float *restrict c_al = (float *)__builtin_assume_aligned(x->c, 16);
#ifdef CHECKING
    double *restrict comp = x->comp;
#endif
    int out_len = x->out_len;
    int ilast = i0 + na - 1;

    const float *bcopy = x->bcopy + (size_t)(j0 / x->leaf) * NCOPY * x->clen;

    // Row groups start at i0 + multiples of JAM_R, so the first output of
    // every group has k0 mod 4 == (i0 + j0) mod 4. h scalar outputs bring c
    // to a 16-byte boundary; vector passes start at m = h.
    int h = (4 - ((i0 + j0) & 3)) & 3;

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
        // With a shared c, this leaf must write only its own outputs, up to
        // ilast + j0 + len - 1. A last group with zero-padded rows past
        // ilast would otherwise add 0 to c[k] above that, which another
        // task may own. Full groups (i + JAM_R - 1 <= ilast) are unchanged.
        if (nout > ilast - i + len) {
            nout = ilast - i + len;
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
// into c at i0 + j0. i0 and j0 are always multiples of L, so a leaf's b
// range is exactly one padded block.
static void dc(const dc_ctx_t *restrict x, int i0, int na, int j0, int nb) {
    int L = x->leaf;
    if (na <= L && nb <= L) {
#ifdef TRACE
        double t0 = omp_get_wtime();
#endif
        leaf(x, i0, na, j0, nb);
#ifdef TRACE
        double dt = omp_get_wtime() - t0;
        tr_secs += dt;
        if (tr_tw_depth > 0) {
            tr_tw_leaf += dt;
        }
        tr_leaves++;
#endif
        return;
    }

    int alo[2], alen[2], blo[2], blen[2], split_a, split_b;
    split(L, i0, na, j0, nb, alo, alen, blo, blen, &split_a, &split_b);

    for (int p = 0; p <= split_a; p++) {
        for (int q = 0; q <= split_b; q++) {
            dc(x, alo[p], alen[p], blo[q], blen[q]);
        }
    }
}

// Parallel recursion. Above the cutoff, a node that splits both a and b
// runs its children in three stages:
//   stage 1: LL and HH as tasks, then taskwait (disjoint outputs);
//   stage 2: LH, called directly;
//   stage 3: HL, called directly, after LH returns.
// LH and HL write the same outputs, and each overlaps LL and HH, so they
// run alone, one after the other; each is still parallel inside.
static void dc_par(const dc_ctx_t *restrict x, int i0, int na, int j0, int nb) {
    if (na <= x->cut && nb <= x->cut) {
        dc(x, i0, na, j0, nb);   // at or below the cutoff: sequential DCQ
        return;
    }

    int alo[2], alen[2], blo[2], blen[2], split_a, split_b;
    split(x->leaf, i0, na, j0, nb, alo, alen, blo, blen, &split_a, &split_b);

    if (!(split_a && split_b)) {
        // Only one range is split: the two children's outputs overlap
        // whenever the other range has more than one element, so they run
        // in sequence, directly, with no tasks. This makes lopsided shapes
        // sequential at this level; that is out of scope for this experiment.
        for (int p = 0; p <= split_a; p++) {
            for (int q = 0; q <= split_b; q++) {
                dc_par(x, alo[p], alen[p], blo[q], blen[q]);
            }
        }
        return;
    }

    int aL = alo[0], naL = alen[0], aH = alo[1], naH = alen[1];
    int bL = blo[0], nbL = blen[0], bH = blo[1], nbH = blen[1];

    // Stage 1: LL and HH write disjoint outputs.
#pragma omp task
    dc_par(x, aL, naL, bL, nbL);
#if HHDIRECT == 1
    dc_par(x, aH, naH, bH, nbH);   // HH runs directly in the current task
#elif HHDIRECT == 2
#pragma omp task if(0)
    dc_par(x, aH, naH, bH, nbH);   // HH runs at once, as its own task
#else
#pragma omp task
    dc_par(x, aH, naH, bH, nbH);
#endif

#if defined(RACE) && RACE == 1
    // RACE=1: no taskwait; LH and HL start while LL (and, without
    // HHDIRECT, HH) may still run.
    dc_par(x, aL, naL, bH, nbH);
    dc_par(x, aH, naH, bL, nbL);
#elif defined(RACE) && RACE == 2
    TASKWAIT();
    // RACE=2: LH and HL run together, writing the same outputs.
#pragma omp task
    dc_par(x, aL, naL, bH, nbH);
#pragma omp task
    dc_par(x, aH, naH, bL, nbL);
    TASKWAIT();
#else
    TASKWAIT();
    // Stage 2, then stage 3.
    dc_par(x, aL, naL, bH, nbH);
    dc_par(x, aH, naH, bL, nbL);
#endif
}

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
    // L % JAM_R == 0 only the last block of a can have one, and the leaf
    // clamps that group's outputs.
    if (L % JAM_R != 0) {
        fprintf(stderr, "DCP2: --leaf-size %d rejected: must be a multiple of %d\n", L, JAM_R);
        exit(1);
    }

    for (int k = 0; k < out_len; k++) {
        c[k] = 0.0f;
    }

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

#ifdef TRACE
    double tr_t_par = omp_get_wtime();
#endif

    // The single thread runs the root directly (the root is not a task).
    // No taskgroup: the single's implicit barrier waits for any tasks left.
#pragma omp parallel
    {
#ifdef TRACE
        int tid = omp_get_thread_num();
        tr_leaves = 0;
        tr_secs = 0.0;
        tr_tw_depth = 0;
        tr_tw_count = 0;
        tr_tw_secs = 0.0;
        tr_tw_leaf = 0.0;
        if (tid < TR_MAX) {
            tr_cpu[tid] = sched_getcpu();
        }
#endif

#pragma omp single
        {
#ifdef TRACE
            tr_single = tid;
            tr_nthr = omp_get_num_threads();
#endif
            dc_par(&ctx, 0, dA + 1, 0, dB + 1);
        }

        // single's barrier: every task has finished before this point.
#ifdef TRACE
        if (tid < TR_MAX) {
            tr_lv[tid] = tr_leaves;
            tr_ms[tid] = tr_secs * 1e3;
            tr_twn[tid] = tr_tw_count;
            tr_twms[tid] = tr_tw_secs * 1e3;
            tr_twleafms[tid] = tr_tw_leaf * 1e3;
        }
#endif
    }

#ifdef TRACE
    double tr_t_reg_end = omp_get_wtime();
#endif

    free(bcopy);
    BPM_COMP_FREE(comp);

#ifdef TRACE
    double tr_t_exit = omp_get_wtime();
    int n = (tr_nthr < TR_MAX) ? tr_nthr : TR_MAX;
    long total = 0;
    for (int t = 0; t < n; t++) {
        total += tr_lv[t];
    }
    long expected = (long)((dA + L) / L) * ((dB + L) / L);   // the L grid

    fprintf(stderr, "TRACE call nthreads=%d single_tid=%d cutoff=%d leaves_total=%ld leaves_expected=%ld\n",
            tr_nthr, tr_single, cutoff, total, expected);
    for (int t = 0; t < n; t++) {
        fprintf(stderr, "TRACE thr tid=%d cpu=%d leaves=%ld leaf_ms=%.3f single=%d "
                        "taskwaits=%ld taskwait_ms=%.3f leaf_ms_in_taskwait=%.3f\n",
                t, tr_cpu[t], tr_lv[t], tr_ms[t], t == tr_single,
                tr_twn[t], tr_twms[t], tr_twleafms[t]);
    }
    fprintf(stderr, "TRACE phase setup_ms=%.3f region_ms=%.3f free_ms=%.3f total_ms=%.3f\n",
            (tr_t_par - tr_t_entry) * 1e3, (tr_t_reg_end - tr_t_par) * 1e3,
            (tr_t_exit - tr_t_reg_end) * 1e3, (tr_t_exit - tr_t_entry) * 1e3);
#endif
}
