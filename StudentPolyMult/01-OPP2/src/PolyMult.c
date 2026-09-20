#include "kernel.h"
#include "accumulate.h"
#include <omp.h>

/*
 * Number of independent accumulation chains to use.
 *
 * Valid values:
 *   4, 6, 8
 */
#define ACCUMULATORS 8

#if ACCUMULATORS != 4 && ACCUMULATORS != 6 && ACCUMULATORS != 8
#error "ACCUMULATORS must be 4, 6, or 8"
#endif

void kernel_multiply(const float *restrict a, int dA,
                     const float *restrict b, int dB,
                     float *restrict c,
                     const bpm_tune_t *restrict tune) {
    (void)tune;

    int output_length = dA + dB + 1;

    #pragma omp parallel for schedule(dynamic, 16)
    for (int k = 0; k < output_length; k++) {
        BPM_COMP_DECL(comp, output_length);

        c[k] = 0.0f;

        int i_start = k - dB;
        if (i_start < 0) {
            i_start = 0;
        }

        int i_end = k;
        if (i_end > dA) {
            i_end = dA;
        }

        float sum0 = 0.0f;
        float sum1 = 0.0f;
        float sum2 = 0.0f;
        float sum3 = 0.0f;
        float sum4 = 0.0f;
        float sum5 = 0.0f;
        float sum6 = 0.0f;
        float sum7 = 0.0f;

        float comp0 = 0.0f;
        float comp1 = 0.0f;
        float comp2 = 0.0f;
        float comp3 = 0.0f;
        float comp4 = 0.0f;
        float comp5 = 0.0f;
        float comp6 = 0.0f;
        float comp7 = 0.0f;

        int i = i_start;

        /*
         * Process ACCUMULATORS independent accumulation chains.
         */
        for (; i + ACCUMULATORS - 1 <= i_end;
             i += ACCUMULATORS) {

            BPM_ACC_LOCAL(sum0, comp0,
                          a[i] * b[k - i]);

            BPM_ACC_LOCAL(sum1, comp1,
                          a[i + 1] * b[k - i - 1]);

            BPM_ACC_LOCAL(sum2, comp2,
                          a[i + 2] * b[k - i - 2]);

            BPM_ACC_LOCAL(sum3, comp3,
                          a[i + 3] * b[k - i - 3]);

#if ACCUMULATORS >= 6
            BPM_ACC_LOCAL(sum4, comp4,
                          a[i + 4] * b[k - i - 4]);

            BPM_ACC_LOCAL(sum5, comp5,
                          a[i + 5] * b[k - i - 5]);
#endif

#if ACCUMULATORS >= 8
            BPM_ACC_LOCAL(sum6, comp6,
                          a[i + 6] * b[k - i - 6]);

            BPM_ACC_LOCAL(sum7, comp7,
                          a[i + 7] * b[k - i - 7]);
#endif
        }

        /*
         * Combine the independent accumulation chains.
         */
        float sum = sum0 + sum1 + sum2 + sum3;

#if ACCUMULATORS >= 6
        sum += sum4 + sum5;
#endif

#if ACCUMULATORS >= 8
        sum += sum6 + sum7;
#endif

        /*
         * Handle remaining terms that do not fill a complete
         * group of ACCUMULATORS.
         */
        for (; i <= i_end; i++) {
            sum += a[i] * b[k - i];
        }

        c[k] = sum;

        BPM_COMP_FREE(comp);
    }
}