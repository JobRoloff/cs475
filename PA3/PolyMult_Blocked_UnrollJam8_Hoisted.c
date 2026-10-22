#include <immintrin.h>
#include "kernel.h"
#include "accumulate.h"

void kernel_multiply(const float *restrict a, int dA,
                     const float *restrict b, int dB,
                     float *restrict c,
                     const bpm_tune_t *restrict tune) {
    // Inform the compiler that the base pointers are 64-byte aligned
    const float *restrict a_al = (const float *)__builtin_assume_aligned(a, 64);
    const float *restrict b_al = (const float *)__builtin_assume_aligned(b, 64);
    float *restrict c_al = (float *)__builtin_assume_aligned(c, 64);

    int output_length = dA + dB + 1;

    for (int k = 0; k < output_length; k++) {
        c_al[k] = 0.0f;
    }

    BPM_COMP_DECL(comp, output_length);

    int block_size = 512;
    if (tune && tune->block_size > 0) {
        block_size = tune->block_size;
    }

    int i_jam_end = (dA + 1) - (dA + 1) % 8;

    for (int j0 = 0; j0 <= dB; j0 += block_size) {
        int j1 = (j0 + block_size <= dB + 1) ? (j0 + block_size) : (dB + 1);
        int jl = j1 - 1;

        for (int i = 0; i < i_jam_end; i += 8) {
            __m256 va0 = _mm256_set1_ps(a_al[i + 0]);
            __m256 va1 = _mm256_set1_ps(a_al[i + 1]);
            __m256 va2 = _mm256_set1_ps(a_al[i + 2]);
            __m256 va3 = _mm256_set1_ps(a_al[i + 3]);
            __m256 va4 = _mm256_set1_ps(a_al[i + 4]);
            __m256 va5 = _mm256_set1_ps(a_al[i + 5]);
            __m256 va6 = _mm256_set1_ps(a_al[i + 6]);
            __m256 va7 = _mm256_set1_ps(a_al[i + 7]);

            int t = j0;

            // Head: scalar boundary loop
            for (; t < j0 + 7 && t <= jl; t++) {
                int r_lo = (t > jl ? t - jl : 0);
                for (int r = r_lo; r <= t - j0; r++) {
                    BPM_ACC(c_al, comp, i + t, a_al[i + r] * b_al[t - r]);
                }
            }

            // -------------------------------------------------------------
            // Body: 4-Way Independent Accumulators (32 floats / 4 YMM chunks)
            // Steps t by 32 to feed 4 independent dependency chains (vc0..vc3)
            // -------------------------------------------------------------
            for (; t + 31 <= jl; t += 32) {
#ifdef CHECKING
                for (int l = 0; l < 32; l++) {
                    for (int r = 0; r < 8; r++) {
                        BPM_ACC(c_al, comp, i + t + l, a_al[i + r] * b_al[t + l - r]);
                    }
                }
#else
                // Load FOUR 8-float chunks of C into independent registers
                __m256 vc0 = _mm256_loadu_ps(&c_al[i + t]);
                __m256 vc1 = _mm256_loadu_ps(&c_al[i + t + 8]);
                __m256 vc2 = _mm256_loadu_ps(&c_al[i + t + 16]);
                __m256 vc3 = _mm256_loadu_ps(&c_al[i + t + 24]);

                // Inner row loop unrolled across all 4 independent chains
                for (int r = 0; r < 8; r++) {
                    __m256 va = (r == 0) ? va0 : (r == 1) ? va1 : (r == 2) ? va2 : (r == 3) ? va3 :
                                (r == 4) ? va4 : (r == 5) ? va5 : (r == 6) ? va6 : va7;

                    __m256 vb_0 = _mm256_loadu_ps(&b_al[t - r]);
                    __m256 vb_1 = _mm256_loadu_ps(&b_al[t + 8 - r]);
                    __m256 vb_2 = _mm256_loadu_ps(&b_al[t + 16 - r]);
                    __m256 vb_3 = _mm256_loadu_ps(&b_al[t + 24 - r]);

                    // Interleave 4 independent FMA chains to hide 4-cycle latency
                    vc0 = _mm256_fmadd_ps(va, vb_0, vc0);
                    vc1 = _mm256_fmadd_ps(va, vb_1, vc1);
                    vc2 = _mm256_fmadd_ps(va, vb_2, vc2);
                    vc3 = _mm256_fmadd_ps(va, vb_3, vc3);
                }

                // Store all 4 finalized chunks back to memory ONCE
                _mm256_storeu_ps(&c_al[i + t], vc0);
                _mm256_storeu_ps(&c_al[i + t + 8], vc1);
                _mm256_storeu_ps(&c_al[i + t + 16], vc2);
                _mm256_storeu_ps(&c_al[i + t + 24], vc3);
#endif
            }

            // Cleanup loop for remaining 16-float blocks (uses 2 accumulators)
            for (; t + 15 <= jl; t += 16) {
#ifdef CHECKING
                for (int l = 0; l < 16; l++) {
                    for (int r = 0; r < 8; r++) {
                        BPM_ACC(c_al, comp, i + t + l, a_al[i + r] * b_al[t + l - r]);
                    }
                }
#else
                __m256 vc0 = _mm256_loadu_ps(&c_al[i + t]);
                __m256 vc1 = _mm256_loadu_ps(&c_al[i + t + 8]);

                for (int r = 0; r < 8; r++) {
                    __m256 va = (r == 0) ? va0 : (r == 1) ? va1 : (r == 2) ? va2 : (r == 3) ? va3 :
                                (r == 4) ? va4 : (r == 5) ? va5 : (r == 6) ? va6 : va7;

                    __m256 vb_0 = _mm256_loadu_ps(&b_al[t - r]);
                    __m256 vb_1 = _mm256_loadu_ps(&b_al[t + 8 - r]);

                    vc0 = _mm256_fmadd_ps(va, vb_0, vc0);
                    vc1 = _mm256_fmadd_ps(va, vb_1, vc1);
                }

                _mm256_storeu_ps(&c_al[i + t], vc0);
                _mm256_storeu_ps(&c_al[i + t + 8], vc1);
#endif
            }

            // Cleanup loop for remaining 8-float blocks (uses 1 accumulator)
            for (; t + 7 <= jl; t += 8) {
#ifdef CHECKING
                for (int l = 0; l < 8; l++) {
                    for (int r = 0; r < 8; r++) {
                        BPM_ACC(c_al, comp, i + t + l, a_al[i + r] * b_al[t + l - r]);
                    }
                }
#else
                __m256 vc = _mm256_loadu_ps(&c_al[i + t]);

                for (int r = 0; r < 8; r++) {
                    __m256 va = (r == 0) ? va0 : (r == 1) ? va1 : (r == 2) ? va2 : (r == 3) ? va3 :
                                (r == 4) ? va4 : (r == 5) ? va5 : (r == 6) ? va6 : va7;
                    __m256 vb = _mm256_loadu_ps(&b_al[t - r]);
                    vc = _mm256_fmadd_ps(va, vb, vc);
                }

                _mm256_storeu_ps(&c_al[i + t], vc);
#endif
            }

            // Tail: scalar cleanup
            for (; t <= jl + 7; t++) {
                int r_hi = (t - j0 < 7 ? t - j0 : 7);
                for (int r = (t > jl ? t - jl : 0); r <= r_hi; r++) {
                    BPM_ACC(c_al, comp, i + t, a_al[i + r] * b_al[t - r]);
                }
            }
        }

        // Remainder rows against this tile: plain OPQ
        for (int i = i_jam_end; i <= dA; i++) {
            for (int j = j0; j < j1; j++) {
                BPM_ACC(c_al, comp, i + j, a_al[i] * b_al[j]);
            }
        }
    }

    BPM_COMP_FREE(comp);
}
// #include <immintrin.h>
// #include "kernel.h"
// #include "accumulate.h"

