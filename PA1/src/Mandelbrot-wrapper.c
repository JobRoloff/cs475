/*/////////////////////////////////////////////////////////////////////////////
//
// File name : Mandelbrot-wrapper.c
// Desc      : Timing/checking harness for the Mandelbrot lab, modeled on
//             the PolyMult-wrapper.c pattern used in later assignments.
//             Students should not need to modify this file.
//
//             Build variants (see Makefile):
//               -DPAR         Current implementation is the parallel one
//                              (link against MandelbrotPAR.o, add -fopenmp)
//                              Omit -DPAR to time the sequential Current.
//               -DCHECKING    Compare Current's output against Gold instead
//                              of reporting times. Only meaningful for the
//                              -DPAR build (checking sequential Current
//                              against Gold, which is also sequential, is
//                              circular -- see Makefile, there is no
//                              Mandelbrot.check target, only .check-par).
//
//             Command-line flags (any subset, any order -- all optional):
//               -w WIDTH      image width                    (default 2400)
//               -h HEIGHT     image height                   (default 2400)
//               -i MAX_ITER   iteration budget                (default 1000)
//               -a REAL_MIN   real-axis lower bound             (default -2.0)
//               -b REAL_MAX   real-axis upper bound              (default 2.0)
//               -c IMAG_MIN   imaginary-axis lower bound          (default -2.0)
//               -d IMAG_MAX   imaginary-axis upper bound           (default 2.0)
//               -v FILE       write a color PPM visualization of Current's
//                              output to FILE (skipped by default -- writing
//                              an image on every sweep trial would slow data
//                              collection for no benefit)
//
/////////////////////////////////////////////////////////////////////////////*/

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>

#define DEFAULT_WIDTH 2400
#define DEFAULT_HEIGHT 2400
#define DEFAULT_MAX_ITER 10000
#define DEFAULT_REAL_MIN -2.0
#define DEFAULT_REAL_MAX 2.0
#define DEFAULT_IMAG_MIN -2.0
#define DEFAULT_IMAG_MAX 2.0

// "Current" -- whichever object file this wrapper was linked against
// (bin/Mandelbrot.o for sequential, bin/MandelbrotPAR.o for parallel).
void MandelbrotCompute(int *img, int width, int height, int max_iter,
                        double real_min, double real_max,
                        double imag_min, double imag_max);

// Trusted sequential baseline (bin/MandelbrotGold.o), only used for
// -DCHECKING. Same source file as Current, compiled with -DGOLD.
void MandelbrotGold(int *img, int width, int height, int max_iter,
                     double real_min, double real_max,
                     double imag_min, double imag_max);

// Writes img[] (raw escape counts, each in [0, max_iter]) as a color PPM.
// In-set pixels (escape == max_iter) are black. Escaping pixels are scaled
// by sqrt(escape/color_scale_ref): for nearly all pixels the escape count
// itself does not grow with max_iter (a point that escapes in 15 iterations
// does so regardless of the iteration budget) -- only the thin band of
// slow-escaping points near the boundary does. color_scale_ref tracks
// max_iter/80 (floored at 32) rather than a single fixed constant: a purely
// fixed reference kept ordinary pixels' colors stable across -i but let the
// boundary band saturate to solid yellow at high -i (no variance there,
// since escape counts there scale with max_iter); scaling the reference
// with max_iter keeps that band from saturating as -i grows, at the cost of
// slowly drifting the bulk-pixel colors too. At the -i=10000 default this
// equals the old fixed value (10000/80 = 125), so behavior there is
// unchanged. The scaled fraction in [0,1] is linearly interpolated dark
// blue (0) -> MID (0.5) -> yellow (1), then reversed so low escape counts
// render yellow and high (near-boundary) counts render blue. MID itself
// shifts from green to red as max_iter grows (log scale over -i in
// [100,20000], saturating at red for any max_iter beyond 20000), so the
// overall palette -- not just the amount of boundary detail -- visibly
// depends on -i.
static void writePPM(const char *filename, int *img, int width, int height, int max_iter) {
    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        printf("Warning: could not open %s for writing; skipping visualization.\n", filename);
        return;
    }
    static const int LOW[3]      = {0, 0, 80};      // dark blue
    static const int MID_LOW[3]  = {0, 200, 0};      // green  (low max_iter)
    static const int MID_HIGH[3] = {200, 0, 0};      // red    (high max_iter)
    static const int HIGH[3]     = {255, 255, 0};    // yellow

    double color_scale_ref = (double)max_iter / 80.0;
    if (color_scale_ref < 32.0) color_scale_ref = 32.0;

    double t = log10((double)max_iter / 100.0) / log10(20000.0 / 100.0);
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    int MID[3];
    for (int k = 0; k < 3; k++) {
        MID[k] = (int)(MID_LOW[k] + t * (MID_HIGH[k] - MID_LOW[k]));
    }

    fprintf(f, "P3\n%d %d\n255\n", width, height);
    for (int n = 0; n < width * height; n++) {
        int r, g, b;
        if (img[n] >= max_iter) {
            r = g = b = 0; // in the set
        } else {
            double frac = sqrt((double)img[n] / color_scale_ref);
            if (frac > 1.0) frac = 1.0;
            frac = 1.0 - frac; // reversed: low escape counts -> yellow, high -> blue
            if (frac <= 0.5) {
                double s = frac / 0.5;
                r = (int)(LOW[0] + s * (MID[0] - LOW[0]));
                g = (int)(LOW[1] + s * (MID[1] - LOW[1]));
                b = (int)(LOW[2] + s * (MID[2] - LOW[2]));
            } else {
                double s = (frac - 0.5) / 0.5;
                r = (int)(MID[0] + s * (HIGH[0] - MID[0]));
                g = (int)(MID[1] + s * (HIGH[1] - MID[1]));
                b = (int)(MID[2] + s * (HIGH[2] - MID[2]));
            }
        }
        fprintf(f, "%d %d %d\n", r, g, b);
    }
    fclose(f);
}

