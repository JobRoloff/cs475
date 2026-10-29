// Timed-build checker for DCP1.
//
// The correctness gate runs only the CHECKING build, whose leaf takes the
// scalar BPM_ACC path. Every reported rate comes from the timed build's
// intrinsics path, which the gate never executes. This program links
// PolyMult_DCP1.c compiled with the .time-par flags (-O3 -march=native
// -ffast-math -DPAR -fopenmp, plus -DTRED=N) and compares every output of
// kernel_multiply with a double-precision reference.
//
// Tolerance, per output k: |c[k] - ref[k]| <= (n_k + T + 2) * FLT_EPSILON * m_k,
// where m_k = sum |a_i * b_j| over the n_k terms of output k and T is the
// thread count (one extra rounding per private copy in the merge). This is
// relative to the output's magnitude sum, so it stays meaningful for
// mixed-sign inputs whose ref[k] is near zero. A window or offset bug gives
// errors of order m_k itself, far beyond this bound.
//
// c is filled with NaN before each call, so an output the kernel never
// writes fails too.
//
// Thread count comes from OMP_NUM_THREADS (verify_timed.sh sets it and the
// sweep's binding). Exit status 0 = every configuration passed.

#include <float.h>
#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include "kernel.h"

typedef struct {
    int dA, dB;
} cfg_t;

static const cfg_t cfgs[] = {
    // tiny
    {0, 0}, {0, 500}, {500, 0}, {1, 1}, {7, 7}, {8, 3},
    // square, below / at / around the leaf size (1280) and its multiples
    {100, 100}, {500, 500}, {1279, 1279}, {1280, 1280}, {1281, 1281},
    {2559, 2559}, {2560, 2560}, {2561, 2561}, {3000, 3000}, {7681, 7681},
    {9999, 9999}, {20000, 20000},
    // not divisible by the leaf size, unequal
    {5119, 5121}, {12345, 6789}, {6789, 12345},
    // lopsided
    {100000, 3}, {3, 100000}, {99999, 0}, {0, 99999}, {1, 5000}, {5000, 1},
    {1280, 20000}, {20000, 1281}, {40000, 2000}, {2000, 40000}, {30001, 301},
};
#define NCFG ((int)(sizeof(cfgs) / sizeof(cfgs[0])))

// Uniform in [-1, 1]: mixed signs exercise cancellation.
static void fill(float *x, int n, unsigned *seed) {
    for (int i = 0; i < n; i++) {
        x[i] = 2.0f * ((float)rand_r(seed) / (float)RAND_MAX) - 1.0f;
    }
}

int main(void) {
    int nthr = omp_get_max_threads();
    bpm_tune_t tune = {0, 0, 0};   // kernel defaults: leaf 1280, cutoff 2
    int failed = 0;

    printf("verify_timed: build_params=\"%s\" threads=%d\n", kernel_build_params(), nthr);

    for (int s = 0; s < NCFG; s++) {
        int dA = cfgs[s].dA, dB = cfgs[s].dB;
        int nA = dA + 1, nB = dB + 1, n = dA + dB + 1;
        float *a = malloc(sizeof(float) * (size_t)nA);
        float *b = malloc(sizeof(float) * (size_t)nB);
        float *c = malloc(sizeof(float) * (size_t)n);
        double *ref = calloc((size_t)n, sizeof(double));
        double *mag = calloc((size_t)n, sizeof(double));
        if (!a || !b || !c || !ref || !mag) {
            fprintf(stderr, "allocation failed at dA=%d dB=%d\n", dA, dB);
            return 2;
        }

        unsigned seed = 12345u + (unsigned)s;
        fill(a, nA, &seed);
        fill(b, nB, &seed);
        for (int k = 0; k < n; k++) {
            c[k] = NAN;
        }

        kernel_multiply(a, dA, b, dB, c, &tune);

        for (int i = 0; i < nA; i++) {
            double ai = a[i];
            for (int j = 0; j < nB; j++) {
                double t = ai * (double)b[j];
                ref[i + j] += t;
                mag[i + j] += fabs(t);
            }
        }

        long bad = 0;
        int worst_k = -1;
        double worst_err = 0.0, worst_ratio = 0.0;   // ratio = err / (FLT_EPSILON * m_k)
        for (int k = 0; k < n; k++) {
            int lo = (k - dB > 0) ? k - dB : 0;
            int hi = (k < dA) ? k : dA;
            double nterms = (double)(hi - lo + 1);
            double err = fabs((double)c[k] - ref[k]);
            double bound = (nterms + nthr + 2) * FLT_EPSILON * mag[k];
            double ratio = (mag[k] > 0.0) ? err / (FLT_EPSILON * mag[k]) : (err > 0.0 ? INFINITY : 0.0);
            if (!(err <= bound)) {          // also catches NaN
                bad++;
                ratio = isnan(err) ? INFINITY : ratio;
            }
            if (worst_k < 0 || ratio > worst_ratio || isnan(err)) {
                worst_ratio = ratio;
                worst_err = err;
                worst_k = k;
            }
        }

        printf("  %-4s dA=%-6d dB=%-6d  bad=%ld/%d  worst: k=%d err=%.3e err/(eps*m_k)=%.2f\n",
               bad ? "FAIL" : "PASS", dA, dB, bad, n, worst_k, worst_err, worst_ratio);
        failed += (bad != 0);

        free(a);
        free(b);
        free(c);
        free(ref);
        free(mag);
    }

    printf("verify_timed: %d of %d configuration(s) failed (threads=%d)\n", failed, NCFG, nthr);
    return failed ? 1 : 0;
}