// void kernel_multiply(const float *restrict a, int dA,
//                      const float *restrict b, int dB,
//                      float *restrict c,
//                      const bpm_tune_t *restrict tune) {
//     int output_length = dA + dB + 1;

//     for (int k = 0; k < output_length; k++) {
//         c[k] = 0.0f;
//     }

//     BPM_COMP_DECL(comp, output_length);

//     int block_size = 512;
//     if (tune && tune->block_size > 0) {
//         block_size = tune->block_size;
//     }

//     int i_jam_end = (dA + 1) - (dA + 1) % 8;

//     for (int j0 = 0; j0 <= dB; j0 += block_size) {
//         int j1 = (j0 + block_size <= dB + 1) ? (j0 + block_size) : (dB + 1);
//         int jl = j1 - 1;

//         for (int i = 0; i < i_jam_end; i += 8) {
//             __m256 va0 = _mm256_set1_ps(a[i + 0]);
//             __m256 va1 = _mm256_set1_ps(a[i + 1]);
//             __m256 va2 = _mm256_set1_ps(a[i + 2]);
//             __m256 va3 = _mm256_set1_ps(a[i + 3]);
//             __m256 va4 = _mm256_set1_ps(a[i + 4]);
//             __m256 va5 = _mm256_set1_ps(a[i + 5]);
//             __m256 va6 = _mm256_set1_ps(a[i + 6]);
//             __m256 va7 = _mm256_set1_ps(a[i + 7]);

//             int t = j0;

