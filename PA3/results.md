# PA3

## Goals
1. Improve sequential performance of OPQ via loop strip-mining/blocking and AVX2 vectorization.
2. Improve concurrent performance of OPQ via cache-local tiling and thread-safe data decomposition.

---

## Single-Threaded Performance Experiments

Polynomial multiplication (1-D convolution) between vectors of degree $N$ requires $O(N^2)$ operations. 

### Microarchitectural Targets (Intel i9-13900K):
* **Vector Registers:** 16 YMM registers (256-bit AVX2) = 128 single-precision floats (512 bytes).
* **L1 Data Cache:** 
  * E-Core: 32 KiB L1d per core = 8,192 floats.
* **L2 Cache:** 4 MiB shared per 4-core E-module / 2 MiB per P-core.

### Experiment 1: Blocking

In the unblocked loop, as array size $N$ exceeds the L1 cache capacity, stream reads of $B$ and read/write updates to $C$ miss in the L1/L2 cache. Tiling the inner loop into blocks ($B_{\text{block}}$) keeps active working slices of array $B$ pinned in L1d cache while inner iterations leverage SIMD vector units.

#### Methodology:
1. **Baseline Measurement:** Benchmark sequential non-blocked OPQ across problem sizes $N \in [2^8, 2^{16}]$, recording GFLOP/s and median execution time.
2. **Block-Size Sweep:** Sweep tile sizes ($B_{\text{block}} \in \{16, 32, 64, 128, 256, 512, 1024, 2048\}$ floats) to identify the knee of the curve where L1 cache residency is maximized before performance plateaus or degrades.

#### Results: sequential non-blocked OPQ

```bash
$ ./run_sweep.sh OPQ "1" 5 -dA 256 -dB 256
threads  median_s       gflops       reps
1        0.000012304    10.736       5
$ ./run_sweep.sh OPQ "1" 5 -dA 512 -dB 512
threads  median_s       gflops       reps
1        0.000038969    13.507       5
$ ./run_sweep.sh OPQ "1" 5 -dA 1024 -dB 1024
threads  median_s       gflops       reps
1        0.000165670    12.683       5
$ ./run_sweep.sh OPQ "1" 5 -dA 2048 -dB 2048
threads  median_s       gflops       reps
1        0.000646616    12.986       5
$ ./run_sweep.sh OPQ "1" 5 -dA 4096 -dB 4096
threads  median_s       gflops       reps
1        0.002676469    12.543       5
$ ./run_sweep.sh OPQ "1" 5 -dA 8192 -dB 8192
threads  median_s       gflops       reps
1        0.014327268    9.370        5
$ ./run_sweep.sh OPQ "1" 5 -dA 16384 -dB 16384
threads  median_s       gflops       reps
1        0.057098344    9.404        5
$ ./run_sweep.sh OPQ "1" 5 -dA 32768 -dB 32768
threads  median_s       gflops       reps
1        0.228025619    9.418        5
$ ./run_sweep.sh OPQ "1" 5 -dA 65536 -dB 65536
threads  median_s       gflops       reps
1        0.910392270    9.436        5
```

| $N$ ($d_A = d_B$) | Median time (s) | GFLOP/s |
|---:|---:|---:|
| $2^{8}$ = 256 | 0.000012304 | 10.736 |
| $2^{9}$ = 512 | 0.000038969 | 13.507 |
| $2^{10}$ = 1,024 | 0.000165670 | 12.683 |
| $2^{11}$ = 2,048 | 0.000646616 | 12.986 |
| $2^{12}$ = 4,096 | 0.002676469 | 12.543 |
| $2^{13}$ = 8,192 | 0.014327268 | 9.370 |
| $2^{14}$ = 16,384 | 0.057098344 | 9.404 |
| $2^{15}$ = 32,768 | 0.228025619 | 9.418 |
| $2^{16}$ = 65,536 | 0.910392270 | 9.436 |

#### Results: Block-size sweep: `PolyMult_Blocked_Single.c`

The sequential program has a change in median and GFLOP/s depending upon the input dimensions.

