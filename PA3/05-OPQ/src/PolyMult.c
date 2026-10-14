#include "kernel.h"
#include "accumulate.h"


/**
 * Tiled Outer Product Polynomial Multiplication
 *
 * Same computation as the outer product kernel, but the (i, j) iteration
 * space is split into square B x B tiles. The tile loops (ii, jj) are
 * outermost, with ii outside jj, and the micro-kernel loops (i, j)
 * traverse one tile. The micro-kernel's j loop is manually unrolled by 4.
 *
 * Local tile accumulation: each tile accumulates into a stack scratchpad
 * c_local (length tile_dA + tile_dB + 1), which is committed to the global
 * c in a single pass once the tile finishes.
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

            // Local scratchpad for this tile's output range c[ii+jj .. ]
            int tile_dA = i_end - ii - 1;
            int tile_dB = j_end - jj - 1;
            int local_len = tile_dA + tile_dB + 1;
            float c_local[2 * B - 1];
            for (int lk = 0; lk < local_len; lk++) {
                c_local[lk] = 0.0f;
            }
            BPM_COMP_DECL(comp_local, local_len);

            // Micro-kernel: traverse one tile, accumulating into c_local
            for (int i = ii; i < i_end; i++) {
                // Hold a[i] in a register for reuse across the tile's b[j]
                const float ai = a[i];
                int local_i = i - ii;
                int j = jj;
                // Traverse the tile's j range, manually unrolled by 4
                for (; j + 3 < j_end; j += 4) {
                    int local_k = local_i + (j - jj);
                    BPM_ACC(c_local, comp_local, local_k,     ai * b[j]);
                    BPM_ACC(c_local, comp_local, local_k + 1, ai * b[j + 1]);
                    BPM_ACC(c_local, comp_local, local_k + 2, ai * b[j + 2]);
                    BPM_ACC(c_local, comp_local, local_k + 3, ai * b[j + 3]);
                }
                // Remainder: the (j_end - jj) % 4 coefficients left over
                for (; j < j_end; j++) {
                    BPM_ACC(c_local, comp_local, local_i + (j - jj), ai * b[j]);
                }
            }

            // Commit phase: write the tile's local sums back to global c
            int k_base = ii + jj;
            for (int lk = 0; lk < local_len; lk++) {
                BPM_ACC(c, comp, k_base + lk, c_local[lk]);
            }
            BPM_COMP_FREE(comp_local);
        }
    }

    BPM_COMP_FREE(comp);
}