//             // Head: scalar boundary loop
//             for (; t < j0 + 7 && t <= jl; t++) {
//                 int r_lo = (t > jl ? t - jl : 0);
//                 for (int r = r_lo; r <= t - j0; r++) {
//                     BPM_ACC(c, comp, i + t, a[i + r] * b[t - r]);
//                 }
//             }

//             // -------------------------------------------------------------
//             // Body: 4-Way Independent Accumulators (32 floats / 4 YMM chunks)
//             // Steps t by 32 to feed 4 independent dependency chains (vc0..vc3)
//             // -------------------------------------------------------------
//             for (; t + 31 <= jl; t += 32) {
// #ifdef CHECKING
//                 for (int l = 0; l < 32; l++) {
//                     for (int r = 0; r < 8; r++) {
//                         BPM_ACC(c, comp, i + t + l, a[i + r] * b[t + l - r]);
//                     }
//                 }
// #else
//                 // Load FOUR 8-float chunks of C into independent registers
//                 __m256 vc0 = _mm256_loadu_ps(&c[i + t]);
//                 __m256 vc1 = _mm256_loadu_ps(&c[i + t + 8]);
//                 __m256 vc2 = _mm256_loadu_ps(&c[i + t + 16]);
//                 __m256 vc3 = _mm256_loadu_ps(&c[i + t + 24]);

//                 // Inner row loop unrolled across all 4 independent chains
//                 for (int r = 0; r < 8; r++) {
//                     __m256 va = (r == 0) ? va0 : (r == 1) ? va1 : (r == 2) ? va2 : (r == 3) ? va3 :
//                                 (r == 4) ? va4 : (r == 5) ? va5 : (r == 6) ? va6 : va7;

//                     __m256 vb_0 = _mm256_loadu_ps(&b[t - r]);
//                     __m256 vb_1 = _mm256_loadu_ps(&b[t + 8 - r]);
//                     __m256 vb_2 = _mm256_loadu_ps(&b[t + 16 - r]);
//                     __m256 vb_3 = _mm256_loadu_ps(&b[t + 24 - r]);

//                     // Interleave 4 independent FMA chains to hide 4-cycle latency
//                     vc0 = _mm256_fmadd_ps(va, vb_0, vc0);
//                     vc1 = _mm256_fmadd_ps(va, vb_1, vc1);
//                     vc2 = _mm256_fmadd_ps(va, vb_2, vc2);
//                     vc3 = _mm256_fmadd_ps(va, vb_3, vc3);
//                 }

//                 // Store all 4 finalized chunks back to memory ONCE
//                 _mm256_storeu_ps(&c[i + t], vc0);
//                 _mm256_storeu_ps(&c[i + t + 8], vc1);
//                 _mm256_storeu_ps(&c[i + t + 16], vc2);
//                 _mm256_storeu_ps(&c[i + t + 24], vc3);
// #endif
//             }

//             // Cleanup loop for remaining 16-float blocks (uses 2 accumulators)
//             for (; t + 15 <= jl; t += 16) {
// #ifdef CHECKING
//                 for (int l = 0; l < 16; l++) {
//                     for (int r = 0; r < 8; r++) {
//                         BPM_ACC(c, comp, i + t + l, a[i + r] * b[t + l - r]);
//                     }
//                 }
// #else
//                 __m256 vc0 = _mm256_loadu_ps(&c[i + t]);
//                 __m256 vc1 = _mm256_loadu_ps(&c[i + t + 8]);

//                 for (int r = 0; r < 8; r++) {
//                     __m256 va = (r == 0) ? va0 : (r == 1) ? va1 : (r == 2) ? va2 : (r == 3) ? va3 :
//                                 (r == 4) ? va4 : (r == 5) ? va5 : (r == 6) ? va6 : va7;

//                     __m256 vb_0 = _mm256_loadu_ps(&b[t - r]);
//                     __m256 vb_1 = _mm256_loadu_ps(&b[t + 8 - r]);

//                     vc0 = _mm256_fmadd_ps(va, vb_0, vc0);
//                     vc1 = _mm256_fmadd_ps(va, vb_1, vc1);
//                 }

//                 _mm256_storeu_ps(&c[i + t], vc0);
//                 _mm256_storeu_ps(&c[i + t + 8], vc1);
// #endif
//             }

//             // Cleanup loop for remaining 8-float blocks (uses 1 accumulator)
//             for (; t + 7 <= jl; t += 8) {
// #ifdef CHECKING
//                 for (int l = 0; l < 8; l++) {
//                     for (int r = 0; r < 8; r++) {
//                         BPM_ACC(c, comp, i + t + l, a[i + r] * b[t + l - r]);
//                     }
//                 }
// #else
//                 __m256 vc = _mm256_loadu_ps(&c[i + t]);