If input is sufficiently large, blocking may be used to improve GFLOP/s and reduce median time across trials because of more prosimal data locality will reduce the memory bound bottlenec.

This experiment uses polynomial size options of 8192 and 16384 becuase given the cpu's L1 size, these values will straddle non blocked input sizes that might result in thrashing.

Trhough using vaious block sizes, we aim to identify how well blocking improves sequential performance across a range of values.


```bash
$ ./run_sweep.sh Blocked_Single "1" 5 -dA 8192 -dB 8192 --block-size 16
threads  median_s       gflops       reps
1        0.049850892    2.693        5
$ ./run_sweep.sh Blocked_Single "1" 5 -dA 8192 -dB 8192 --block-size 32
threads  median_s       gflops       reps
1        0.025893393    5.185        5
$ ./run_sweep.sh Blocked_Single "1" 5 -dA 8192 -dB 8192 --block-size 64
threads  median_s       gflops       reps
1        0.014857988    9.036        5
$ ./run_sweep.sh Blocked_Single "1" 5 -dA 8192 -dB 8192 --block-size 128
threads  median_s       gflops       reps
1        0.010618180    12.643       5
$ ./run_sweep.sh Blocked_Single "1" 5 -dA 8192 -dB 8192 --block-size 256
threads  median_s       gflops       reps
1        0.010285208    13.053       5
$ ./run_sweep.sh Blocked_Single "1" 5 -dA 8192 -dB 8192 --block-size 512
threads  median_s       gflops       reps
1        0.010084378    13.313       5
$ ./run_sweep.sh Blocked_Single "1" 5 -dA 8192 -dB 8192 --block-size 1024
threads  median_s       gflops       reps
1        0.010537064    12.741       5
$ ./run_sweep.sh Blocked_Single "1" 5 -dA 8192 -dB 8192 --block-size 2048
threads  median_s       gflops       reps
1        0.010291826    13.044       5
$ ./run_sweep.sh Blocked_Single "1" 5 -dA 16384 -dB 16384 --block-size 16
threads  median_s       gflops       reps
1        0.199388759    2.693        5
$ ./run_sweep.sh Blocked_Single "1" 5 -dA 16384 -dB 16384 --block-size 32
threads  median_s       gflops       reps
1        0.103238649    5.201        5
$ ./run_sweep.sh Blocked_Single "1" 5 -dA 16384 -dB 16384 --block-size 64
threads  median_s       gflops       reps
1        0.060050612    8.941        5
$ ./run_sweep.sh Blocked_Single "1" 5 -dA 16384 -dB 16384 --block-size 128
threads  median_s       gflops       reps
1        0.042555708    12.617       5
$ ./run_sweep.sh Blocked_Single "1" 5 -dA 16384 -dB 16384 --block-size 256
threads  median_s       gflops       reps
1        0.041001878    13.095       5
$ ./run_sweep.sh Blocked_Single "1" 5 -dA 16384 -dB 16384 --block-size 512
threads  median_s       gflops       reps
1        0.040339786    13.310       5
$ ./run_sweep.sh Blocked_Single "1" 5 -dA 16384 -dB 16384 --block-size 1024
threads  median_s       gflops       reps
1        0.042040014    12.772       5
$ ./run_sweep.sh Blocked_Single "1" 5 -dA 16384 -dB 16384 --block-size 2048
threads  median_s       gflops       reps
1        0.040963579    13.108       5
```

| Block size (floats) | $N$ = 8,192: median time (s) | $N$ = 8,192: GFLOP/s | $N$ = 16,384: median time (s) | $N$ = 16,384: GFLOP/s |
|---:|---:|---:|---:|---:|
| 16 | 0.049850892 | 2.693 | 0.199388759 | 2.693 |
| 32 | 0.025893393 | 5.185 | 0.103238649 | 5.201 |
| 64 | 0.014857988 | 9.036 | 0.060050612 | 8.941 |
| 128 | 0.010618180 | 12.643 | 0.042555708 | 12.617 |
| 256 | 0.010285208 | 13.053 | 0.041001878 | 13.095 |
| 512 | 0.010084378 | 13.313 | 0.040339786 | 13.310 |
| 1024 | 0.010537064 | 12.741 | 0.042040014 | 12.772 |
| 2048 | 0.010291826 | 13.044 | 0.040963579 | 13.108 |
| *Unblocked OPQ baseline* | *0.014327268* | *9.370* | *0.057098344* | *9.404* |

