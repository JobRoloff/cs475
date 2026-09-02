/*/////////////////////////////////////////////////////////////////////////////
//
// File name : Mandelbrot.c
// Desc      : Escape-time Mandelbrot computation. Compiled three ways by
//             the Makefile, all from this single source file:
//               plain            -> bin/Mandelbrot.o      (sequential Current)
//               -DPAR -fopenmp   -> bin/MandelbrotPAR.o    (parallel Current)
//               -DGOLD           -> bin/MandelbrotGold.o   (trusted reference,
//                                    always sequential regardless of -DPAR --
//                                    see the rename below)
//             There is no separate Gold implementation to maintain: Gold is
//             this exact same escape-time logic, just compiled under a
//             different name. You should not need to touch
//             Mandelbrot-wrapper.c.
//
/////////////////////////////////////////////////////////////////////////////*/

#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#if defined(GOLD)
#define MandelbrotCompute MandelbrotGold
#endif

// Escape-time iteration count for a single point in the complex plane.
static int mandel_point(double cr, double ci, int max_iter) {
    double zr = 0.0, zi = 0.0;
    int iter = 0;
    while (zr * zr + zi * zi < 4.0 && iter < max_iter) {
        double tmp = zr * zr - zi * zi + cr;
        zi = 2 * zr * zi + ci;
        zr = tmp;
        iter++;
    }
    return iter;
}

// Fills img[width*height] with per-pixel iteration counts, over the complex
// window [real_min,real_max] x [imag_min,imag_max].
// Intentionally has NO explicit schedule() clause on the pragma below.
// GOLD builds never parallelize, regardless of -DPAR, since the #if below
// checks !defined(GOLD) too.
void MandelbrotCompute(int *img, int width, int height, int max_iter,
                        double real_min, double real_max,
                        double imag_min, double imag_max) {
    int row_counts[height]; 
#if defined(PAR) && !defined(GOLD)
#pragma omp parallel for 
#endif
    for (int i = 0; i < height; i++) {
        int sum_of_escape_iteraciton_counts = 0;
        for (int j = 0; j < width; j++) {
            double cr = real_min + j * (real_max - real_min) / (width - 1);
            double ci = imag_min + i * (imag_max - imag_min) / (height - 1);
            int point_value = mandel_point(cr, ci, max_iter);
            img[i * width + j] = point_value;
            sum_of_escape_iteraciton_counts += point_value;
        }
        row_counts[i] = sum_of_escape_iteraciton_counts;
    }
    // --- Begin Stage 2 Instrumentation ---
        // Only export the profile when running sequentially (!defined(PAR) and !defined(GOLD))
    #if !defined(PAR) && !defined(GOLD)
        FILE *f = fopen("density_profile.csv", "w");
        if (f != NULL) {
            fprintf(f, "row_index,density\n");
            for (int i = 0; i < height; i++) {
                fprintf(f, "%d,%d\n", i, row_counts[i]);
            }
            fclose(f);
            printf("Row-density profile written to density_profile.csv\n");
        }
    #endif
}