//                 for (int r = 0; r < 8; r++) {
//                     __m256 va = (r == 0) ? va0 : (r == 1) ? va1 : (r == 2) ? va2 : (r == 3) ? va3 :
//                                 (r == 4) ? va4 : (r == 5) ? va5 : (r == 6) ? va6 : va7;
//                     __m256 vb = _mm256_loadu_ps(&b[t - r]);
//                     vc = _mm256_fmadd_ps(va, vb, vc);
//                 }

//                 _mm256_storeu_ps(&c[i + t], vc);
// #endif
//             }

//             // Tail: scalar cleanup
//             for (; t <= jl + 7; t++) {
//                 int r_hi = (t - j0 < 7 ? t - j0 : 7);
//                 for (int r = (t > jl ? t - jl : 0); r <= r_hi; r++) {
//                     BPM_ACC(c, comp, i + t, a[i + r] * b[t - r]);
//                 }
//             }
//         }

//         // Remainder rows against this tile: plain OPQ
//         for (int i = i_jam_end; i <= dA; i++) {
//             for (int j = j0; j < j1; j++) {
//                 BPM_ACC(c, comp, i + j, a[i] * b[j]);
//             }
//         }
//     }

//     BPM_COMP_FREE(comp);
// }
// #include <immintrin.h>
// #include "kernel.h"
// #include "accumulate.h"

// void kernel_multiply(const float *restrict a, int dA,
//                      const float *restrict b, int dB,
//                      float *restrict c,
//                      const bpm_tune_t *restrict tune) {
//     int output_length = dA + dB + 1;

//     for (int k = 0; k < output_length; k++) {
//         c[k] = 0.0f;
//     }

//     BPM_COMP_DECL(comp, output_length);

//     int block_size = 512;
//     if (tune && tune->block_size > 0) {
//         block_size = tune->block_size;
//     }

//     int i_jam_end = (dA + 1) - (dA + 1) % 8;

//     for (int j0 = 0; j0 <= dB; j0 += block_size) {
//         int j1 = (j0 + block_size <= dB + 1) ? (j0 + block_size) : (dB + 1);
//         int jl = j1 - 1;

//         for (int i = 0; i < i_jam_end; i += 8) {
//             __m256 va0 = _mm256_set1_ps(a[i + 0]);
//             __m256 va1 = _mm256_set1_ps(a[i + 1]);
//             __m256 va2 = _mm256_set1_ps(a[i + 2]);
//             __m256 va3 = _mm256_set1_ps(a[i + 3]);
//             __m256 va4 = _mm256_set1_ps(a[i + 4]);
//             __m256 va5 = _mm256_set1_ps(a[i + 5]);
//             __m256 va6 = _mm256_set1_ps(a[i + 6]);
//             __m256 va7 = _mm256_set1_ps(a[i + 7]);

//             int t = j0;

//             // Head: scalar boundary loop
//             for (; t < j0 + 7 && t <= jl; t++) {
//                 int r_lo = (t > jl ? t - jl : 0);
//                 for (int r = r_lo; r <= t - j0; r++) {
//                     BPM_ACC(c, comp, i + t, a[i + r] * b[t - r]);
//                 }
//             }

//             // -------------------------------------------------------------
//             // Body: 4-Way Independent Accumulators (32 floats / 4 YMM chunks)
//             // Steps t by 32 to feed 4 independent dependency chains (vc0..vc3)
//             // -------------------------------------------------------------
//             for (; t + 31 <= jl; t += 32) {
// #ifdef CHECKING
//                 for (int l = 0; l < 32; l++) {
//                     for (int r = 0; r < 8; r++) {
//                         BPM_ACC(c, comp, i + t + l, a[i + r] * b[t + l - r]);
//                     }
//                 }
// #else
//                 // Load FOUR 8-float chunks of C into independent registers
//                 __m256 vc0 = _mm256_loadu_ps(&c[i + t]);
//                 __m256 vc1 = _mm256_loadu_ps(&c[i + t + 8]);
//                 __m256 vc2 = _mm256_loadu_ps(&c[i + t + 16]);
//                 __m256 vc3 = _mm256_loadu_ps(&c[i + t + 24]);

//                 // Inner row loop unrolled across all 4 independent chains
//                 for (int r = 0; r < 8; r++) {
//                     __m256 va = (r == 0) ? va0 : (r == 1) ? va1 : (r == 2) ? va2 : (r == 3) ? va3 :
//                                 (r == 4) ? va4 : (r == 5) ? va5 : (r == 6) ? va6 : va7;

//                     __m256 vb_0 = _mm256_loadu_ps(&b[t - r]);
//                     __m256 vb_1 = _mm256_loadu_ps(&b[t + 8 - r]);
//                     __m256 vb_2 = _mm256_loadu_ps(&b[t + 16 - r]);
//                     __m256 vb_3 = _mm256_loadu_ps(&b[t + 24 - r]);