these results show that naive blocking successfully mitigates L1 cache thrashing for large problem sizes, improving performance from the unblocked baseline of about 9.4 GFLOP/s up to a peak of 13.3 GFLOP/s at a block size of 512 floats.

Results for each block size is as follows:
- Small (16-64): underperformed compared to the baseline because the overhead of managing small tiles outweighs the locality benefits, and vectorization / instruciton levle parallelism are constrained
- Optimal (512 floats): This appears to be ideal through fitting active subsets of array b and the sliding window of c comfortablty instide the core's 32 KiB l1 data cache alongside overhead and register state.
- Over Blocked(1024 - 2048): This block size range shows a slight plateau or minor regression as working sets push closer to L1 capacity limits, increasing pressure on cache replacement policies 

##### Outer-loop unroll-and-jam: `PolyMult_OPQ_Outer_UnrollJam4.c`, `PolyMult_OPQ_Outer_UnrollJam8.c`

Non-blocked OPQ with the outer `i` loop unrolled by 4 and by 8 and the resulting inner loops jammed into one: each pass over the output window updates `c[i+t]` with the products from all 4 / 8 rows (`a[i+r] * b[t-r]`), followed by head/tail loops for the partial-row edges of each block and a plain OPQ loop for the remaining $(d_A + 1) \bmod 4$ / $\bmod 8$ rows. Swept like the baseline: `.time` binary (no `-fopenmp`), pinned to core 16 (E-core), $d_A = d_B = N$, 5 repetitions per size, median reported.

**Unroll-and-jam by 4:**

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
```

**Unroll-and-jam by 8:**

```bash
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

| $N$ ($d_A = d_B$) | Baseline time (s) | Baseline GFLOP/s | UnrollJam 4 time (s) | UnrollJam 4 GFLOP/s | UnrollJam 8 time (s) | UnrollJam 8 GFLOP/s |
|---:|---:|---:|---:|---:|---:|---:|
| $2^{7}$ = 128 | — | — | 0.000002326 | 14.309 | 0.000002958 | 11.252 |
| $2^{8}$ = 256 | 0.000012304 | 10.736 | 0.000008649 | 15.273 | 0.000009279 | 14.236 |
| $2^{9}$ = 512 | 0.000038969 | 13.507 | 0.000031700 | 16.604 | 0.000032963 | 15.968 |
| $2^{10}$ = 1,024 | 0.000165670 | 12.683 | 0.000119529 | 17.579 | 0.000116103 | 18.098 |
| $2^{11}$ = 2,048 | 0.000646616 | 12.986 | 0.000467511 | 17.961 | 0.000437233 | 19.204 |
| $2^{12}$ = 4,096 | 0.002676469 | 12.543 | 0.001857879 | 18.069 | 0.001708525 | 19.649 |
| $2^{13}$ = 8,192 | 0.014327268 | 9.370 | 0.007762632 | 17.294 | 0.006840783 | 19.625 |
| $2^{14}$ = 16,384 | 0.057098344 | 9.404 | 0.030844327 | 17.408 | 0.027212176 | 19.731 |
| $2^{15}$ = 32,768 | 0.228025619 | 9.418 | 0.122916076 | 17.472 | 0.108207100 | 19.847 |
| $2^{16}$ = 65,536 | 0.910392270 | 9.436 | 0.491156922 | 17.490 | 0.432409252 | 19.866 |

Through the usage of Unroll Jam 8 on an outer product operation of two equal sized 1 dimensional arrays of size 2^16 holding elements of type float, we reach about 56% of the Theoretical Peak Performance on a single thread. 


