#ifndef BPM_KERNEL_H
#define BPM_KERNEL_H

/* Tuning values are supplied at run time, so sweeping a block size is a
 * shell loop rather than a rebuild. Fields a variant does not use are
 * ignored by that variant's kernel. Thread count is deliberately absent:
 * it is an OpenMP runtime setting the harness applies, not something a
 * kernel reads. */
typedef struct {
    int block_size;      /* OPB, EXB, and the DCS/DCP leaf body */
    int leaf_size;       /* DCS, DCP: a degree threshold, not a recursion depth */
    int task_threshold;  /* DCP only */
} bpm_tune_t;

/* Every variant implements this function. `c` has dA+dB+1 elements
 * (indices 0..dA+dB); kernel_multiply must write all of them.
 * dA and dB are independent -- never assume dA == dB.
 *
 * UPDATES.md U-001-26/08/15: every pointer parameter is restrict-qualified.
 * Without it the compiler must assume c[k] may alias a[i] or b[j], which
 * blocks hoisting the accumulator out of the inner loop and therefore blocks
 * vectorisation -- a measurement then reports an aliasing artefact rather
 * than the algorithm. `restrict` is a promise the caller must keep: the
 * harness allocates a, b, and c as separate arrays, and a variant that
 * aliases them internally violates the contract. Note the qualifier must
 * appear on each variant's *definition* to affect its codegen; repeating it
 * here keeps the declaration honest about the contract. */
void kernel_multiply(const float *restrict a, int dA,
                     const float *restrict b, int dB,
                     float *restrict c, const bpm_tune_t *restrict tune);

/* UPDATES.md U-002-26/08/15: a parameter fixed at compile time by a -D is
 * reported per observation, like a runtime one. It cannot travel through
 * bpm_tune_t -- by the time the binary runs, the value is already baked in --
 * so the binary self-reports it instead, and only the binary can: the shell
 * that invoked it never saw the -D.
 *
 * Returns a `key=value;key=value` string, e.g. "sched=dynamic,8" or
 * "unroll=2". The collector splits it into one trials.csv column per key, so
 * the harness never learns which knobs exist; a variant naming a new one needs
 * no change here or in the collector. Values may contain commas (a schedule
 * clause does), so pairs are `;`-separated and the field is quoted on output.
 *
 * A variant with no compile-time parameters defines nothing: time_main.c
 * carries a weak definition returning "". */
const char *kernel_build_params(void);

#endif