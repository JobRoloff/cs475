#ifdef TRACE
#define _GNU_SOURCE           // sched_getcpu
#endif

#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "kernel.h"
#include "accumulate.h"

// DCP3: output-range recursion. Each task owns a contiguous range of outputs
// c[k0 .. k1) and computes every pair (i, j) with k0 <= i + j < k1. No two
// tasks write the same element of c (or comp), so there is no reduction, no
// taskwait and no taskgroup.
//
// TRACE build: per thread, its CPU at region start, the pieces it ran, the
// time spent in them, and whether it ran the single block; per call, setup
// time, region start to the last piece finishing, the piece count, the
// smallest and largest piece in pairs, and the pairs summed over all pieces
// against na * nb. Output goes to stderr, so the csv on stdout is unchanged.
#ifdef TRACE
#ifndef _OPENMP
#error "TRACE needs an OpenMP build"
#endif
#include <omp.h>
#include <sched.h>

#define TR_MAX 256

static long tr_pieces;
static double tr_secs;
static double tr_last_end;    // when this thread's last piece finished
static int64_t tr_pairs;      // pairs in the pieces this thread ran
static int64_t tr_pmin, tr_pmax;   // smallest / largest piece it ran, in pairs
#pragma omp threadprivate(tr_pieces, tr_secs, tr_last_end, tr_pairs, tr_pmin, tr_pmax)

static int tr_cpu[TR_MAX];
static long tr_pc[TR_MAX];
static double tr_ms[TR_MAX], tr_end[TR_MAX];
static int64_t tr_pr[TR_MAX], tr_mn[TR_MAX], tr_mx[TR_MAX];
static int tr_single, tr_nthr;
#endif

// Output chunk W when --leaf-size is not given: BKQ's best single-core BS in
// PA3. A W at least as wide as the widest piece turns chunking off.
#define LEAF_DEFAULT 1280

// Piece count when --task-threshold is not given; must be a power of two
// (the recursion splits log2(pieces) times). -DPIECES_DEFAULT=N overrides it
// for the checking builds, which take no --task-threshold.
#ifndef PIECES_DEFAULT
#define PIECES_DEFAULT 128
#endif

#define JAM_R  8              // rows of a fused per pass (unroll-and-jam)
#define IL_U   3              // independent 8-float vectors per pass
#define NCOPY  4              // shifted copies of b (shifts 0..3)
#define PAD    (JAM_R - 1)    // zeros before b in every copy

#define STR_(x) #x
#define STR(x)  STR_(x)

// Race control (negative control, separate binary; must pass the gate at
// 1 thread and fail it at 16):
//   RACE=1: each piece's rows are split in two halves, run as two tasks
//           that write the same outputs.
#ifdef RACE
#if RACE != 1
#error "RACE must be 1"
#endif
#define BP_RACE ";race=" STR(RACE)
#else
#define BP_RACE ""
#endif

const char *kernel_build_params(void) {
    return "r=" STR(JAM_R) ";u=" STR(IL_U) BP_RACE;
}

// Everything the recursion carries unchanged from the top-level call.
typedef struct {
    const float *a;
    float *c;
    int na, nb;           // coefficients of a and b
    int out_len;          // na + nb - 1
    int chunk;            // output chunk W within a piece
    int clen;            // floats per shifted copy (multiple of 16)
    const float *bcopy;   // NCOPY shifted, padded copies of all of b
#ifdef CHECKING
    double *comp;
#endif
} dc_ctx_t;

// Number of pairs (i, j), 0 <= i < na, 0 <= j < nb, with i + j < k.
// Row i contributes min(nb, k - i) pairs for i < min(na, k): rows below
// t = clamp(k - nb + 1, 0, s) contribute nb each, rows t .. s-1 contribute
// k - i each.
static int64_t pairs_below(const dc_ctx_t *x, int k) {
    int64_t na = x->na, nb = x->nb, kk = k;
    int64_t s = (kk < na) ? kk : na;
    if (s <= 0) {
        return 0;
    }
    int64_t t = kk - nb + 1;
    if (t < 0) {
        t = 0;
    }
    if (t > s) {
        t = s;
    }
    // sum_{i=t}^{s-1} (k - i) = (s - t) * k - (s - t) * (s - 1 + t) / 2
    return t * nb + (s - t) * kk - (s - t) * (s - 1 + t) / 2;
}

