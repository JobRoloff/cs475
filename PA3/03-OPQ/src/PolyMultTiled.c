#include "kernel.h"
#include "accumulate.h"


/**
 * Tiled Outer Product Polynomial Multiplication
 *
 * Same computation as the outer product kernel, but the (i, j) iteration
 * space is split into square B x B tiles. The tile loops (ii, jj) are
 * outermost, with ii outside jj, and the micro-kernel loops (i, j)
 * traverse one tile.
 *
 *     product[i][j] = a[i] * b[j]  accumulated into  c[i + j]
 *
 * Tiles at the right/bottom edges are clamped with min() bounds, so
 * dA + 1 and dB + 1 need not be multiples of B.
 *
 * @param a      Coefficients of the first polynomial (dA + 1 entries).
 * @param dA     Degree of the first polynomial.
 * @param b      Coefficients of the second polynomial (dB + 1 entries).
 * @param dB     Degree of the second polynomial.
 * @param c      Output coefficients (dA + dB + 1 entries).
 * @param tune   tune->block_size is the tile size B. B <= 0 means a
 *               single tile covering the whole iteration space.
 */

static inline int min_int(int x, int y) { return x < y ? x : y; }

void kernel_multiply(const float *restrict a, int dA,
                     const float *restrict b, int dB,
                     float *restrict c,
                     const bpm_tune_t *restrict tune) {
    int output_length = dA + dB + 1;

    // Tile size from the harness; B <= 0 means one whole tile (no tiling)
    int B = tune->block_size;
    if (B <= 0) {
        B = (dA > dB ? dA : dB) + 1;
    }

    for (int k = 0; k < output_length; k++) {
        c[k] = 0.0f;
    }

    BPM_COMP_DECL(comp, output_length);

    // Iterate over tiles of polynomial A
    for (int ii = 0; ii <= dA; ii += B) {
        int i_end = min_int(ii + B, dA + 1);
        // Iterate over tiles of polynomial B
        for (int jj = 0; jj <= dB; jj += B) {
            int j_end = min_int(jj + B, dB + 1);
            // Micro-kernel: traverse one tile
            for (int i = ii; i < i_end; i++) {
                for (int j = jj; j < j_end; j++) {
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