//                     // Interleave 4 independent FMA chains to hide 4-cycle latency
//                     vc0 = _mm256_fmadd_ps(va, vb_0, vc0);
//                     vc1 = _mm256_fmadd_ps(va, vb_1, vc1);
//                     vc2 = _mm256_fmadd_ps(va, vb_2, vc2);
//                     vc3 = _mm256_fmadd_ps(va, vb_3, vc3);
//                 }

//                 // Store all 4 finalized chunks back to memory ONCE
//                 _mm256_storeu_ps(&c[i + t], vc0);
//                 _mm256_storeu_ps(&c[i + t + 8], vc1);
//                 _mm256_storeu_ps(&c[i + t + 16], vc2);
//                 _mm256_storeu_ps(&c[i + t + 24], vc3);
// #endif
//             }

//             // Cleanup loop for remaining 16-float blocks (uses 2 accumulators)
//             for (; t + 15 <= jl; t += 16) {
// #ifdef CHECKING
//                 for (int l = 0; l < 16; l++) {
//                     for (int r = 0; r < 8; r++) {
//                         BPM_ACC(c, comp, i + t + l, a[i + r] * b[t + l - r]);
//                     }
//                 }
// #else
//                 __m256 vc0 = _mm256_loadu_ps(&c[i + t]);
//                 __m256 vc1 = _mm256_loadu_ps(&c[i + t + 8]);

//                 for (int r = 0; r < 8; r++) {
//                     __m256 va = (r == 0) ? va0 : (r == 1) ? va1 : (r == 2) ? va2 : (r == 3) ? va3 :
//                                 (r == 4) ? va4 : (r == 5) ? va5 : (r == 6) ? va6 : va7;

//                     __m256 vb_0 = _mm256_loadu_ps(&b[t - r]);
//                     __m256 vb_1 = _mm256_loadu_ps(&b[t + 8 - r]);

//                     vc0 = _mm256_fmadd_ps(va, vb_0, vc0);
//                     vc1 = _mm256_fmadd_ps(va, vb_1, vc1);
//                 }

//                 _mm256_storeu_ps(&c[i + t], vc0);
//                 _mm256_storeu_ps(&c[i + t + 8], vc1);
// #endif
//             }

//             // Cleanup loop for remaining 8-float blocks (uses 1 accumulator)
//             for (; t + 7 <= jl; t += 8) {
// #ifdef CHECKING
//                 for (int l = 0; l < 8; l++) {
//                     for (int r = 0; r < 8; r++) {
//                         BPM_ACC(c, comp, i + t + l, a[i + r] * b[t + l - r]);
//                     }
//                 }
// #else
//                 __m256 vc = _mm256_loadu_ps(&c[i + t]);

//                 for (int r = 0; r < 8; r++) {
//                     __m256 va = (r == 0) ? va0 : (r == 1) ? va1 : (r == 2) ? va2 : (r == 3) ? va3 :
//                                 (r == 4) ? va4 : (r == 5) ? va5 : (r == 6) ? va6 : va7;
//                     __m256 vb = _mm256_loadu_ps(&b[t - r]);
//                     vc = _mm256_fmadd_ps(va, vb, vc);
//                 }

//                 _mm256_storeu_ps(&c[i + t], vc);
// #endif
//             }

//             // Tail: scalar cleanup
//             for (; t <= jl + 7; t++) {
//                 int r_hi = (t - j0 < 7 ? t - j0 : 7);
//                 for (int r = (t > jl ? t - jl : 0); r <= r_hi; r++) {
//                     BPM_ACC(c, comp, i + t, a[i + r] * b[t - r]);
//                 }
//             }
//         }

//         // Remainder rows against this tile: plain OPQ
//         for (int i = i_jam_end; i <= dA; i++) {
//             for (int j = j0; j < j1; j++) {
//                 BPM_ACC(c, comp, i + j, a[i] * b[j]);
//             }
//         }
//     }

//     BPM_COMP_FREE(comp);
// }

// #include <immintrin.h>
// #include "kernel.h"
// #include "accumulate.h"

// void kernel_multiply(const float *restrict a, int dA,
//                      const float *restrict b, int dB,
//                      float *restrict c,
//                      const bpm_tune_t *restrict tune) {
//     int output_length = dA + dB + 1;

//     for (int k = 0; k < output_length; k++) {
//         c[k] = 0.0f;
//     }

//     BPM_COMP_DECL(comp, output_length);

//     // Retrieve block size from tuning parameter or fall back to default (512 floats)
//     int block_size = 512;
//     if (tune && tune->block_size > 0) {
//         block_size = tune->block_size;
//     }

