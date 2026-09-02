
## Stage 1

```bash
blowfish:~/cs475/Lab1 make all
make: Nothing to be done for 'all'.
blowfish:~/cs475/Lab1 OMP_NUM_T
```

## Stage 2

[insert image here]

Expensive rows are concentrated nearest to the vertical center of the image. The vertical center requires more iterations to break from the mandel_point funciton's where loop compared to the edges of the image.

## Stage 3

## Stage 4

//Justify the shape you drew using your own Stage 3 numbers: at what p does imbalance start to dominate, and why?

Part A

[insert s4a]

A ratio of 1.0 indicates perfect balance, where computational work is divided across all p threads. As |1 - ratio| increases, load imbalence worsens, meaning the overall execution time is bounded by the slowest thread.

For the default vidwing window, load imbalance begins to dominate significantly around p = 8 (where the max/avg ratio reaches 3.325).

Because the expensive Mandelbrot set points are concentrated in the central rows of the image (y=0), a static contiguous row split assigns the heavy central block to a small subset of middle threads while top and bottom threads finish almost instantly.

p = 1 to p = 6: Speedup increases nearly linearly because each thread block contains a mix of inner and outer region rows, keeping the max/avg ratio relatively low.

p = 8 to p = 24: As p increases, the contiguous block size drops (2400 / p rows per thread). The central thread's workload shrinks much slower than the average workload, driving the max/avg ratio up to [insert p=24 ratio].

p >= 24: The speedup curve completely flatlines. The latency of the single busiest thread holding the central rows dominates execution time, rendering additional idle threads ineffective.


Part B

[insert s4b]


If all computationally expensive pixels are concentrated in the first 10% of rows (rows 0 through 239 out of 2400), a static contiguous row split yields extreme imbalance at higher thread counts:

For p <= 10: The top 10% of rows fall entirely inside Thread 0's block (2400 / 10 = 240 rows per thread). Thread 0 does 100% of the expensive computation while Threads 1 through p-1 process cheap background rows in near-zero time. Consequently, increasing p from 1 to 10 yields zero speedup (Speedup = 1.0).

For p > 10: The block size drops below 240 rows (e.g., at p = 20, block size is 120 rows), spreading the expensive 240 rows across multiple threads (e.g., Thread 0 and Thread 1). Only then does speedup begin to scale.