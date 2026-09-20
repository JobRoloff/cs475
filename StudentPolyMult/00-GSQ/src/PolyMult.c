#include "kernel.h"
#include "accumulate.h"

/* Diagonal traversal (VARIANTS.md 00-GSQ): c[k] is the sum over the
 * anti-diagonal i+j=k. GSQ has no tuning parameters. */
void kernel_multiply(const float *restrict a, int dA,
                     const float *restrict b, int dB,
                     float *restrict c, const bpm_tune_t *restrict tune) {
    (void)tune;

    // store indicies of the polynomial values
    int n = dA + dB + 1;
    for (int k = 0; k < n; k++) c[k] = 0.0f;

    BPM_COMP_DECL(comp, n);

    // walk the anti-diagonals
    for (int k = 0; k < n; k++) {
        // calculate anti-diagnal bounds output operand 
        int jmin = (k - dA > 0) ? (k - dA) : 0;
        int jmax = (dB < k) ? dB : k;
        // walk the individual anti-diagonal's cells
        for (int j = jmin; j <= jmax; j++) {
            int i = k - j;
            // compute the cell value via dot product of the input polynomial
            BPM_ACC(c, comp, k, a[i] * b[j]);
        }
    }

    BPM_COMP_FREE(comp);
}

/**
 * In polynomial multiplication, you line up the polynomials and multiply the coefficients with the matching degree into an output polynomial
 * 
 * A(x) = a0 + a1x + anx^n
 * B(x) = b0 + b1x + bnx^n
 * 
 * C(x) = a0x0 
 *      + a0b1x + a1b0x
 *      + a0b2x^2 + a1b1x^2 + a2b0x^2
 *      + a1b2x^3 + a2b1x^3
 *      + a2b2x^4
 * 
 * C(x) = c0 + c1 + c2 + c3 + c4 
 * 
 * math fact -> x^i * x^j = x^(i+j)
 * considering the above fact, we can think about the following when looping through the input polynomials' operands:
 * 1. Which output coefficient am I computing?
 * 2. From the input polynomials A and B, what coeficient combination can i use to match that of the output coefficient in question?
 * 
 * The above 2 questions could also be thought of as a certain type of matrix multiplication where polynomial operands serve as row / col labels.
 * 
 * Knowing polynomial multiplication is simply lining up operands and multiplying them, we can do the same with filling out the grid's contents. With such a grid, the anti-diagonals represent output values that corresponds to the mentioned math fact.
 * 
 * 
 */