//     // Rows are jammed 8 at a time; remaining (dA + 1) % 8 rows use plain OPQ
//     int i_jam_end = (dA + 1) - (dA + 1) % 8;

//     // Tile loop over b is outermost: the slice b[j0 .. jl] stays in L1d
//     for (int j0 = 0; j0 <= dB; j0 += block_size) {
//         int j1 = (j0 + block_size <= dB + 1) ? (j0 + block_size) : (dB + 1);
//         int jl = j1 - 1;  // last b index in this tile

//         for (int i = 0; i < i_jam_end; i += 8) {
//             // Broadcast 'a[i .. i+7]' into 8 dedicated YMM registers
//             __m256 va0 = _mm256_set1_ps(a[i + 0]);
//             __m256 va1 = _mm256_set1_ps(a[i + 1]);
//             __m256 va2 = _mm256_set1_ps(a[i + 2]);
//             __m256 va3 = _mm256_set1_ps(a[i + 3]);
//             __m256 va4 = _mm256_set1_ps(a[i + 4]);
//             __m256 va5 = _mm256_set1_ps(a[i + 5]);
//             __m256 va6 = _mm256_set1_ps(a[i + 6]);
//             __m256 va7 = _mm256_set1_ps(a[i + 7]);

//             int t = j0;

//             // -------------------------------------------------------------
//             // Head: t < j0 + 7, scalar accumulation for unaligned boundary
//             // -------------------------------------------------------------
//             for (; t < j0 + 7 && t <= jl; t++) {
//                 int r_lo = (t > jl ? t - jl : 0);
//                 for (int r = r_lo; r <= t - j0; r++) {
//                     BPM_ACC(c, comp, i + t, a[i + r] * b[t - r]);
//                 }
//             }

//             // -------------------------------------------------------------
//             // Body: AVX2 Vectorized Inner Loop with Dual Independent Accumulators
//             // Unrolled 16 floats (2 YMM vectors) per step: vc0 = c[i+t..i+t+7], vc1 = c[i+t+8..i+t+15]
//             // Interleaving FMAs between vc0 and vc1 hides the 4-cycle FMA latency chain.
//             // -------------------------------------------------------------
//             for (; t + 15 <= jl; t += 16) {
// #ifdef CHECKING
//                 for (int l = 0; l < 16; l++) {
//                     for (int r = 0; r < 8; r++) {
//                         BPM_ACC(c, comp, i + t + l, a[i + r] * b[t + l - r]);
//                     }
//                 }
// #else
//                 // HOIST: Load TWO 8-float chunks of C into independent YMM registers
//                 __m256 vc0 = _mm256_loadu_ps(&c[i + t]);
//                 __m256 vc1 = _mm256_loadu_ps(&c[i + t + 8]);

//                 // Stream vector slices for chunk 0 (&b[t - r])
//                 __m256 vb0_0 = _mm256_loadu_ps(&b[t]);
//                 __m256 vb1_0 = _mm256_loadu_ps(&b[t - 1]);
//                 __m256 vb2_0 = _mm256_loadu_ps(&b[t - 2]);
//                 __m256 vb3_0 = _mm256_loadu_ps(&b[t - 3]);
//                 __m256 vb4_0 = _mm256_loadu_ps(&b[t - 4]);
//                 __m256 vb5_0 = _mm256_loadu_ps(&b[t - 5]);
//                 __m256 vb6_0 = _mm256_loadu_ps(&b[t - 6]);
//                 __m256 vb7_0 = _mm256_loadu_ps(&b[t - 7]);

//                 // Stream vector slices for chunk 1 (&b[t + 8 - r])
//                 __m256 vb0_1 = _mm256_loadu_ps(&b[t + 8]);
//                 __m256 vb1_1 = _mm256_loadu_ps(&b[t + 7]);
//                 __m256 vb2_1 = _mm256_loadu_ps(&b[t + 6]);
//                 __m256 vb3_1 = _mm256_loadu_ps(&b[t + 5]);
//                 __m256 vb4_1 = _mm256_loadu_ps(&b[t + 4]);
//                 __m256 vb5_1 = _mm256_loadu_ps(&b[t + 3]);
//                 __m256 vb6_1 = _mm256_loadu_ps(&b[t + 2]);
//                 __m256 vb7_1 = _mm256_loadu_ps(&b[t + 1]);

//                 // Interleave FMAs between vc0 and vc1 to eliminate latency stalls
//                 vc0 = _mm256_fmadd_ps(va0, vb0_0, vc0);
//                 vc1 = _mm256_fmadd_ps(va0, vb0_1, vc1);

//                 vc0 = _mm256_fmadd_ps(va1, vb1_0, vc0);
//                 vc1 = _mm256_fmadd_ps(va1, vb1_1, vc1);

