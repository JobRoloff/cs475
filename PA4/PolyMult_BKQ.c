#include <immintrin.h>
#include <string.h>
#include "kernel.h"
#include "accumulate.h"

// Tile width over b. Compile-time so it can be rescanned with -DBS=<n>.
#ifndef BS
#define BS 1280
#endif

#define JAM_R  8              // rows of a fused per pass (unroll-and-jam)
#define IL_U   3              // independent 8-float vectors per pass
#define NCOPY  4              // shifted copies of each b block (shifts 0..3)
#define PAD    (JAM_R - 1)    // zeros on each side of a b block

// One shifted copy: PAD + BS + PAD of padded block, + up to 3 shift, + slack
// so a full 8-float load past the last output only ever reads zeros.
// Rounded to 16 floats so every copy starts on a 64-byte boundary.
#define CLEN   (((BS + 2 * PAD + NCOPY + 16) + 15) & ~15)

#define STR_(x) #x
#define STR(x)  STR_(x)

const char *kernel_build_params(void) {
    return "bs=" STR(BS) ";r=" STR(JAM_R) ";u=" STR(IL_U);
}

// The timing build uses -ffast-math; keep the per-output FMA chain in order.
__attribute__((optimize("no-associative-math")))
void kernel_multiply(const float *restrict a, int dA,
                     const float *restrict b, int dB,
                     float *restrict c,
                     const bpm_tune_t *restrict tune) {
    (void)tune;   // block size is the compile-time BS

    // The harness allocates with plain malloc: only 16-byte alignment is guaranteed.
    const float *restrict a_al = (const float *)__builtin_assume_aligned(a, 16);
    const float *restrict b_al = (const float *)__builtin_assume_aligned(b, 16);
    float *restrict c_al = (float *)__builtin_assume_aligned(c, 16);

    int out_len = dA + dB + 1;

    for (int k = 0; k < out_len; k++) {
        c_al[k] = 0.0f;
    }

    BPM_COMP_DECL(comp, out_len);

    // NCOPY copies of the zero-padded b block, each 64-byte aligned.
    float *bcopy = (float *)aligned_alloc(64, (size_t)NCOPY * CLEN * sizeof(float));

    for (int j0 = 0; j0 <= dB; j0 += BS) {
        int len = (j0 + BS <= dB + 1) ? BS : (dB + 1 - j0);

        // Padded block P[x] = b[j0 + x - PAD] for PAD <= x < PAD + len, else 0.
        // Copy s holds P shifted right by s floats: copy_s[y] = P[y - s].
        memset(bcopy, 0, (size_t)NCOPY * CLEN * sizeof(float));
        for (int s = 0; s < NCOPY; s++) {
            float *cs = bcopy + s * CLEN;
            for (int x = 0; x < len; x++) {
                cs[PAD + s + x] = b_al[j0 + x];
            }
        }

        // Row groups start at multiples of JAM_R, so the first output of
        // every group (k0 = i + j0) has k0 mod 4 == j0 mod 4. h scalar
        // outputs bring c to a 16-byte boundary; vector passes start at m = h.
        int h = (4 - (j0 & 3)) & 3;

        // Row r reads P[m + PAD - r]. Pick the copy that puts that window
        // on a 16-byte boundary when m == h (mod 4); rows r and r+4 share it.
        const float *brow[JAM_R];
        for (int r = 0; r < JAM_R; r++) {
            int s = (r - PAD - h) & 3;
            brow[r] = bcopy + s * CLEN + PAD - r + s;
        }

        // Rows past dA are zero-padded rows of a, so the last group runs
        // the same code; outputs are clamped to c's length.
        for (int i = 0; i <= dA; i += JAM_R) {
            float ar[JAM_R];
            for (int r = 0; r < JAM_R; r++) {
                ar[r] = (i + r <= dA) ? a_al[i + r] : 0.0f;
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

    free(bcopy);
    BPM_COMP_FREE(comp);
}
