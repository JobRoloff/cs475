```bash
$ ./run_sweep.sh OPQ_Outer_UnrollJam4 "1" 5 -dA 128 -dB 128
threads  median_s       gflops       reps
1        0.000002326    14.309       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam4 "1" 5 -dA 256 -dB 256
threads  median_s       gflops       reps
1        0.000008649    15.273       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam4 "1" 5 -dA 512 -dB 512
threads  median_s       gflops       reps
1        0.000031700    16.604       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam4 "1" 5 -dA 1024 -dB 1024
threads  median_s       gflops       reps
1        0.000119529    17.579       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam4 "1" 5 -dA 2048 -dB 2048
threads  median_s       gflops       reps
1        0.000467511    17.961       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam4 "1" 5 -dA 4096 -dB 4096
threads  median_s       gflops       reps
1        0.001857879    18.069       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam4 "1" 5 -dA 8192 -dB 8192
threads  median_s       gflops       reps
1        0.007762632    17.294       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam4 "1" 5 -dA 16384 -dB 16384
threads  median_s       gflops       reps
1        0.030844327    17.408       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam4 "1" 5 -dA 32768 -dB 32768
threads  median_s       gflops       reps
1        0.122916076    17.472       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam4 "1" 5 -dA 65536 -dB 65536
threads  median_s       gflops       reps
1        0.491156922    17.490       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam8 "1" 5 -dA 128 -dB 128
threads  median_s       gflops       reps
1        0.000002958    11.252       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam8 "1" 5 -dA 256 -dB 256
threads  median_s       gflops       reps
1        0.000009279    14.236       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam8 "1" 5 -dA 512 -dB 512
threads  median_s       gflops       reps
1        0.000032963    15.968       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam8 "1" 5 -dA 1024 -dB 1024
threads  median_s       gflops       reps
1        0.000116103    18.098       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam8 "1" 5 -dA 2048 -dB 2048
threads  median_s       gflops       reps
1        0.000437233    19.204       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam8 "1" 5 -dA 4096 -dB 4096
threads  median_s       gflops       reps
1        0.001708525    19.649       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam8 "1" 5 -dA 8192 -dB 8192
threads  median_s       gflops       reps
1        0.006840783    19.625       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam8 "1" 5 -dA 16384 -dB 16384
threads  median_s       gflops       reps
1        0.027212176    19.731       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam8 "1" 5 -dA 32768 -dB 32768
threads  median_s       gflops       reps
1        0.108207100    19.847       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam8 "1" 5 -dA 65536 -dB 65536
threads  median_s       gflops       reps
1        0.432409252    19.866       5
```

### OPQ_Outer_UnrollJam4

| dA | dB | median time (s) | GFLOP/s |
|---|---|---|---|
| 128 | 128 | 0.000002326 | 14.309 |
| 256 | 256 | 0.000008649 | 15.273 |
| 512 | 512 | 0.000031700 | 16.604 |
| 1024 | 1024 | 0.000119529 | 17.579 |
| 2048 | 2048 | 0.000467511 | 17.961 |
| 4096 | 4096 | 0.001857879 | 18.069 |
| 8192 | 8192 | 0.007762632 | 17.294 |
| 16384 | 16384 | 0.030844327 | 17.408 |
| 32768 | 32768 | 0.122916076 | 17.472 |
| 65536 | 65536 | 0.491156922 | 17.490 |

### OPQ_Outer_UnrollJam8

| dA | dB | median time (s) | GFLOP/s |
|---|---|---|---|
| 128 | 128 | 0.000002958 | 11.252 |
| 256 | 256 | 0.000009279 | 14.236 |
| 512 | 512 | 0.000032963 | 15.968 |
| 1024 | 1024 | 0.000116103 | 18.098 |
| 2048 | 2048 | 0.000437233 | 19.204 |
| 4096 | 4096 | 0.001708525 | 19.649 |
| 8192 | 8192 | 0.006840783 | 19.625 |
| 16384 | 16384 | 0.027212176 | 19.731 |
| 32768 | 32768 | 0.108207100 | 19.847 |
| 65536 | 65536 | 0.432409252 | 19.866 |

## Blocked_UnrollJam8

#### Methodology

