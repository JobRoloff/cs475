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

    // Iterage over coefficients of polynomial A
    for (int i = 0; i <= dA; i++) {
        // Iterage over coefficients of polynomial B
        for (int j = 0; j <= dB; j++) {
            // figure out which output coefficient recieves their product
            int k = i + j;
            // Multiply the selected coefficients and accumulate the result into the output coefficient c[k]
            BPM_ACC(
                c,
                comp,
                k,
                a[i] * b[j]
            );
        }
    }
    
    BPM_COMP_FREE(comp);
}