//                 vc0 = _mm256_fmadd_ps(va2, vb2_0, vc0);
//                 vc1 = _mm256_fmadd_ps(va2, vb2_1, vc1);

//                 vc0 = _mm256_fmadd_ps(va3, vb3_0, vc0);
//                 vc1 = _mm256_fmadd_ps(va3, vb3_1, vc1);

//                 vc0 = _mm256_fmadd_ps(va4, vb4_0, vc0);
//                 vc1 = _mm256_fmadd_ps(va4, vb4_1, vc1);

//                 vc0 = _mm256_fmadd_ps(va5, vb5_0, vc0);
//                 vc1 = _mm256_fmadd_ps(va5, vb5_1, vc1);

//                 vc0 = _mm256_fmadd_ps(va6, vb6_0, vc0);
//                 vc1 = _mm256_fmadd_ps(va6, vb6_1, vc1);

//                 vc0 = _mm256_fmadd_ps(va7, vb7_0, vc0);
//                 vc1 = _mm256_fmadd_ps(va7, vb7_1, vc1);

//                 // STORE: Write both finalized YMM accumulators back to memory ONCE
//                 _mm256_storeu_ps(&c[i + t], vc0);
//                 _mm256_storeu_ps(&c[i + t + 8], vc1);
// #endif
//             }

//             // Single 8-float vector cleanup loop for odd step remainders before scalar tail
//             for (; t + 7 <= jl; t += 8) {
// #ifdef CHECKING
//                 for (int l = 0; l < 8; l++) {
//                     for (int r = 0; r < 8; r++) {
//                         BPM_ACC(c, comp, i + t + l, a[i + r] * b[t + l - r]);
//                     }
//                 }
// #else
//                 __m256 vc = _mm256_loadu_ps(&c[i + t]);

//                 __m256 vb0 = _mm256_loadu_ps(&b[t]);
//                 __m256 vb1 = _mm256_loadu_ps(&b[t - 1]);
//                 __m256 vb2 = _mm256_loadu_ps(&b[t - 2]);
//                 __m256 vb3 = _mm256_loadu_ps(&b[t - 3]);
//                 __m256 vb4 = _mm256_loadu_ps(&b[t - 4]);
//                 __m256 vb5 = _mm256_loadu_ps(&b[t - 5]);
//                 __m256 vb6 = _mm256_loadu_ps(&b[t - 6]);
//                 __m256 vb7 = _mm256_loadu_ps(&b[t - 7]);

//                 vc = _mm256_fmadd_ps(va0, vb0, vc);
//                 vc = _mm256_fmadd_ps(va1, vb1, vc);
//                 vc = _mm256_fmadd_ps(va2, vb2, vc);
//                 vc = _mm256_fmadd_ps(va3, vb3, vc);
//                 vc = _mm256_fmadd_ps(va4, vb4, vc);
//                 vc = _mm256_fmadd_ps(va5, vb5, vc);
//                 vc = _mm256_fmadd_ps(va6, vb6, vc);
//                 vc = _mm256_fmadd_ps(va7, vb7, vc);

//                 _mm256_storeu_ps(&c[i + t], vc);
// #endif
//             }

//             // -------------------------------------------------------------
//             // Tail: t > jl - 7, scalar cleanup for remaining elements
//             // -------------------------------------------------------------
//             for (; t <= jl + 7; t++) {
//                 int r_hi = (t - j0 < 7 ? t - j0 : 7);
//                 for (int r = (t > jl ? t - jl : 0); r <= r_hi; r++) {
//                     BPM_ACC(c, comp, i + t, a[i + r] * b[t - r]);
//                 }
//             }
//         }

//         // Remainder rows against this tile: plain OPQ
//         for (int i = i_jam_end; i <= dA; i++) {
//             for (int j = j0; j < j1; j++) {
//                 BPM_ACC(c, comp, i + j, a[i] * b[j]);
//             }
//         }
//     }

//     BPM_COMP_FREE(comp);
// }
// #include <immintrin.h>
// #include "kernel.h"
// #include "accumulate.h"

// void kernel_multiply(const float *restrict a, int dA,
//                      const float *restrict b, int dB,
//                      float *restrict c,
//                      const bpm_tune_t *restrict tune) {
//     int output_length = dA + dB + 1;

//     for (int k = 0; k < output_length; k++) {
//         c[k] = 0.0f;
//     }

//     BPM_COMP_DECL(comp, output_length);

//     // Retrieve block size from tuning parameter or fall back to default (512 floats)
//     int block_size = 512;
//     if (tune && tune->block_size > 0) {
//         block_size = tune->block_size;
//     }

//     // Rows are jammed 8 at a time; remaining (dA + 1) % 8 rows use plain OPQ
//     int i_jam_end = (dA + 1) - (dA + 1) % 8;