static void usage(const char *prog) {
    printf("Usage: %s [-w width] [-h height] [-i max_iter]\n", prog);
    printf("       [-a real_min] [-b real_max] [-c imag_min] [-d imag_max]\n");
    printf("       [-v outfile.ppm]\n");
}

int main(int argc, char **argv) {
    int width = DEFAULT_WIDTH;
    int height = DEFAULT_HEIGHT;
    int max_iter = DEFAULT_MAX_ITER;
    double real_min = DEFAULT_REAL_MIN;
    double real_max = DEFAULT_REAL_MAX;
    double imag_min = DEFAULT_IMAG_MIN;
    double imag_max = DEFAULT_IMAG_MAX;
    const char *visfile = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "w:h:i:a:b:c:d:v:?")) != -1) {
        switch (opt) {
            case 'w': width = atoi(optarg); break;
            case 'h': height = atoi(optarg); break;
            case 'i': max_iter = atoi(optarg); break;
            case 'a': real_min = atof(optarg); break;
            case 'b': real_max = atof(optarg); break;
            case 'c': imag_min = atof(optarg); break;
            case 'd': imag_max = atof(optarg); break;
            case 'v': visfile = optarg; break;
            default:
                usage(argv[0]);
                exit(-1);
        }
    }

    if (width <= 0 || height <= 0 || max_iter <= 0) {
        printf("width, height, and max_iter must all be positive.\n");
        exit(-1);
    }
    if (real_max <= real_min || imag_max <= imag_min) {
        printf("Require real_max > real_min and imag_max > imag_min.\n");
        exit(-1);
    }

    int *C = (int *)malloc(sizeof(int) * width * height); // Current's output
    if (C == NULL) {
        printf("Failed to allocate %d x %d image.\n", width, height);
        exit(-1);
    }
#if defined(CHECKING)
    int *D = (int *)malloc(sizeof(int) * width * height); // Gold's output
    if (D == NULL) {
        printf("Failed to allocate %d x %d image.\n", width, height);
        exit(-1);
    }
#endif

    struct timeval time;
    double elapsed_current;

    //**********************************************************************//
    /*   TIME ONLY THE COMPUTATION -- NOT ALLOCATION, SETUP, OR OUTPUT      */
    //**********************************************************************//

    gettimeofday(&time, NULL);
    elapsed_current = ((double)time.tv_sec) + ((double)time.tv_usec) / (1000 * 1000);

    MandelbrotCompute(C, width, height, max_iter, real_min, real_max, imag_min, imag_max);

    gettimeofday(&time, NULL);
    double now = ((double)time.tv_sec) + ((double)time.tv_usec) / (1000 * 1000);
    elapsed_current = now - elapsed_current;

    // Gold only runs for correctness checks -- .time/.time-par never pay for
    // an extra full sequential pass on every sweep invocation.
#if defined(CHECKING)
    MandelbrotGold(D, width, height, max_iter, real_min, real_max, imag_min, imag_max);
#endif

    //**********************************************************************//
    /*                      PRINT OUTPUT (DEPENDS ON FLAGS)                 */
    //**********************************************************************//

#if defined(CHECKING)
    long error_count = 0;
    for (long n = 0; n < (long)width * height; n++) {
        if (C[n] != D[n]) {
            error_count++;
        }
    }
    printf("The total number of errors is %ld\n", error_count);
#else
    printf("Execution time for Current: \t%lf sec.\n", elapsed_current);
#endif

    if (visfile != NULL) {
        writePPM(visfile, C, width, height, max_iter);
    }

    free(C);
#if defined(CHECKING)
    free(D);
#endif
    return EXIT_SUCCESS;
}
