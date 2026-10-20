#include "kernel.h"
#include "accumulate.h"

void kernel_multiply(const float *restrict a, int dA,
                     const float *restrict b, int dB,
                     float *restrict c,
                     const bpm_tune_t *restrict tune) {
    int output_length = dA + dB + 1;

    // Zero out output vector
    for (int k = 0; k < output_length; k++) {
        c[k] = 0.0f;
    }

    BPM_COMP_DECL(comp, output_length);

    // Retrieve block size from tuning parameter or fall back to default (e.g., 64 floats)
    int block_size = 64;
    if (tune && tune->block_size > 0) {
        block_size = tune->block_size;
    }

    // Outer loops: Tile the iteration space (i and j) into blocks of size 'block_size'
    for (int i_block = 0; i_block <= dA; i_block += block_size) {
        int i_end = (i_block + block_size <= dA + 1) ? (i_block + block_size) : (dA + 1);

        for (int j_block = 0; j_block <= dB; j_block += block_size) {
            int j_end = (j_block + block_size <= dB + 1) ? (j_block + block_size) : (dB + 1);

            // Inner loops: Iterate within the current tile
            for (int i = i_block; i < i_end; i++) {
                for (int j = j_block; j < j_end; j++) {
                    int k = i + j;

                    BPM_ACC(
                        c,
                        comp,
                        k,
                        a[i] * b[j]
                    );
                }
            }
        }
    }

    BPM_COMP_FREE(comp);
}