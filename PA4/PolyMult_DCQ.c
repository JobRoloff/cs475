#include <immintrin.h>
#include <string.h>
#include "kernel.h"
#include "accumulate.h"

// Leaf size when --leaf-size is not given: BKQ's best single-core BS in PA3.
#define LEAF_DEFAULT 1280

#define JAM_R  8              // rows of a fused per pass (unroll-and-jam)
#define IL_U   3              // independent 8-float vectors per pass
#define NCOPY  4              // shifted copies of each b block (shifts 0..3)
#define PAD    (JAM_R - 1)    // zeros on each side of a b block

#define STR_(x) #x
#define STR(x)  STR_(x)

const char *kernel_build_params(void) {
    return "r=" STR(JAM_R) ";u=" STR(IL_U);
}

// Everything the recursion carries unchanged from the top-level call.
typedef struct {
    const float *a;
    float *c;
    int out_len;
    int leaf;             // leaf size L: b blocks start at multiples of L
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

// a[i0 .. i0+na) * b[j0 .. j0+nb), added into c at i0 + j0. i0 and j0 are
// always multiples of L, so a leaf's b range is exactly one padded block.
static void dc(const dc_ctx_t *restrict x, int i0, int na, int j0, int nb) {
    int L = x->leaf;
    if (na <= L && nb <= L) {
        leaf(x, i0, na, j0, nb);
        return;
    }

    // Split the longer range; split both when within a factor of 2.
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
    int alo[2] = {i0, i0 + ha}, alen[2] = {ha, na - ha};
    int blo[2] = {j0, j0 + hb}, blen[2] = {hb, nb - hb};

    for (int p = 0; p <= split_a; p++) {
        for (int q = 0; q <= split_b; q++) {
            dc(x, alo[p], alen[p], blo[q], blen[q]);
        }
    }
}

void kernel_multiply(const float *restrict a, int dA,
                     const float *restrict b, int dB,
                     float *restrict c,
                     const bpm_tune_t *restrict tune) {
    int L = (tune->leaf_size > 0) ? tune->leaf_size : LEAF_DEFAULT;
    int out_len = dA + dB + 1;

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
        .leaf = L, .clen = clen, .bcopy = bcopy,
#ifdef CHECKING
        .comp = comp,
#endif
    };
    dc(&ctx, 0, dA + 1, 0, dB + 1);

    free(bcopy);
    BPM_COMP_FREE(comp);
}
