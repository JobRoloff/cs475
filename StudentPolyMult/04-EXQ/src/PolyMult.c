#include "kernel.h"
#include "accumulate.h"


/**
 * Outer Product Polynomial Multiplication
 *
 * Computes the polynomials multiplication by traversing the logical outer product of the input coefficient vectors
 *
 * Each pair of coefficients produces one scalar product
 *
 *     product[i][j] = a[i] * b[j]
 *
 * Scalar product is immediately accumulated into the output coefficient corresponding to its combined degree
 * 
 * @param a
 *     Coefficients of the first input polynomial. a[i] is the coefficient
 *     of x^i. The array contains dA + 1 coefficients, representing a
 *     polynomial of degree dA.
 *
 * @param dA
 *     Degree of the first input polynomial. Valid coefficient indices
 *     for a are 0 through dA.
 *
 * @param b
 *     Coefficients of the second input polynomial. b[j] is the coefficient
 *     of x^j. The array contains dB + 1 coefficients, representing a
 *     polynomial of degree dB.
 *
 * @param dB
 *     Degree of the second input polynomial. Valid coefficient indices
 *     for b are 0 through dB.
 *
 * @param c
 *     Output array containing the coefficients of the resulting polynomial.
 *     The result has degree dA + dB and therefore contains
 *     dA + dB + 1 coefficients. c[k] is the coefficient of x^k.
 *
 * @param tune
 *     Optional tuning parameters for the benchmark framework. This
 *     implementation does not use any tuning parameters.
 */
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

    // Iterage over coefficients of polynomial B
    for (int j = 0; j <= dB; j++) {
        // Iterage over coefficients of polynomial A
        for (int i = 0; i <= dA; i++) {
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