// Number of pairs with k0 <= i + j < k1.
static int64_t pairs(const dc_ctx_t *x, int k0, int k1) {
    return pairs_below(x, k1) - pairs_below(x, k0);
}

// All pairs with output k in [k0, k1) and row i in [rlo, rhi), added into
// c[k0 .. k1) only: the neighbouring pieces own every other output. The
// piece is split into chunks of W outputs [kc0, kc1); for each chunk, the
// rows that contribute are [max(rlo, kc0 - nb + 1), min(rhi, kc1)), in row
// groups of JAM_R. A group computes only outputs in
// [max(kc0, i), min(kc1, i_last + nb)), where it has at least one valid pair;
// the padded copies supply zeros for the rest. A W at least as wide as the
// piece gives one chunk (chunking off).
__attribute__((optimize("no-associative-math")))
static void piece(const dc_ctx_t *restrict x, int k0, int k1, int rlo, int rhi) {
    const float *restrict a_al = (const float *)__builtin_assume_aligned(x->a, 16);
    float *restrict c_al = (float *)__builtin_assume_aligned(x->c, 16);
#ifdef CHECKING
    double *restrict comp = x->comp;
#endif
    int nb = x->nb;
    int W = x->chunk;

    for (int kc0 = k0, kc1; kc0 < k1; kc0 = kc1) {
        kc1 = (k1 - kc0 > W) ? kc0 + W : k1;

        int ilo = kc0 - nb + 1;
        if (ilo < rlo) {
            ilo = rlo;
        }
        int ihi = (kc1 < rhi) ? kc1 : rhi;

        // For a group at row i, m = k - i: row r reads P[m + PAD - r], where
        // P[y] = b[y - PAD] (zero outside b). Groups start at ilo + multiples
        // of JAM_R, so every group's vector passes start at m == -ilo (mod 4)
        // (c aligned to 16 bytes there). Pick the copy that puts each row's
        // window on a 16-byte boundary at those m; rows r and r+4 share it.
        int hh = (-ilo) & 3;
        const float *brow[JAM_R];
        for (int r = 0; r < JAM_R; r++) {
            int s = (r - PAD - hh) & 3;
            brow[r] = x->bcopy + s * x->clen + PAD - r + s;
        }

        for (int i = ilo; i < ihi; i += JAM_R) {
            // Rows at or past ihi contribute nothing to this chunk, belong to
            // another row range, or are past the end of a: zero them.
            float ar[JAM_R];
            for (int r = 0; r < JAM_R; r++) {
                ar[r] = (i + r < ihi) ? a_al[i + r] : 0.0f;
            }
            int ilast = (i + JAM_R - 1 < ihi) ? i + JAM_R - 1 : ihi - 1;

            // This group's outputs, clipped to the chunk.
            int kstart = (kc0 > i) ? kc0 : i;
            int kend = (kc1 < ilast + nb) ? kc1 : ilast + nb;
            if (kstart >= kend) {
                continue;
            }
            int m = kstart - i;
            int mhi = kend - i;

            // Head: up to 3 scalar outputs to align c to 16 bytes
            int h = (4 - (kstart & 3)) & 3;
            int mh = (m + h < mhi) ? m + h : mhi;
            for (; m < mh; m++) {
                for (int r = 0; r < JAM_R; r++) {
                    BPM_ACC(c_al, comp, i + m, ar[r] * brow[r][m]);
                }
            }

#ifndef CHECKING
            __m256 va[JAM_R];
            for (int r = 0; r < JAM_R; r++) {
                va[r] = _mm256_set1_ps(ar[r]);
            }
#endif

            // Body: IL_U interleaved vectors (24 outputs) per pass
            for (; m + IL_U * 8 <= mhi; m += IL_U * 8) {
#ifdef CHECKING
                for (int l = 0; l < IL_U * 8; l++) {
                    for (int r = 0; r < JAM_R; r++) {
                        BPM_ACC(c_al, comp, i + m + l, ar[r] * brow[r][m + l]);
                    }
                }
#else
                __m256 vc0 = _mm256_loadu_ps(&c_al[i + m]);
                __m256 vc1 = _mm256_loadu_ps(&c_al[i + m + 8]);
                __m256 vc2 = _mm256_loadu_ps(&c_al[i + m + 16]);

#pragma GCC unroll 8
                for (int r = 0; r < JAM_R; r++) {
                    vc0 = _mm256_fmadd_ps(va[r], _mm256_loadu_ps(&brow[r][m]), vc0);
                    vc1 = _mm256_fmadd_ps(va[r], _mm256_loadu_ps(&brow[r][m + 8]), vc1);
                    vc2 = _mm256_fmadd_ps(va[r], _mm256_loadu_ps(&brow[r][m + 16]), vc2);
                }

                _mm256_storeu_ps(&c_al[i + m], vc0);
                _mm256_storeu_ps(&c_al[i + m + 8], vc1);
                _mm256_storeu_ps(&c_al[i + m + 16], vc2);
#endif
            }

            // Cleanup: remaining whole 8-float vectors
            for (; m + 8 <= mhi; m += 8) {
#ifdef CHECKING
                for (int l = 0; l < 8; l++) {
                    for (int r = 0; r < JAM_R; r++) {
                        BPM_ACC(c_al, comp, i + m + l, ar[r] * brow[r][m + l]);
                    }
                }
#else
                __m256 vc = _mm256_loadu_ps(&c_al[i + m]);

#pragma GCC unroll 8
                for (int r = 0; r < JAM_R; r++) {
                    vc = _mm256_fmadd_ps(va[r], _mm256_loadu_ps(&brow[r][m]), vc);
                }

                _mm256_storeu_ps(&c_al[i + m], vc);
#endif
            }

            // Leftover: fewer than 8 outputs, masked
            if (m < mhi) {
#ifdef CHECKING
                for (int l = 0; l < mhi - m; l++) {
                    for (int r = 0; r < JAM_R; r++) {
                        BPM_ACC(c_al, comp, i + m + l, ar[r] * brow[r][m + l]);
                    }
                }
#else
                __m256i mask = _mm256_cmpgt_epi32(_mm256_set1_epi32(mhi - m),
                                                  _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7));
                __m256 vc = _mm256_maskload_ps(&c_al[i + m], mask);

#pragma GCC unroll 8
                for (int r = 0; r < JAM_R; r++) {
                    vc = _mm256_fmadd_ps(va[r], _mm256_loadu_ps(&brow[r][m]), vc);
                }

                _mm256_maskstore_ps(&c_al[i + m], mask, vc);
#endif
            }
        }
    }
}