The Results so far indicates the process of performance optimization is to start closest to the CPU registers and work your way out to L1 data cache and L2 cache. So far we've optimized the register level using SIMD instructions to essentially do more work in a single clock cycle. We then optimized the boundary between CPU register and L1 data cache through maxamize cache hits. Doing this prevents wasting clock cycles waiting for data to eventually reach the CPU registers. This was done through blocking. Moving forward, we optimize the flow of data between the L1 data cache and the L2 cache through blocking again.

##### Blocked and Unroll Jam8: `PolyMult_Blocked_Unrolljam8.c`

#### Methodology

* **Variant:** `PolyMult_Blocked_UnrollJam8.c` — the outer-loop unroll-and-jam by 8 from `PolyMult_OPQ_Outer_UnrollJam8.c`, with the inner loop tiled over `b`. The tile loop is outermost: for each tile `b[j0 .. j0+block_size-1]`, every 8-row block of `a` accumulates its products into its output window `c[i+j0 .. i+j0+block_size+6]` (head / 8-row body / tail relative to the tile bounds), then the remaining $(d_A + 1) \bmod 8$ rows are applied to the same tile with plain OPQ. The intent is that the `b` slice and the slowly-sliding `c` window (≈ 2 × `block_size` floats) stay resident in L1d across all row blocks, reducing L1d ↔ L2 traffic.
* **Problem sizes:** $d_A = d_B = N = 2^n$, $n = 7 \ldots 16$.
* **Repetitions:** 5 per size, median reported.

#### Results

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

| $N$ ($d_A = d_B$) | Baseline time (s) | Baseline GFLOP/s | UnrollJam 8 time (s) | UnrollJam 8 GFLOP/s | Blocked UnrollJam 8 time (s) | Blocked UnrollJam 8 GFLOP/s |
|---:|---:|---:|---:|---:|---:|---:|
| $2^{7}$ = 128 | — | — | 0.000002958 | 11.252 | 0.000003081 | 10.802 |
| $2^{8}$ = 256 | 0.000012304 | 10.736 | 0.000009279 | 14.236 | 0.000009307 | 14.193 |
| $2^{9}$ = 512 | 0.000038969 | 13.507 | 0.000032963 | 15.968 | 0.000035388 | 14.873 |
| $2^{10}$ = 1,024 | 0.000165670 | 12.683 | 0.000116103 | 18.098 | 0.000128733 | 16.323 |
| $2^{11}$ = 2,048 | 0.000646616 | 12.986 | 0.000437233 | 19.204 | 0.000506347 | 16.583 |
| $2^{12}$ = 4,096 | 0.002676469 | 12.543 | 0.001708525 | 19.649 | 0.002008141 | 16.717 |
| $2^{13}$ = 8,192 | 0.014327268 | 9.370 | 0.006840783 | 19.625 | 0.007961851 | 16.862 |
| $2^{14}$ = 16,384 | 0.057098344 | 9.404 | 0.027212176 | 19.731 | 0.031760179 | 16.906 |
| $2^{15}$ = 32,768 | 0.228025619 | 9.418 | 0.108207100 | 19.847 | 0.122294420 | 17.561 |
| $2^{16}$ = 65,536 | 0.910392270 | 9.436 | 0.432409252 | 19.866 | 0.488266241 | 17.593 |

Despite usage of blocing and unroll jamming, were're still under 60% TPP. 

---

### 2. Multi-Threaded Performance & Locality

#### Rationale for Threaded Block Size:
In a multi-threaded environment, block size selection depends on both cache hierarchy and thread mapping:
* **Cache Contention:** Multiple threads running on E-cores share a single 4 MiB L2 cache module. Tile sizes must be scaled down per thread to fit simultaneously without cache line thrashing.
* **False Sharing:** Chunk sizes assigned to individual threads must be padded/aligned to 64-byte cache lines (16 single-precision floats) to eliminate cache line invalidation ping-ponging on output array $C$.

#### Experimental Methodology:
1. Benchmark performance across thread counts (1 to 32 threads) on fixed problem sizes.
2. Compare static chunk allocation vs. dynamic cache-local thread workloads to find the optimal tile size per thread.