* **Variant:** `PolyMult_Blocked_UnrollJam8.c` — the outer-loop unroll-and-jam by 8 from `PolyMult_OPQ_Outer_UnrollJam8.c`, with the inner loop tiled over `b`. The tile loop is outermost: for each tile `b[j0 .. j0+block_size-1]`, every 8-row block of `a` accumulates its products into its output window `c[i+j0 .. i+j0+block_size+6]` (head / 8-row body / tail relative to the tile bounds), then the remaining $(d_A + 1) \bmod 8$ rows are applied to the same tile with plain OPQ. The intent is that the `b` slice and the slowly-sliding `c` window (≈ 2 × `block_size` floats) stay resident in L1d across all row blocks, reducing L1d ↔ L2 traffic.
* **Correctness gate:** `.check` and `.check-par` passed (oracle 42/42, gold vs GSQ 14/14) before timing. The gate calls the kernel with `block_size = 0`, so only the default tile size of 512 was verified.
* **Tile size:** default, 512 floats (2 KiB of `b`; ≈ 4 KiB working set against a 32 KiB L1d). No `--block-size` was passed.
* **Problem sizes:** $d_A = d_B = N = 2^n$, $n = 7 \ldots 16$.
* **Execution:** `.time` binary (`-O3 -march=native -ffast-math`, no `-fopenmp`), single thread pinned to core 16 (E-core) by `run_sweep.sh`.
* **Repetitions:** 5 per size, median reported.

```bash
$ ./run_sweep.sh Blocked_UnrollJam8 "1" 5 -dA 128 -dB 128
threads  median_s       gflops       reps
1        0.000003081    10.802       5
$ ./run_sweep.sh Blocked_UnrollJam8 "1" 5 -dA 256 -dB 256
threads  median_s       gflops       reps
1        0.000009307    14.193       5
$ ./run_sweep.sh Blocked_UnrollJam8 "1" 5 -dA 512 -dB 512
threads  median_s       gflops       reps
1        0.000035388    14.873       5
$ ./run_sweep.sh Blocked_UnrollJam8 "1" 5 -dA 1024 -dB 1024
threads  median_s       gflops       reps
1        0.000128733    16.323       5
$ ./run_sweep.sh Blocked_UnrollJam8 "1" 5 -dA 2048 -dB 2048
threads  median_s       gflops       reps
1        0.000506347    16.583       5
$ ./run_sweep.sh Blocked_UnrollJam8 "1" 5 -dA 4096 -dB 4096
threads  median_s       gflops       reps
1        0.002008141    16.717       5
$ ./run_sweep.sh Blocked_UnrollJam8 "1" 5 -dA 8192 -dB 8192
threads  median_s       gflops       reps
1        0.007961851    16.862       5
$ ./run_sweep.sh Blocked_UnrollJam8 "1" 5 -dA 16384 -dB 16384
threads  median_s       gflops       reps
1        0.031760179    16.906       5
$ ./run_sweep.sh Blocked_UnrollJam8 "1" 5 -dA 32768 -dB 32768
threads  median_s       gflops       reps
1        0.122294420    17.561       5
$ ./run_sweep.sh Blocked_UnrollJam8 "1" 5 -dA 65536 -dB 65536
threads  median_s       gflops       reps
1        0.488266241    17.593       5
```

### Blocked_UnrollJam8

| dA | dB | median time (s) | GFLOP/s |
|---|---|---|---|
| 128 | 128 | 0.000003081 | 10.802 |
| 256 | 256 | 0.000009307 | 14.193 |
| 512 | 512 | 0.000035388 | 14.873 |
| 1024 | 1024 | 0.000128733 | 16.323 |
| 2048 | 2048 | 0.000506347 | 16.583 |
| 4096 | 4096 | 0.002008141 | 16.717 |
| 8192 | 8192 | 0.007961851 | 16.862 |
| 16384 | 16384 | 0.031760179 | 16.906 |
| 32768 | 32768 | 0.122294420 | 17.561 |
| 65536 | 65536 | 0.488266241 | 17.593 |

## Large N: OPQ_Outer_UnrollJam8 vs Blocked_UnrollJam8