#if defined(RACE) && defined(TRACE)
#error "RACE and TRACE are separate builds"
#endif

// Run piece(k0, k1) over all rows, timed and counted in the TRACE build.
static void run_piece(const dc_ctx_t *restrict x, int k0, int k1) {
#if defined(RACE)
    // RACE=1: split the piece's rows at a midpoint 8-aligned from the first
    // contributing row and run the halves as two tasks. Every pair is still
    // computed once, but both halves write the same outputs.
    int rlo = (k0 - x->nb + 1 > 0) ? k0 - x->nb + 1 : 0;
    int rhi = (k1 < x->na) ? k1 : x->na;
    int rmid = rlo + (((rhi - rlo) / 2) & ~(JAM_R - 1));
#pragma omp task
    piece(x, k0, k1, rlo, rmid);
#pragma omp task
    piece(x, k0, k1, rmid, rhi);
#else
#ifdef TRACE
    double t0 = omp_get_wtime();
#endif
    piece(x, k0, k1, 0, x->na);
#ifdef TRACE
    double t1 = omp_get_wtime();
    int64_t p = pairs(x, k0, k1);
    tr_secs += t1 - t0;
    tr_last_end = t1;
    tr_pieces++;
    tr_pairs += p;
    if (p < tr_pmin) {
        tr_pmin = p;
    }
    if (p > tr_pmax) {
        tr_pmax = p;
    }
#endif
#endif
}

