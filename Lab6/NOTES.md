## Profiling

# 1. Set up. Your home directory is shared across lab machines, so your files are already there.
module load cuda/12.8
nvidia-smi                                   # confirm 0% use and no processes
cd ~/cs475/Lab6/CUDAL6
make clean && make                           # rebuild both programs

# 2. Plain timed runs, without the profiler
./sgemm00 4096
./sgemm01 4096

# 3. Record a timeline for each program
nsys profile -o sgemm00 --force-overwrite true ./sgemm00 4096
nsys profile -o sgemm01 --force-overwrite true ./sgemm01 4096

# 4. Print the summary tables
nsys stats --report nvtx_sum,cuda_api_sum,cuda_gpu_kern_sum,cuda_gpu_mem_time_sum,cuda_gpu_mem_size_sum sgemm00.nsys-rep
nsys stats --report nvtx_sum,cuda_api_sum,cuda_gpu_kern_sum,cuda_gpu_mem_time_sum,cuda_gpu_mem_size_sum sgemm01.nsys-rep

Why each step

- nvidia-smi first: the GPU is shared. Earlier on anchovy, another user's job had it at 100%, which makes both your printed times and the nsys numbers unreliable.
- make clean && make: this makes sure both programs are built from your current source on that machine.
- Plain runs before profiling: Question 1 asks whether the profiler's kernel times agree with the times the programs print. You need the printed times from runs without the profiler to compare against.
- nsys profile: this runs the program and records everything it does on the CPU and GPU into a .nsys-rep file.
  - -o names the file.
  - --force-overwrite true is needed because an old sgemm00.nsys-rep already exists.
- nsys stats: this turns the recording into tables. All times are in nanoseconds. Each report answers part of the questions:

┌──────────────────────────────────────┬──────────────────────────────────────────────────────────────────────┬────────────────────────────────────────────────────────────────────────┐
│                Report                │                            What it shows                             │                                Question                                │
├──────────────────────────────────────┼──────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────────────────┤
│ cuda_gpu_kern_sum                    │ How long MatMulKernel runs on the GPU. There are 2 instances: the    │ Q1: kernel times and the speedup of the tiled kernel                   │
│                                      │ warm-up run and the timed run.                                       │                                                                        │
├──────────────────────────────────────┼──────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────────────────┤
│ cuda_gpu_mem_time_sum and            │ Time taken and bytes moved for the copies of A and B to the GPU and  │ Q2: transfer rate in GB/s, and copy time compared with the kernel      │
│ cuda_gpu_mem_size_sum                │ C back                                                               │                                                                        │
├──────────────────────────────────────┼──────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────────────────┤
│ cuda_api_sum                         │ Time the CPU spends inside each CUDA call                            │ Q3: why cudaDeviceSynchronize is slow and cudaLaunchKernel is fast,    │
│                                      │                                                                      │ and why the first cudaMalloc is slow                                   │
├──────────────────────────────────────┼──────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────────────────┤
│ nvtx_sum                             │ Time for each phase that sgemm.cu marks: initialize A and B, copy,   │ Q4: which phase dominates, and how much a 2× faster kernel would       │
│                                      │ warm-up, timed kernel, copy back, check                              │ actually help                                                          │
└──────────────────────────────────────┴──────────────────────────────────────────────────────────────────────┴────────────────────────────────────────────────────────────────────────┘

If you're sitting at the lab machine, you can also run nsys-ui sgemm01.nsys-rep to see the timeline, but it's optional. You can answer all the questions from the tables.


## Results

### Provided Matrix Mult Kernel

