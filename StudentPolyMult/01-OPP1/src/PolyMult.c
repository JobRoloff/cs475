#include "kernel.h"
#include "accumulate.h"
#include <omp.h>

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

        /*
         * For a given output coefficient k:
         *
         *     c[k] = sum a[i] * b[j]
         *
         * where:
         *
         *     i + j = k
         *
         * The valid range of i is constrained by both
         * polynomial degrees.
         */
        int i_start = k - dB;
        if (i_start < 0) {
            i_start = 0;
        }

        int i_end = k;
        if (i_end > dA) {
            i_end = dA;
        }

        for (int i = i_start; i <= i_end; i++) {
            int j = k - i;

            BPM_ACC(
                c,
                comp,
                k,
                a[i] * b[j]
            );
        }

        BPM_COMP_FREE(comp);
    }
}