// Owns outputs c[k0 .. k1). Splits by work, depth times, so there are
// exactly 2^depth pieces: m is the smallest k with
// pairs(k0, k) >= pairs(k0, k1) / 2, rounded to the nearest output whose
// address starts a 64-byte cache line of c, so both halves have about equal
// pairs (pieces within about 2.3% at 128 pieces).
// A half may be empty when the range is too small to split (a piece with no
// outputs does nothing). Both halves are tasks; nothing waits for them here
// (the parallel region's implicit barrier does).
static void rec(const dc_ctx_t *restrict x, int k0, int k1, int depth) {
    if (depth == 0) {
        run_piece(x, k0, k1);
        return;
    }

    // Binary search on k in [k0, k1]: pairs(k0, k) is non-decreasing.
    int64_t base = pairs_below(x, k0);
    int64_t target = (pairs_below(x, k1) - base) / 2;
    int lo = k0, hi = k1;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (pairs_below(x, mid) - base >= target) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    int m = lo;

    // Round the split to the nearest m with &c[m] on a 64-byte boundary,
    // using c's actual address, so no two pieces share a cache line of c.
    // (The harness's malloc puts c 16 B past a line, so these m are
    // 12 mod 16; any such m is a multiple of 4, so pieces need no scalar
    // head. Rounding to multiples of 8 instead gives only 16-byte, not
    // 32-byte, alignment for such a c, and every boundary falls inside a
    // line.) Clamped, so the halves still partition [k0, k1).
    int off = (int)(((uintptr_t)x->c / sizeof(float)) & 15);   // floats past a line
    m = ((m + off + 8) & ~15) - off;
    if (m < k0) {
        m = k0;
    }
    if (m > k1) {
        m = k1;
    }

#pragma omp task
    rec(x, k0, m, depth - 1);
#pragma omp task
    rec(x, m, k1, depth - 1);
}

