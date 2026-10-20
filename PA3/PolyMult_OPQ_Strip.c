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

    // Define the block/strip size (e.g., 8 for AVX2 vector width)
    const int STRIP_SIZE = 8;

    // Iterate over coefficients of polynomial A
    for (int i = 0; i <= dA; i++) {
        int j = 0;

        // Strip-mined inner loop: process chunks of STRIP_SIZE
        for (; j <= dB - STRIP_SIZE + 1; j += STRIP_SIZE) {
            for (int jj = 0; jj < STRIP_SIZE; jj++) {
                int curr_j = j + jj;
                int k = i + curr_j;
                
                BPM_ACC(
                    c,
                    comp,
                    k,
                    a[i] * b[curr_j]
                );
            }
        }

        // Tail loop for remaining elements where dB is not a multiple of STRIP_SIZE
        for (; j <= dB; j++) {
            int k = i + j;
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