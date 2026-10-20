#include "kernel.h"
#include "accumulate.h"

void kernel_multiply(const float *restrict a, int dA,
                     const float *restrict b, int dB,
                     float *restrict c,
                     const bpm_tune_t *restrict tune) {
    int output_length = dA + dB + 1;

    for (int k = 0; k < output_length; k++) {
        c[k] = 0.0f;
    }

    BPM_COMP_DECL(comp, output_length);

    // Retrieve block size from tuning parameter or fall back to default
    int block_size = 512;
    if (tune && tune->block_size > 0) {
        block_size = tune->block_size;
    }

    // Rows are jammed 8 at a time; the remaining (dA + 1) % 8 rows use plain OPQ
    int i_jam_end = (dA + 1) - (dA + 1) % 8;

    // Tile loop over b is outermost: the slice b[j0 .. jl] stays in L1 while
    // every 8-row block of a consumes it, and consecutive blocks' c windows
    // (c[i+j0 .. i+jl+7]) overlap in all but 8 elements.
    for (int j0 = 0; j0 <= dB; j0 += block_size) {
        int j1 = (j0 + block_size <= dB + 1) ? (j0 + block_size) : (dB + 1);
        int jl = j1 - 1;  // last b index in this tile

        for (int i = 0; i < i_jam_end; i += 8) {
            const float ar[8] = { a[i],     a[i + 1], a[i + 2], a[i + 3],
                                  a[i + 4], a[i + 5], a[i + 6], a[i + 7] };
            int t = j0;

            // Head: t < j0 + 7, the later rows have not entered the tile yet
            for (; t < j0 + 7; t++) {
                int r_lo = (t > jl ? t - jl : 0);
                for (int r = r_lo; r <= t - j0; r++) {
                    BPM_ACC(c, comp, i + t, ar[r] * b[t - r]);
                }
            }

            // Body: all eight rows read b from inside this tile
            for (; t <= jl; t++) {
                BPM_ACC(c, comp, i + t, ar[0] * b[t]);
                BPM_ACC(c, comp, i + t, ar[1] * b[t - 1]);
                BPM_ACC(c, comp, i + t, ar[2] * b[t - 2]);
                BPM_ACC(c, comp, i + t, ar[3] * b[t - 3]);
                BPM_ACC(c, comp, i + t, ar[4] * b[t - 4]);
                BPM_ACC(c, comp, i + t, ar[5] * b[t - 5]);
                BPM_ACC(c, comp, i + t, ar[6] * b[t - 6]);
                BPM_ACC(c, comp, i + t, ar[7] * b[t - 7]);
            }

            // Tail: t > jl, the earlier rows have already left the tile
            for (; t <= jl + 7; t++) {
                int r_hi = (t - j0 < 7 ? t - j0 : 7);
                for (int r = t - jl; r <= r_hi; r++) {
                    BPM_ACC(c, comp, i + t, ar[r] * b[t - r]);
                }
            }
        }

        // Remainder rows against this tile: plain OPQ
        for (int i = i_jam_end; i <= dA; i++) {
            for (int j = j0; j < j1; j++) {
                BPM_ACC(c, comp, i + j, a[i] * b[j]);
            }
        }
    }

    BPM_COMP_FREE(comp);
}