void kernel_multiply(const float *restrict a, int dA,
                     const float *restrict b, int dB,
                     float *restrict c,
                     const bpm_tune_t *restrict tune) {
    int W = (tune->leaf_size > 0) ? tune->leaf_size : LEAF_DEFAULT;
    int npieces = (tune->task_threshold > 0) ? tune->task_threshold : PIECES_DEFAULT;
    int na = dA + 1, nb = dB + 1;
    int out_len = dA + dB + 1;
#ifdef TRACE
    double tr_t_entry = omp_get_wtime();
#endif

    if (W % JAM_R != 0) {
        fprintf(stderr, "DCP3: --leaf-size %d rejected: the chunk width must be a multiple of %d\n",
                W, JAM_R);
        exit(1);
    }
    if (npieces & (npieces - 1)) {
        fprintf(stderr, "DCP3: --task-threshold %d rejected: the piece count must be a power of two\n",
                npieces);
        exit(1);
    }
    int depth = 0;
    while ((1 << depth) < npieces) {
        depth++;
    }

    for (int k = 0; k < out_len; k++) {
        c[k] = 0.0f;
    }

    BPM_COMP_DECL(comp, out_len);

    // One shifted copy of all of b: PAD zeros, nb floats of b, then zeros.
    // Copy s holds the padded b shifted right by s floats:
    // copy_s[y] = P[y - s], P[x] = b[x - PAD] for PAD <= x < PAD + nb, else 0.
    // A group reads copy indices PAD - r + s + m with 0 <= m <= nb + 13 (its
    // last output is at most m = nb + 6, and a masked leftover loads 8 floats
    // from there), so indices 0 .. nb + 23. The length nb + 2 * PAD + NCOPY +
    // 16 = nb + 34, rounded up to 16 floats, covers that: every copy has
    // 7 + s zeros before b and at least 24 after it.
    int clen = ((nb + 2 * PAD + NCOPY + 16) + 15) & ~15;
    size_t bytes = (size_t)NCOPY * clen * sizeof(float);

    // Built once, before the parallel region.
    float *bcopy = (float *)aligned_alloc(64, bytes);
    memset(bcopy, 0, bytes);
    for (int s = 0; s < NCOPY; s++) {
        float *cs = bcopy + (size_t)s * clen;
        for (int j = 0; j < nb; j++) {
            cs[PAD + s + j] = b[j];
        }
    }

    dc_ctx_t ctx = {
        .a = a, .c = c, .na = na, .nb = nb, .out_len = out_len,
        .chunk = W,
        .clen = clen, .bcopy = bcopy,
#ifdef CHECKING
        .comp = comp,
#endif
    };

#ifdef TRACE
    double tr_t_par = omp_get_wtime();
#endif

    // The single thread calls the root directly (the root is not a task).
    // No taskgroup, no taskwait: the single's implicit barrier waits for
    // every task.
#pragma omp parallel
    {
#ifdef TRACE
        int tid = omp_get_thread_num();
        tr_pieces = 0;
        tr_secs = 0.0;
        tr_last_end = 0.0;
        tr_pairs = 0;
        tr_pmin = INT64_MAX;
        tr_pmax = -1;
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
            rec(&ctx, 0, out_len, depth);
        }

        // single's barrier: every task has finished before this point.
#ifdef TRACE
        if (tid < TR_MAX) {
            tr_pc[tid] = tr_pieces;
            tr_ms[tid] = tr_secs * 1e3;
            tr_end[tid] = tr_last_end;
            tr_pr[tid] = tr_pairs;
            tr_mn[tid] = tr_pmin;
            tr_mx[tid] = tr_pmax;
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
    int64_t total_pairs = (int64_t)na * nb;
    long pieces_total = 0;
    int64_t pairs_total = 0, piece_min = INT64_MAX, piece_max = -1;
    double last_end = tr_t_par;
    for (int t = 0; t < n; t++) {
        pieces_total += tr_pc[t];
        pairs_total += tr_pr[t];
        if (tr_pc[t] > 0) {
            if (tr_end[t] > last_end) {
                last_end = tr_end[t];
            }
            if (tr_mn[t] < piece_min) {
                piece_min = tr_mn[t];
            }
            if (tr_mx[t] > piece_max) {
                piece_max = tr_mx[t];
            }
        }
    }

    fprintf(stderr, "TRACE call nthreads=%d single_tid=%d pieces_opt=%d chunk=%d pieces_total=%ld "
                    "piece_pairs_min=%lld piece_pairs_max=%lld "
                    "pairs_total=%lld pairs_expected=%lld pairs_ok=%d\n",
            tr_nthr, tr_single, npieces, W, pieces_total,
            (long long)piece_min, (long long)piece_max,
            (long long)pairs_total, (long long)total_pairs, pairs_total == total_pairs);
    for (int t = 0; t < n; t++) {
        fprintf(stderr, "TRACE thr tid=%d cpu=%d pieces=%ld piece_ms=%.3f single=%d\n",
                t, tr_cpu[t], tr_pc[t], tr_ms[t], t == tr_single);
    }
    fprintf(stderr, "TRACE phase setup_ms=%.3f start_to_last_piece_ms=%.3f region_ms=%.3f "
                    "free_ms=%.3f total_ms=%.3f\n",
            (tr_t_par - tr_t_entry) * 1e3, (last_end - tr_t_par) * 1e3,
            (tr_t_reg_end - tr_t_par) * 1e3, (tr_t_exit - tr_t_reg_end) * 1e3,
            (tr_t_exit - tr_t_entry) * 1e3);
#endif
}