Same setup as above (`.time` binary, single thread pinned to core 16, default tile size 512 for the blocked variant, 5 repetitions, median reported), extended to $d_A = d_B = N = 2^n$, $n = 17 \ldots 20$.

```bash
$ ./run_sweep.sh OPQ_Outer_UnrollJam8 "1" 5 -dA 131072 -dB 131072
threads  median_s       gflops       reps
1        1.727606518    19.889       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam8 "1" 5 -dA 262144 -dB 262144
threads  median_s       gflops       reps
1        6.898443303    19.923       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam8 "1" 5 -dA 524288 -dB 524288
threads  median_s       gflops       reps
1        27.630761669   19.897       5
$ ./run_sweep.sh OPQ_Outer_UnrollJam8 "1" 5 -dA 1048576 -dB 1048576
threads  median_s       gflops       reps
1        109.954645772  19.999       5
$ ./run_sweep.sh Blocked_UnrollJam8 "1" 5 -dA 131072 -dB 131072
threads  median_s       gflops       reps
1        1.954156017    17.583       5
$ ./run_sweep.sh Blocked_UnrollJam8 "1" 5 -dA 262144 -dB 262144
threads  median_s       gflops       reps
1        7.810769143    17.596       5
$ ./run_sweep.sh Blocked_UnrollJam8 "1" 5 -dA 524288 -dB 524288
threads  median_s       gflops       reps
1        31.247598804   17.594       5
$ ./run_sweep.sh Blocked_UnrollJam8 "1" 5 -dA 1048576 -dB 1048576
threads  median_s       gflops       reps
1        125.039355251  17.587       5
```

| dA | dB | UnrollJam8 median time (s) | UnrollJam8 GFLOP/s | Blocked_UnrollJam8 median time (s) | Blocked_UnrollJam8 GFLOP/s |
|---|---|---|---|---|---|
| 131072 | 131072 | 1.727606518 | 19.889 | 1.954156017 | 17.583 |
| 262144 | 262144 | 6.898443303 | 19.923 | 7.810769143 | 17.596 |
| 524288 | 524288 | 27.630761669 | 19.897 | 31.247598804 | 17.594 |
| 1048576 | 1048576 | 109.954645772 | 19.999 | 125.039355251 | 17.587 |

## Block-size sweep: Blocked_UnrollJam8 at N = 2^16

Same setup as above (`.time` binary, single thread pinned to core 16, 5 repetitions, median reported), $d_A = d_B = 65536$, tile size passed with `--block-size`. Working set per tile ≈ 2 × block size floats (`b` slice + `c` window), against a 32 KiB L1d. The correctness gate verified only the default block size (512).

```bash
$ ./run_sweep.sh Blocked_UnrollJam8 "1" 5 -dA 65536 -dB 65536 --block-size 512
threads  median_s       gflops       reps
1        0.489037751    17.566       5
$ ./run_sweep.sh Blocked_UnrollJam8 "1" 5 -dA 65536 -dB 65536 --block-size 1024
threads  median_s       gflops       reps
1        0.444491278    19.326       5
$ ./run_sweep.sh Blocked_UnrollJam8 "1" 5 -dA 65536 -dB 65536 --block-size 2048
threads  median_s       gflops       reps
1        0.431938537    19.888       5
$ ./run_sweep.sh Blocked_UnrollJam8 "1" 5 -dA 65536 -dB 65536 --block-size 4096
threads  median_s       gflops       reps
1        0.427775750    20.081       5
$ ./run_sweep.sh Blocked_UnrollJam8 "1" 5 -dA 65536 -dB 65536 --block-size 8192
threads  median_s       gflops       reps
1        0.435485557    19.726       5
```

| Block size (floats) | Working set | median time (s) | GFLOP/s |
|---|---|---|---|
| 512 | ≈ 4 KiB | 0.489037751 | 17.566 |
| 1024 | ≈ 8 KiB | 0.444491278 | 19.326 |
| 2048 | ≈ 16 KiB | 0.431938537 | 19.888 |
| 4096 | ≈ 32 KiB | 0.427775750 | 20.081 |
| 8192 | ≈ 64 KiB | 0.435485557 | 19.726 |
| *Unblocked OPQ_Outer_UnrollJam8* | — | *0.432409252* | *19.866* |
