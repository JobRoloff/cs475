#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "args.h"
#include "range.h"

static void usage(const char *progname, const char *extra_usage) {
    printf("Usage: %s [-h|--help] [-dA <int>] [-dB <int>] [-t <range>] [-r <int>]\n", progname);
    printf("               [--block-size <int>] [--leaf-size <int>] [--task-threshold <int>]\n");
    printf("  -dA <int>            degree of polynomial A (default: -dB if given, else 99999)\n");
    printf("  -dB <int>            degree of polynomial B (default: -dA if given, else 99999)\n");
    printf("  -t <range>           thread count(s): enumeration (1,2,4), stride (2:16:2),\n");
    printf("                       or powers of 2 (1...16). Sequential builds accept only 1.\n");
    printf("  -r <int>             repetitions per configuration (default: 3)\n");
    printf("  --block-size <int>   block size (blocked variants; ignored otherwise)\n");
    printf("  --leaf-size <int>    recursion leaf size (DCS/DCP; ignored otherwise)\n");
    printf("  --task-threshold <int>  task spawn threshold (DCP; ignored otherwise)\n");
    if (extra_usage) printf("%s", extra_usage);
}

int bpm_parse_args(int argc, char **argv, const char *progname, const char *extra_usage, bpm_args_t *args) {
    args->dA = -1;
    args->dB = -1;
    args->threads = NULL;
    args->nthreads = 0;
    args->reps = 3;
    args->tune.block_size = 0;
    args->tune.leaf_size = 0;
    args->tune.task_threshold = 0;
    args->help = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(progname, extra_usage);
            args->help = 1;
            return 0;
        } else if (!strcmp(argv[i], "-dA") && i + 1 < argc) {
            args->dA = strtol(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "-dB") && i + 1 < argc) {
            args->dB = strtol(argv[++i], NULL, 10);
        } else if ((!strcmp(argv[i], "-r") || !strcmp(argv[i], "--reps")) && i + 1 < argc) {
            args->reps = (int)strtol(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--block-size") && i + 1 < argc) {
            args->tune.block_size = (int)strtol(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--leaf-size") && i + 1 < argc) {
            args->tune.leaf_size = (int)strtol(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--task-threshold") && i + 1 < argc) {
            args->tune.task_threshold = (int)strtol(argv[++i], NULL, 10);
        } else if ((!strcmp(argv[i], "-t") || !strcmp(argv[i], "--threads")) && i + 1 < argc) {
            if (bpm_parse_range(argv[++i], &args->threads, &args->nthreads) != 0) {
                fprintf(stderr, "Malformed -t range: %s\n", argv[i]);
                usage(progname, extra_usage);
                return -1;
            }
        } else {
            fprintf(stderr, "Unrecognized argument: %s\n", argv[i]);
            usage(progname, extra_usage);
            return -1;
        }
    }

    if (args->dA < 0 && args->dB < 0) { args->dA = 99999; args->dB = 99999; }
    else if (args->dA < 0) args->dA = args->dB;
    else if (args->dB < 0) args->dB = args->dA;

    if (args->dA < 0 || args->dB < 0) {
        fprintf(stderr, "Degrees must be non-negative (got dA=%ld dB=%ld)\n", args->dA, args->dB);
        return -1;
    }
    if (args->reps < 1) {
        fprintf(stderr, "--reps must be at least 1 (got %d)\n", args->reps);
        return -1;
    }

    if (args->threads == NULL) {
        args->threads = malloc(sizeof(long));
        args->threads[0] = 1;
        args->nthreads = 1;
    }

    /* A sequential build cannot honour a thread sweep. Fail loudly rather
     * than silently reporting 1-thread numbers under a multi-thread label. */
#ifndef PAR
    for (int i = 0; i < args->nthreads; i++) {
        if (args->threads[i] != 1) {
            fprintf(stderr,
                    "This is a sequential build: -t accepts only 1 (got %ld).\n"
                    "Use the .time-par binary for a thread sweep.\n",
                    args->threads[i]);
            return -1;
        }
    }
#endif

    return 0;
}

void bpm_free_args(bpm_args_t *args) {
    free(args->threads);
    args->threads = NULL;
}