```bash
perch:~/cs475/Lab6/CUDAL6$ nsys stats --report nvtx_sum,cuda_api_sum,cuda_gpu_kern_sum,cuda_gpu_mem_time_sum,cuda_gpu_mem_size_sum sgemm00.nsys-rep
Generating SQLite file sgemm00.sqlite from sgemm00.nsys-rep
Processing [sgemm00.sqlite] with [/usr/local/cuda/12.8/nsight-systems-2024.6.2/host-linux-x64/reports/nvtx_sum.py]... 

 ** NVTX Range Summary (nvtx_sum):

 Time (%)  Total Time (ns)  Instances    Avg (ns)       Med (ns)      Min (ns)     Max (ns)    StdDev (ns)   Style              Range           
 --------  ---------------  ---------  -------------  -------------  -----------  -----------  -----------  -------  ---------------------------
     50.0      238,773,966          1  238,773,966.0  238,773,966.0  238,773,966  238,773,966          0.0  PushPop  :Check C on host           
     20.0       95,747,666          1   95,747,666.0   95,747,666.0   95,747,666   95,747,666          0.0  PushPop  :Timed kernel              
     20.0       95,590,561          1   95,590,561.0   95,590,561.0   95,590,561   95,590,561          0.0  PushPop  :Warm-up kernel            
      5.5       26,373,501          1   26,373,501.0   26,373,501.0   26,373,501   26,373,501          0.0  PushPop  :Initialize A and B on host
      2.4       11,338,292          1   11,338,292.0   11,338,292.0   11,338,292   11,338,292          0.0  PushPop  :Copy C to host            
      2.1       10,156,278          1   10,156,278.0   10,156,278.0   10,156,278   10,156,278          0.0  PushPop  :Copy A and B to GPU       

Processing [sgemm00.sqlite] with [/usr/local/cuda/12.8/nsight-systems-2024.6.2/host-linux-x64/reports/cuda_api_sum.py]... 

 ** CUDA API Summary (cuda_api_sum):

 Time (%)  Total Time (ns)  Num Calls    Avg (ns)      Med (ns)     Min (ns)    Max (ns)    StdDev (ns)            Name         
 --------  ---------------  ---------  ------------  ------------  ----------  -----------  ------------  ----------------------
     51.7      191,194,184          2  95,597,092.0  95,597,092.0  95,453,962   95,740,222     202,416.4  cudaDeviceSynchronize 
     28.0      103,612,764          3  34,537,588.0      51,069.0      40,411  103,521,284  59,741,633.4  cudaMalloc            
     13.6       50,326,569          1  50,326,569.0  50,326,569.0  50,326,569   50,326,569           0.0  cudaDeviceReset       
      5.8       21,486,558          3   7,162,186.0   5,133,634.0   5,015,639   11,337,285   3,616,223.1  cudaMemcpy            
      0.7        2,724,074          3     908,024.7   1,100,470.0     192,682    1,430,922     641,159.9  cudaFree              
      0.0          139,059          2      69,529.5      69,529.5       5,987      133,072      89,862.7  cudaLaunchKernel      
      0.0            8,297          1       8,297.0       8,297.0       8,297        8,297           0.0  cuCtxSynchronize      
      0.0              971          1         971.0         971.0         971          971           0.0  cuModuleGetLoadingMode

Processing [sgemm00.sqlite] with [/usr/local/cuda/12.8/nsight-systems-2024.6.2/host-linux-x64/reports/cuda_gpu_kern_sum.py]... 

 ** CUDA GPU Kernel Summary (cuda_gpu_kern_sum):

 Time (%)  Total Time (ns)  Instances    Avg (ns)      Med (ns)     Min (ns)    Max (ns)   StdDev (ns)                            Name                          
 --------  ---------------  ---------  ------------  ------------  ----------  ----------  -----------  --------------------------------------------------------
    100.0      191,192,649          2  95,596,324.5  95,596,324.5  95,454,067  95,738,582    201,182.5  MatMulKernel(const float *, const float *, float *, int)

Processing [sgemm00.sqlite] with [/usr/local/cuda/12.8/nsight-systems-2024.6.2/host-linux-x64/reports/cuda_gpu_mem_time_sum.py]... 

 ** CUDA GPU MemOps Summary (by Time) (cuda_gpu_mem_time_sum):

 Time (%)  Total Time (ns)  Count    Avg (ns)      Med (ns)     Min (ns)    Max (ns)   StdDev (ns)           Operation          
 --------  ---------------  -----  ------------  ------------  ----------  ----------  -----------  ----------------------------
     51.1       10,451,005      1  10,451,005.0  10,451,005.0  10,451,005  10,451,005          0.0  [CUDA memcpy Device-to-Host]
     48.9       10,012,859      2   5,006,429.5   5,006,429.5   4,933,181   5,079,678    103,589.0  [CUDA memcpy Host-to-Device]

Processing [sgemm00.sqlite] with [/usr/local/cuda/12.8/nsight-systems-2024.6.2/host-linux-x64/reports/cuda_gpu_mem_size_sum.py]... 

 ** CUDA GPU MemOps Summary (by Size) (cuda_gpu_mem_size_sum):

 Total (MB)  Count  Avg (MB)  Med (MB)  Min (MB)  Max (MB)  StdDev (MB)           Operation          
 ----------  -----  --------  --------  --------  --------  -----------  ----------------------------
    134.218      2    67.109    67.109    67.109    67.109        0.000  [CUDA memcpy Host-to-Device]
     67.109      1    67.109    67.109    67.109    67.109        0.000  [CUDA memcpy Device-to-Host]
```

