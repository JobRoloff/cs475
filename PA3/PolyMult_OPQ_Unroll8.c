#include "kernel.h"
#include "accumulate.h"

void kernel_multiply(const float *restrict a, int dA,
                     const float *restrict b, int dB,
                     float *restrict c,
                     const bpm_tune_t *restrict tune) {
    (void)tune;

    int output_length = dA + dB + 1;

    for (int k = 0; k < output_length; k++) {
        c[k] = 0.0f;
    }

    BPM_COMP_DECL(comp, output_length);

    // Iterate over coefficients of polynomial A
    for (int i = 0; i <= dA; i++) {
        int j = 0;

        // Inner loop over coefficients of polynomial B, unrolled by 8
        for (; j + 7 <= dB; j += 8) {
            int k = i + j;
            BPM_ACC(c, comp, k,     a[i] * b[j]);
            BPM_ACC(c, comp, k + 1, a[i] * b[j + 1]);
            BPM_ACC(c, comp, k + 2, a[i] * b[j + 2]);
            BPM_ACC(c, comp, k + 3, a[i] * b[j + 3]);
            BPM_ACC(c, comp, k + 4, a[i] * b[j + 4]);
            BPM_ACC(c, comp, k + 5, a[i] * b[j + 5]);
            BPM_ACC(c, comp, k + 6, a[i] * b[j + 6]);
            BPM_ACC(c, comp, k + 7, a[i] * b[j + 7]);
        }

        // Tail loop for the remaining (dB + 1) % 8 coefficients
        for (; j <= dB; j++) {
            BPM_ACC(c, comp, i + j, a[i] * b[j]);
        }
    }

    BPM_COMP_FREE(comp);
}