//     // Tile loop over b is outermost: the slice b[j0 .. jl] stays in L1d
//     for (int j0 = 0; j0 <= dB; j0 += block_size) {
//         int j1 = (j0 + block_size <= dB + 1) ? (j0 + block_size) : (dB + 1);
//         int jl = j1 - 1;  // last b index in this tile

//         for (int i = 0; i < i_jam_end; i += 8) {
//             // Broadcast 'a[i .. i+7]' into 8 dedicated YMM registers
//             __m256 va0 = _mm256_set1_ps(a[i + 0]);
//             __m256 va1 = _mm256_set1_ps(a[i + 1]);
//             __m256 va2 = _mm256_set1_ps(a[i + 2]);
//             __m256 va3 = _mm256_set1_ps(a[i + 3]);
//             __m256 va4 = _mm256_set1_ps(a[i + 4]);
//             __m256 va5 = _mm256_set1_ps(a[i + 5]);
//             __m256 va6 = _mm256_set1_ps(a[i + 6]);
//             __m256 va7 = _mm256_set1_ps(a[i + 7]);

//             int t = j0;

//             // -------------------------------------------------------------
//             // Head: t < j0 + 7, scalar accumulation for unaligned boundary
//             // -------------------------------------------------------------
//             for (; t < j0 + 7 && t <= jl; t++) {
//                 int r_lo = (t > jl ? t - jl : 0);
//                 for (int r = r_lo; r <= t - j0; r++) {
//                     BPM_ACC(c, comp, i + t, a[i + r] * b[t - r]);
//                 }
//             }

//             // -------------------------------------------------------------
//             // Body: AVX2 Vectorized Inner Loop (8 floats per step)
//             // Hoists c[i+t .. i+t+7] into YMM registers to eliminate memory port stalls
//             // -------------------------------------------------------------
//             for (; t + 7 <= jl; t += 8) {
// #ifdef CHECKING
//                 // Checking build: identical 8 outputs x 8 rows of products,
//                 // accumulated through BPM_ACC for Kahan compensation correctness gate
//                 for (int l = 0; l < 8; l++) {
//                     for (int r = 0; r < 8; r++) {
//                         BPM_ACC(c, comp, i + t + l, a[i + r] * b[t + l - r]);
//                     }
//                 }
// #else
//                 // HOIST: Load 8 contiguous output floats into a YMM register ONCE
//                 __m256 vc = _mm256_loadu_ps(&c[i + t]);

//                 // Stream unaligned 8-element vector slices of B
//                 __m256 vb0 = _mm256_loadu_ps(&b[t]);
//                 __m256 vb1 = _mm256_loadu_ps(&b[t - 1]);
//                 __m256 vb2 = _mm256_loadu_ps(&b[t - 2]);
//                 __m256 vb3 = _mm256_loadu_ps(&b[t - 3]);
//                 __m256 vb4 = _mm256_loadu_ps(&b[t - 4]);
//                 __m256 vb5 = _mm256_loadu_ps(&b[t - 5]);
//                 __m256 vb6 = _mm256_loadu_ps(&b[t - 6]);
//                 __m256 vb7 = _mm256_loadu_ps(&b[t - 7]);

//                 // 8 FMAs executed entirely inside CPU YMM registers
//                 vc = _mm256_fmadd_ps(va0, vb0, vc);
//                 vc = _mm256_fmadd_ps(va1, vb1, vc);
//                 vc = _mm256_fmadd_ps(va2, vb2, vc);
//                 vc = _mm256_fmadd_ps(va3, vb3, vc);
//                 vc = _mm256_fmadd_ps(va4, vb4, vc);
//                 vc = _mm256_fmadd_ps(va5, vb5, vc);
//                 vc = _mm256_fmadd_ps(va6, vb6, vc);
//                 vc = _mm256_fmadd_ps(va7, vb7, vc);

//                 // STORE: Write the finalized YMM accumulator back to memory ONCE
//                 _mm256_storeu_ps(&c[i + t], vc);
// #endif
//             }

//             // -------------------------------------------------------------
//             // Tail: t > jl - 7, scalar cleanup for remaining elements
//             // -------------------------------------------------------------
//             for (; t <= jl + 7; t++) {
//                 int r_hi = (t - j0 < 7 ? t - j0 : 7);
//                 for (int r = (t > jl ? t - jl : 0); r <= r_hi; r++) {
//                     BPM_ACC(c, comp, i + t, a[i + r] * b[t - r]);
//                 }
//             }
//         }

//         // Remainder rows against this tile: plain OPQ
//         for (int i = i_jam_end; i <= dA; i++) {
//             for (int j = j0; j < j1; j++) {
//                 BPM_ACC(c, comp, i + j, a[i] * b[j]);
//             }
//         }
//     }

//     BPM_COMP_FREE(comp);
// }