Problem size: each matrix copy is 67.109 MB, which is 4096 × 4096 floats, so N = 4096.

Kernel (MatMulKernel)
- It ran twice: once as the warm-up and once timed. Each run took about 95.6 ms, and the two differ by only about 0.3 ms.
- The work is 2N³ ≈ 1.37 × 10¹¹ FLOPs, which comes to about 1.44 TFLOP/s.
- On the GPU timeline, kernel time (191 ms total) is about 9× the memory-copy time (about 20.5 ms).

Memory transfers
- Host to device: 134 MB (A and B) in 10.0 ms, about 13.4 GB/s.
- Device to host: 67 MB (C) in 10.45 ms, about 6.4 GB/s.
- Your host buffers are probably regular (pageable) memory from malloc. That would explain the slower device-to-host copy; pinned memory from cudaMallocHost is usually faster.

Where the CPU time goes
- cudaDeviceSynchronize (191 ms) is just the CPU waiting for the two kernels to finish. It matches the kernel time almost exactly.
- One cudaMalloc call took about 103 ms. That's the one-time cost of starting up the CUDA context, not the allocation itself; the other two calls took about 50 µs each.
- cudaDeviceReset took 50 ms of teardown.
- The single biggest NVTX range is "Check C on host" at 239 ms. Checking the result on the CPU takes about 2.5× as long as the kernel itself


### Tiled Matrix Mult Kernel

```bash
perch:~/cs475/Lab6/CUDAL6$ nsys stats --report nvtx_sum,cuda_api_sum,cuda_gpu_kern_sum,cuda_gpu_mem_time_sum,cuda_gpu_mem_size_sum sgemm01.nsys-rep
Generating SQLite file sgemm01.sqlite from sgemm01.nsys-rep
Processing [sgemm01.sqlite] with [/usr/local/cuda/12.8/nsight-systems-2024.6.2/host-linux-x64/reports/nvtx_sum.py]... 

 ** NVTX Range Summary (nvtx_sum):

 Time (%)  Total Time (ns)  Instances    Avg (ns)       Med (ns)      Min (ns)     Max (ns)    StdDev (ns)   Style              Range           
 --------  ---------------  ---------  -------------  -------------  -----------  -----------  -----------  -------  ---------------------------
     51.3      200,228,382          1  200,228,382.0  200,228,382.0  200,228,382  200,228,382          0.0  PushPop  :Check C on host           
     18.4       72,051,222          1   72,051,222.0   72,051,222.0   72,051,222   72,051,222          0.0  PushPop  :Warm-up kernel            
     18.3       71,454,258          1   71,454,258.0   71,454,258.0   71,454,258   71,454,258          0.0  PushPop  :Timed kernel              
      6.5       25,525,543          1   25,525,543.0   25,525,543.0   25,525,543   25,525,543          0.0  PushPop  :Initialize A and B on host
      2.9       11,141,039          1   11,141,039.0   11,141,039.0   11,141,039   11,141,039          0.0  PushPop  :Copy C to host            
      2.6       10,171,618          1   10,171,618.0   10,171,618.0   10,171,618   10,171,618          0.0  PushPop  :Copy A and B to GPU       

Processing [sgemm01.sqlite] with [/usr/local/cuda/12.8/nsight-systems-2024.6.2/host-linux-x64/reports/cuda_api_sum.py]... 

 ** CUDA API Summary (cuda_api_sum):

 Time (%)  Total Time (ns)  Num Calls    Avg (ns)      Med (ns)     Min (ns)    Max (ns)   StdDev (ns)            Name         
 --------  ---------------  ---------  ------------  ------------  ----------  ----------  ------------  ----------------------
     47.0      143,372,868          2  71,686,434.0  71,686,434.0  71,444,143  71,928,725     342,651.2  cudaDeviceSynchronize 
     28.8       87,845,408          3  29,281,802.7      47,075.0      43,964  87,754,369  50,638,727.9  cudaMalloc            
     16.4       50,161,779          1  50,161,779.0  50,161,779.0  50,161,779  50,161,779           0.0  cudaDeviceReset       
      7.0       21,303,953          3   7,101,317.7   5,103,577.0   5,060,839  11,139,537   3,497,265.8  cudaMemcpy            
      0.8        2,288,794          3     762,931.3   1,050,082.0     144,837   1,093,875     535,733.1  cudaFree              
      0.0          127,089          2      63,544.5      63,544.5       8,336     118,753      78,076.6  cudaLaunchKernel      
      0.0            7,287          1       7,287.0       7,287.0       7,287       7,287           0.0  cuCtxSynchronize      
      0.0              959          1         959.0         959.0         959         959           0.0  cuModuleGetLoadingMode

Processing [sgemm01.sqlite] with [/usr/local/cuda/12.8/nsight-systems-2024.6.2/host-linux-x64/reports/cuda_gpu_kern_sum.py]... 

 ** CUDA GPU Kernel Summary (cuda_gpu_kern_sum):

 Time (%)  Total Time (ns)  Instances    Avg (ns)      Med (ns)     Min (ns)    Max (ns)   StdDev (ns)                            Name                          
 --------  ---------------  ---------  ------------  ------------  ----------  ----------  -----------  --------------------------------------------------------
    100.0      143,376,756          2  71,688,378.0  71,688,378.0  71,442,921  71,933,835    347,128.6  MatMulKernel(const float *, const float *, float *, int)

Processing [sgemm01.sqlite] with [/usr/local/cuda/12.8/nsight-systems-2024.6.2/host-linux-x64/reports/cuda_gpu_mem_time_sum.py]... 

 ** CUDA GPU MemOps Summary (by Time) (cuda_gpu_mem_time_sum):

 Time (%)  Total Time (ns)  Count    Avg (ns)      Med (ns)     Min (ns)    Max (ns)   StdDev (ns)           Operation          
 --------  ---------------  -----  ------------  ------------  ----------  ----------  -----------  ----------------------------
     50.5       10,224,349      1  10,224,349.0  10,224,349.0  10,224,349  10,224,349          0.0  [CUDA memcpy Device-to-Host]
     49.5       10,036,284      2   5,018,142.0   5,018,142.0   4,986,462   5,049,822     44,802.3  [CUDA memcpy Host-to-Device]

Processing [sgemm01.sqlite] with [/usr/local/cuda/12.8/nsight-systems-2024.6.2/host-linux-x64/reports/cuda_gpu_mem_size_sum.py]... 

 ** CUDA GPU MemOps Summary (by Size) (cuda_gpu_mem_size_sum):

 Total (MB)  Count  Avg (MB)  Med (MB)  Min (MB)  Max (MB)  StdDev (MB)           Operation          
 ----------  -----  --------  --------  --------  --------  -----------  ----------------------------
    134.218      2    67.109    67.109    67.109    67.109        0.000  [CUDA memcpy Host-to-Device]
     67.109      1    67.109    67.109    67.109    67.109        0.000  [CUDA memcpy Device-to-Host]
```


