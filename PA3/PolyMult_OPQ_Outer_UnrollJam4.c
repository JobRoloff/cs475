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

    int i = 0;

    // Unroll the outer loop by 4 and jam the four inner loops together.
    // Rows i..i+3 all write into the window c[i .. i+dB+3]. Walking that
    // window by offset t, output c[i+t] receives a[i+r] * b[t-r] from each
    // row r whose b index t-r lies in [0, dB]. Each c[i+t] is then loaded
    // and stored once for four products instead of once per product.
    for (; i + 3 <= dA; i += 4) {
        const float ar[4] = { a[i], a[i + 1], a[i + 2], a[i + 3] };
        int t = 0;

        // Head: t < 3, the later rows have not started yet
        for (; t < 3; t++) {
            for (int r = (t > dB ? t - dB : 0); r <= t; r++) {
                BPM_ACC(c, comp, i + t, ar[r] * b[t - r]);
            }
        }

        // Body: all four rows contribute to c[i+t]
        for (; t <= dB; t++) {
            BPM_ACC(c, comp, i + t, ar[0] * b[t]);
            BPM_ACC(c, comp, i + t, ar[1] * b[t - 1]);
            BPM_ACC(c, comp, i + t, ar[2] * b[t - 2]);
            BPM_ACC(c, comp, i + t, ar[3] * b[t - 3]);
        }

        // Tail: t > dB, the earlier rows have already finished
        for (; t <= dB + 3; t++) {
            for (int r = t - dB; r < 4; r++) {
                BPM_ACC(c, comp, i + t, ar[r] * b[t - r]);
            }
        }
    }

    // Remainder rows when dA + 1 is not a multiple of 4: plain OPQ
    for (; i <= dA; i++) {
        for (int j = 0; j <= dB; j++) {
            BPM_ACC(c, comp, i + j, a[i] * b[j]);
        }
    }

    BPM_COMP_FREE(comp);
}