The tiled kernel is about 1.33× faster than the naive one: about 71.7 ms per run instead of 95.6 ms. That's a real improvement, but smaller than tiling's theoretical reduction in global-memory reads would suggest.

┌─────────────────────────────┬─────────────────┬─────────────────┐
│                             │ Naive (sgemm00) │ Tiled (sgemm01) │
├─────────────────────────────┼─────────────────┼─────────────────┤
│ Kernel time (avg of 2)      │ 95.6 ms         │ 71.7 ms         │
├─────────────────────────────┼─────────────────┼─────────────────┤
│ Throughput (2N³ / time)     │ ~1.44 TFLOP/s   │ ~1.92 TFLOP/s   │
├─────────────────────────────┼─────────────────┼─────────────────┤
│ Spread between the two runs │ 0.3 ms          │ 0.5 ms          │
├─────────────────────────────┼─────────────────┼─────────────────┤
│ Host↔device copies          │ 20.5 ms         │ 20.3 ms         │
├─────────────────────────────┼─────────────────┼─────────────────┤
│ Kernel share of GPU time    │ ~82%            │ ~88%            │
└─────────────────────────────┴─────────────────┴─────────────────┘

What stayed the same
- Data movement is identical: still 134 MB to the GPU and 67 MB back, at the same speeds. That makes sense, because tiling only changes what happens inside the kernel.
- The CPU-side overhead is the same. The first cudaMalloc still pays the context-startup cost, and cudaDeviceReset still takes about 50 ms.
- "Check C on host" went from 239 to 200 ms. That's ordinary CPU timing variation, not something the tiling changed.

Why the gain is only 1.33×
- With a tile width of T, tiling cuts each thread's global-memory reads by about a factor of T. But the naive kernel wasn't paying the full cost of those reads to begin with. Modern GPUs keep recently read data in L1/L2 caches, and neighbouring threads read the same rows of A and columns of B, so many of the naive kernel's reads were already served from cache.
- In the tiled kernel each thread still computes one output element. Its inner loop does two shared-memory loads for every multiply-add, so the limit is now shared-memory bandwidth and the number of instructions, not DRAM.
- __syncthreads() at every tile step also leaves some threads waiting.