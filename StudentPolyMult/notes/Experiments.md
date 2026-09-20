sequential algo is __ fast.

it uses ___

## Experiment 1: parallization of inner for loop?

Hypothesis:

If the inner for loop is parallized, there would be a marginal gain - if any in speedup becuase there would be a lot of reoccuring overhead from creating and scheduling threads. This overhead would negate improvements to speedup.

Observations:

## Experimaent 2: parallization fo outer for loop?

Hypothesis:

If the outer for loop is paralllized (and we allocate ), there would be a better speedup because there would be a significant amount of work being done amungst threads

Observation:

speedup is exceeds linear until thread count passes 8

possible explanations?

- the process of creating private copies of the ouptut c so threads can accumulate independently then reducing those copies into a final polynomial "c"

possible approaches?

- parallelize the reduction step
- experiment with a better scheduling strategy
- use a contiguous block for all provate arrays

## Experiment 3: parallization of reduction step && static chunks

observations:
1. for each thread count, speedup was worse

## Experiment 4: thread per anti diagonal

better speedup, but still less than linear.

with anti diagonal approach, don't forget load is no longer uniform...


## Experiment 4: thread per anti diagonal - dynamic schedule (16)

the alternative is 32 but im unsure which would be better

---
Question 3: 

on a single core, achive the Theoretical Peak Program of (~35GFLOPS). Vector units form an impotant part of the processor's parallelism, so call this variant "OPP2" even though its on a single core. [prepare to be disappointed]

35.2 GFLOPS is derived from the usage of an E-core

The E-core's TPP is 35.2 GFLOPS is possible if the 256-bit vector FMA is used.

256-bit vector FMA is where the core allows for 8 simultaneous FP32 mutilply add operations  per instruction. (256bit = 8 * FP32)

Each FMA counts as 2 FLOPs because there's 2 operations: multiply and add

2 FLOPS * 8 Operatios per instruction = 16 FLOPs per instruction

Given an E-core can run 2.2GHz, the E-core has a TPP of 35.2 GFLOPs

This mean for our program to reach the TPP, it would need to use the E-core's 8 simultaneous FP32 multiply add operation

the objdump command display the binaries compiled from the source code

given there's often a lot, the grep command's extended regex for the or operator can be used to filter identify which vector operation is configured: 

```bash
Bash(cd /s/bach/m/under/C837194065/cs475/StudentPolyMult/01-OPP2 && objdump -d bin/PolyMult.time | grep -E 'vfmadd|vfnmadd|vmulp|vaddp|vmulss|vmulsd|vaddss|vaddsd|ymm' )
  401285:  c5 fa 59 44 24 08       vmulss 0x8(%rsp),%xmm0,%xmm0
  4012c5:  c5 fa 59 44 24 08       vmulss 0x8(%rsp),%xmm0,%xmm0
  4012f3:  c5 fb 59 f9             vmulsd %xmm1,%xmm0,%xmm7
  40131a:  c5 c3 59 3d 0e 22 00    vmulsd 0x220e(%rip),%xmm7,%xmm7        # 403530 <__dso_handle+0x528>
  4013cb:  c4 e2 e1 bd 4c 24 10    vfnmadd231sd 0x10(%rsp),%xmm3,%xmm1
  4013e0:  c5 fb 58 c1             vaddsd %xmm1,%xmm0,%xmm0
  40169c:  c4 e2 69 b9 02          vfmadd231ss (%rdx),%xmm2,%xmm0

```
given these results, we can see the suffix "ss" and "sd" appear.

ss = Scalar Single-precision = 1 x 32-bit fload
sd = Scalar Double-precision = 1 x 64-bit double
pd = Packed Double-precision = Multiple 32-bit floats
ps = Packed Single-precision = Multiple 64-bit doubles

Modify makefile to ouput a missed.txt with the compiler's logs when attempting to vectorize our inner loop

```Makefile
bin/PolyMult.time: $(TIME_SRCS) $(HEADERS) | bin
	$(CC) $(STD) $(OPT) $(INCLUDE) $(VARIANT) -ffast-math -fopenmp \
		-fopt-info-vec-missed=vec-missed.txt \
		$(TIME_SRCS) -o $@ $(LIBS)
```

as shown in the first couple lines from logs, our inner loop uses a "complicated" memory access pattern

```txt
src/PolyMult.c:13:13: missed: couldn't vectorize loop
src/PolyMult.c:13:13: missed: not vectorized: multiple nested loops.
src/PolyMult.c:17:10: missed: couldn't vectorize loop
src/PolyMult.c:17:10: missed: not vectorized: control flow in loop.
src/PolyMult.c:41:33: missed: couldn't vectorize loop
src/PolyMult.c:44:13: missed: not vectorized: complicated access pattern.
```

looking at the inner loop, variables might not map in a FP32 friendly format.

To recap,

The current OPP implementation - 1 Thread per anti-diagonal with dynamic scheduling - has nearly linear speedup, but is not at the theoretical peak performance (TPP) on a single core.

We use objdump to identify what assembly code was compiled from our source code.

As shown from the objdump's output, the usage of scalar FP32 FMA was used rather than packed FP32 FMA.

This mean the compiler didn't / was unable to optimize the compiled source code on an instruction-level via vector parallelism.

To determine why the compiler was unable to implement vector parallelism, the Makefile was modified to save logs to a file.

According to the logfile, GCC failed to vectorize the inner accumulation loop due to complicated access pattern.

To identify how to reach TPP via exploiting the processor's 256-bit FP32 vector FMA capability, we use the "perf stat --topdown" measurement command to observe the compiled binary's runtime instruction handling: retired, frontend, backend-core, backend-memory. 


```bash

perf stat --topdown -- ./bin/PolyMult.time -dA 2000 -dB 2000

variant,dA,dB,threads,block_size,leaf_size,task_threshold,build_params,rep,seconds,gflops
OPP2,2000,2000,1,0,0,0,"",0,0.017574620,0.456
OPP2,2000,2000,1,0,0,0,"",1,0.017395423,0.460
OPP2,2000,2000,1,0,0,0,"",2,0.017443352,0.459

 Performance counter stats for './bin/PolyMult.time -dA 2000 -dB 2000':

 %  tma_backend_bound      %  tma_retiring %  tma_bad_speculation %  tma_frontend_bound      %  tma_retiring %  tma_bad_speculation %  tma_backend_bound %  tma_frontend_bound
                        91.5                  3.7                             0.8                            4.0                         12.7                            -0.8                         57.2                           30.8

       0.054455725 seconds time elapsed

       1.524711000 seconds user
       0.035056000 seconds sys

```

The above output show our runtime is backend bound. to identify if core or resource bound, we use the topdown level 2 flag. We also specify runninng the binary on an e core rather than p core to ensure our results remain consistent to the goal of optimizing our code for the e core. note the cpu type was identified via using the lscpu -e command

```bash
Bash(cd /s/bach/m/under/C837194065/cs475/StudentPolyMult/01-OPP2 && taskset -c 16 perf stat --topdown --td-level=2 --cputype cpu_atom -- ./bin/PolyMult.time -dA 2000 -dB 2000)
variant,dA,dB,threads,block_size,leaf_size,task_threshold,build_params,rep,seconds,gflops
OPP2,2000,2000,1,0,0,0,"",0,0.010879166,0.736
OPP2,2000,2000,1,0,0,0,"",1,0.010863757,0.737
OPP2,2000,2000,1,0,0,0,"",2,0.010851651,0.738

 Performance counter stats for './bin/PolyMult.time -dA 2000 -dB 2000':

 %  tma_machine_clears    %  tma_core_bound %  tma_resource_bound %  tma_branch_mispredicts %  tma_ifetch_latency %  tma_ifetch_bandwidth
0.0                          0.0                           75.8                                0.9                            0.1                              0.1

       0.034076523 seconds time elapsed

       0.032744000 seconds user
       0.000992000 seconds sys
```

the output shows 01-OPP2 is currently backend bound: resource bound. At topdown level 2, further narrowing of the bottleneck's cause is needed. Candidate resources include the following:
- load/store
- execution

```bash
taskset -c 16 perf stat --topdown --td-level=3 --cputype cpu_atom -- ./bin/PolyMult.time -dA 2000 -dB 2000
variant,dA,dB,threads,block_size,leaf_size,task_threshold,build_params,rep,seconds,gflops
OPP2,2000,2000,1,0,0,0,"",0,0.010865967,0.737
OPP2,2000,2000,1,0,0,0,"",1,0.010865258,0.737
OPP2,2000,2000,1,0,0,0,"",2,0.010851315,0.738

 Performance counter stats for './bin/PolyMult.time -dA 2000 -dB 2000':

 %  tma_serialization %  tma_reorder_buffer      %  tma_register     %  tma_predecode      %  tma_other_fb          %  tma_nuke %  tma_non_mem_scheduler %  tma_mem_scheduler   %  tma_itlb_misses %  tma_icache_misses     %  tma_fast_nuke        %  tma_decode          %  tma_cisc %  tma_branch_resteer %  tma_branch_detect %  tma_allocation_restriction 
0.5                   72.7                  0.0                  0.0                  0.0                  0.0                       0.1                  0.0                  0.0                  0.0                  0.0                  0.0                  0.0                    0.0                  0.0                            0.0 

       0.034166611 seconds time elapsed

       0.033711000 seconds user
       0.000000000 seconds sys

```

The above output is used for identirying our resource bound bottleneck

Reorder Buffer is a CPU structure that tracks instructions after they are decoded but before they retire. It allows the CPU to have many instructions "in flight" simultaneously so that independent operations can execute out of order

As shown by tma_reorder_buffer being 72.7%, the CPU is spending most of its backend-bound: resource-bound time on the reorder buffer step.

This relates to the part in OPP2 where we add up operands of a given anti-diagonal.

In more detail, every time we add two values in the anti diagonal, the CPU does the following 

1. load a[i]
2. load b[k-i]
3. FMA
4. update c[k]

// in a 2000 x 2000 matrix this can reach nearly 3,000 operations for the longest anti-diagonal

with the current implemenation, every FMA operation of two operands updates the SAME c[k] !!!

this creates a long dependency chain that the CPU CANNOT send many independent operations in flight.

This can be why the Reorder Buffer is our backend-bound:resource-bound bottleneck

Furthermore, since our objdump command exposed single scalar usage (vfmadd231ss) rather than packed scalar usage (vfmadd231ps), we know the cpu is executing scalar FMA one at a time rather than 8 at a atime.

To be sure our chain of single scalar FMA operations is the reason behind our backend-bound:resource-bound:reorder buffer, we can observe how our register behavior using the following command:

```bash
cod:~/cs475/StudentPolyMult/01-OPP2$ objdump -d -Mintel bin/PolyMult.time | grep -B 8 -A 8 -E 'vfmadd231ss'
  40167d:       44 29 ce                sub    esi,r9d
  401680:       4c 29 d2                sub    rdx,r10
  401683:       4c 01 d6                add    rsi,r10
  401686:       4b 8d 44 95 00          lea    rax,[r13+r10*4+0x0]
  40168b:       49 8d 14 96             lea    rdx,[r14+rdx*4]
  40168f:       49 8d 34 b7             lea    rsi,[r15+rsi*4]
  401693:       0f 1f 44 00 00          nop    DWORD PTR [rax+rax*1+0x0]
  401698:       c5 fa 10 10             vmovss xmm2,DWORD PTR [rax]
  40169c:       c4 e2 69 b9 02          vfmadd231ss xmm0,xmm2,DWORD PTR [rdx]
  4016a1:       48 83 c0 04             add    rax,0x4
  4016a5:       48 83 ea 04             sub    rdx,0x4
  4016a9:       c5 fa 11 01             vmovss DWORD PTR [rcx],xmm0
  4016ad:       48 39 c6                cmp    rsi,rax
  4016b0:       75 e6                   jne    401698 <kernel_multiply._omp_fn.0+0xa8>
  4016b2:       48 83 c7 01             add    rdi,0x1
  4016b6:       48 83 c1 04             add    rcx,0x4
  4016ba:       41 39 f8                cmp    r8d,edi
```

As shown in teh above assembly instructions used, we're accumulating c[k] in a register rather than naively loading/storing c[k] from memory each ieration.

Despite this, our OPP2 implementation is still not at TPP  because the inner loop is still inefficient because its salar and dependency heavy.


```bash
cod:~/cs475/StudentPolyMult/01-OPP2$ taskset -c 16 perf stat --topdown --td-level=3 --cputype cpu_atom -- ./bin/PolyMult.time -dA 2000 -dB 2000
variant,dA,dB,threads,block_size,leaf_size,task_threshold,build_params,rep,seconds,gflops
OPP2,2000,2000,1,0,0,0,"",0,0.001474478,5.431
OPP2,2000,2000,1,0,0,0,"",1,0.001459003,5.489
OPP2,2000,2000,1,0,0,0,"",2,0.001453495,5.509

 Performance counter stats for './bin/PolyMult.time -dA 2000 -dB 2000':

 %  tma_serialization %  tma_reorder_buffer      %  tma_register     %  tma_predecode      %  tma_other_fb          %  tma_nuke %  tma_non_mem_scheduler %  tma_mem_scheduler   %  tma_itlb_misses %  tma_icache_misses     %  tma_fast_nuke        %  tma_decode          %  tma_cisc %  tma_branch_resteer %  tma_branch_detect %  tma_allocation_restriction 
                23.3                   37.4                  0.0                  0.0                  0.1                  0.0                       0.1                  0.2                                                                                                                                                                                                                                                                                                                                                 

       0.006373186 seconds time elapsed

       0.005706000 seconds user
       0.000000000 seconds sys
```
this is about 5.5 gflops

given 4 accumulators out performed 6 and 8 accumulators, we continue with looking at performance bottlenecks shown from the following:

```bash
cod:~/cs475/StudentPolyMult/01-OPP2$ objdump -d -Mintel bin/PolyMult.time |
    sed -n '/<kernel_multiply._omp_fn.0>/,/^$/p' |
    grep -B 15 -A 15 -E 'vfmadd231ps|vaddps|vextractf128|vperm'
  4016bf:       4c 63 d9                movsxd r11,ecx
  4016c2:       c5 f0 57 c9             vxorps xmm1,xmm1,xmm1
  4016c6:       48 89 f0                mov    rax,rsi
  4016c9:       d1 ea                   shr    edx,1
  4016cb:       4b 8d 3c 9c             lea    rdi,[r12+r11*4]
  4016cf:       4c 29 d8                sub    rax,r11
  4016d2:       44 8d 4a ff             lea    r9d,[rdx-0x1]
  4016d6:       48 8d 57 20             lea    rdx,[rdi+0x20]
  4016da:       49 c1 e1 05             shl    r9,0x5
  4016de:       48 8d 44 83 f4          lea    rax,[rbx+rax*4-0xc]
  4016e3:       49 01 d1                add    r9,rdx
  4016e6:       eb 0c                   jmp    4016f4 <kernel_multiply._omp_fn.0+0x104>
  4016e8:       0f 1f 84 00 00 00 00    nop    DWORD PTR [rax+rax*1+0x0]
  4016ef:       00 
  4016f0:       48 83 c2 20             add    rdx,0x20
  4016f4:       c4 e3 7d 04 17 1b       vpermilps ymm2,YMMWORD PTR [rdi],0x1b
  4016fa:       c5 f8 10 30             vmovups xmm6,XMMWORD PTR [rax]
  4016fe:       48 89 d7                mov    rdi,rdx
  401701:       48 83 e8 20             sub    rax,0x20
  401705:       c4 e3 4d 18 40 10 01    vinsertf128 ymm0,ymm6,XMMWORD PTR [rax+0x10],0x1
  40170c:       c4 e2 6d b8 c8          vfmadd231ps ymm1,ymm2,ymm0
  401711:       49 39 d1                cmp    r9,rdx
  401714:       75 da                   jne    4016f0 <kernel_multiply._omp_fn.0+0x100>
  401716:       c5 f8 28 c1             vmovaps xmm0,xmm1
  40171a:       c4 e3 7d 19 c9 01       vextractf128 xmm1,ymm1,0x1
  401720:       44 89 f0                mov    eax,r14d
  401723:       c5 f8 58 c9             vaddps xmm1,xmm0,xmm1
  401727:       83 e0 fe                and    eax,0xfffffffe
  40172a:       41 81 e6 01 00 00 80    and    r14d,0x80000001
  401731:       c5 f8 28 e1             vmovaps xmm4,xmm1
  401735:       c5 f0 15 e9             vunpckhps xmm5,xmm1,xmm1
  401739:       c5 f0 c6 d1 55          vshufps xmm2,xmm1,xmm1,0x55
  40173e:       c5 f0 c6 c1 ff          vshufps xmm0,xmm1,xmm1,0xff
  401743:       74 34                   je     401779 <kernel_multiply._omp_fn.0+0x189>
  401745:       49 8d 14 83             lea    rdx,[r11+rax*4]
  401749:       48 c1 e0 04             shl    rax,0x4
  40174d:       c4 c3 79 04 04 94 1b    vpermilps xmm0,XMMWORD PTR [r12+rdx*4],0x1b
  401754:       48 89 f2                mov    rdx,rsi
  401757:       4c 29 da                sub    rdx,r11
  40175a:       48 8d 14 93             lea    rdx,[rbx+rdx*4]
  40175e:       48 29 c2                sub    rdx,rax
  401761:       c4 e2 71 98 42 f4       vfmadd132ps xmm0,xmm1,XMMWORD PTR [rdx-0xc]
  401767:       c5 f8 28 e0             vmovaps xmm4,xmm0
  40176b:       c5 f8 15 e8             vunpckhps xmm5,xmm0,xmm0
  40176f:       c5 f8 c6 d0 55          vshufps xmm2,xmm0,xmm0,0x55
  401774:       c5 f8 c6 c0 ff          vshufps xmm0,xmm0,xmm0,0xff
  401779:       c5 fa 58 c5             vaddss xmm0,xmm0,xmm5
  40177d:       c5 ea 58 d4             vaddss xmm2,xmm2,xmm4
  401781:       42 8d 4c b9 04          lea    ecx,[rcx+r15*4+0x4]
  401786:       c5 fa 58 c2             vaddss xmm0,xmm0,xmm2
  40178a:       41 39 c8                cmp    r8d,ecx
  40178d:       0f 8c 35 01 00 00       jl     4018c8 <kernel_multiply._omp_fn.0+0x2d8>
--
  4017ad:       c5 f0 57 c9             vxorps xmm1,xmm1,xmm1
  4017b1:       45 89 f3                mov    r11d,r14d
  4017b4:       48 29 c2                sub    rdx,rax
  4017b7:       41 c1 eb 03             shr    r11d,0x3
  4017bb:       48 8d 7c 93 e4          lea    rdi,[rbx+rdx*4-0x1c]
  4017c0:       45 8d 7b ff             lea    r15d,[r11-0x1]
  4017c4:       49 8d 14 84             lea    rdx,[r12+rax*4]
  4017c8:       48 8d 47 e0             lea    rax,[rdi-0x20]
  4017cc:       49 c1 e7 05             shl    r15,0x5
  4017d0:       49 89 c3                mov    r11,rax
  4017d3:       4d 29 fb                sub    r11,r15
  4017d6:       eb 0c                   jmp    4017e4 <kernel_multiply._omp_fn.0+0x1f4>
  4017d8:       0f 1f 84 00 00 00 00    nop    DWORD PTR [rax+rax*1+0x0]
  4017df:       00 
  4017e0:       48 83 e8 20             sub    rax,0x20


  4017e4:       c4 e2 65 36 17          vpermd ymm2,ymm3,YMMWORD PTR [rdi]
#   vactor FMA improvement
  4017e9:       c4 e2 6d b8 0a          vfmadd231ps ymm1,ymm2,YMMWORD PTR [rdx]
  4017ee:       48 89 c7                mov    rdi,rax
  4017f1:       48 83 c2 20             add    rdx,0x20
  4017f5:       4c 39 d8                cmp    rax,r11
  4017f8:       75 e6                   jne    4017e0 <kernel_multiply._omp_fn.0+0x1f0>

# serial chain
  4017fa:       c4 e3 7d 19 ca 01       vextractf128 xmm2,ymm1,0x1
  401800:       44 89 f0                mov    eax,r14d
  401803:       c5 e8 58 c9             vaddps xmm1,xmm2,xmm1
  401807:       83 e0 f8                and    eax,0xfffffff8
  40180a:       8d 14 08                lea    edx,[rax+rcx*1]
  40180d:       c5 f0 12 d1             vmovhlps xmm2,xmm1,xmm1
  401811:       c5 e8 58 d1             vaddps xmm2,xmm2,xmm1
  401815:       c5 e8 c6 ca 55          vshufps xmm1,xmm2,xmm2,0x55
  40181a:       c5 f0 58 ca             vaddps xmm1,xmm1,xmm2
  40181e:       c5 fa 58 c1             vaddss xmm0,xmm0,xmm1
  401822:       44 39 f0                cmp    eax,r14d
  401825:       0f 84 9d 00 00 00       je     4018c8 <kernel_multiply._omp_fn.0+0x2d8>
  40182b:       41 29 c1                sub    r9d,eax
  40182e:       41 8d 79 01             lea    edi,[r9+0x1]
  401832:       41 83 f9 02             cmp    r9d,0x2
  401836:       76 3d                   jbe    401875 <kernel_multiply._omp_fn.0+0x285>
  401838:       48 63 c9                movsxd rcx,ecx
  40183b:       49 89 f1                mov    r9,rsi
  40183e:       49 29 c9                sub    r9,rcx
  401841:       49 29 c1                sub    r9,rax
  401844:       48 01 c8                add    rax,rcx
  401847:       c4 a3 79 04 4c 8b f4    vpermilps xmm1,XMMWORD PTR [rbx+r9*4-0xc],0x1b
  40184e:       1b 
  40184f:       c4 c1 70 59 0c 84       vmulps xmm1,xmm1,XMMWORD PTR [r12+rax*4]
  401855:       89 f8                   mov    eax,edi
  401857:       83 e0 fc                and    eax,0xfffffffc
  40185a:       01 c2                   add    edx,eax
  40185c:       c5 f0 12 d1             vmovhlps xmm2,xmm1,xmm1
  401860:       c5 e8 58 d1             vaddps xmm2,xmm2,xmm1
  401864:       c5 e8 c6 ca 55          vshufps xmm1,xmm2,xmm2,0x55
  401869:       c5 f0 58 ca             vaddps xmm1,xmm1,xmm2
  40186d:       c5 fa 58 c1             vaddss xmm0,xmm0,xmm1
  401871:       39 f8                   cmp    eax,edi
  401873:       74 53                   je     4018c8 <kernel_multiply._omp_fn.0+0x2d8>
  401875:       44 89 d0                mov    eax,r10d
  401878:       48 63 fa                movsxd rdi,edx
  40187b:       29 d0                   sub    eax,edx
  40187d:       48 8d 0c bd 00 00 00    lea    rcx,[rdi*4+0x0]
  401884:       00 
  401885:       48 98                   cdqe   
  401887:       c5 fa 10 3c 83          vmovss xmm7,DWORD PTR [rbx+rax*4]
  40188c:       c4 c2 41 b9 04 bc       vfmadd231ss xmm0,xmm7,DWORD PTR [r12+rdi*4]
  401892:       8d 7a 01                lea    edi,[rdx+0x1]
  401895:       41 39 f8                cmp    r8d,edi
  401898:       7c 2e                   jl     4018c8 <kernel_multiply._omp_fn.0+0x2d8>
  40189a:       44 89 d0                mov    eax,r10d
```

Althogh implementing a loop that takes advantage of vector FMA, we create a chain of serial work that comes after the FMA loop.

Only through analyzing the topdown level 3 of the binary that uses 6 and 8 accumulartors, we can narrow down weather or not the accumulator/reduction structure is contributing to he bottleneck.

<!-- 6 acc -->
```bash
cod:~/cs475/StudentPolyMult/01-OPP2$ taskset -c 16 perf stat --topdown --td-level=3 --cputype cpu_atom -- ./bin/PolyMult.time -dA 2000 -dB 2000
variant,dA,dB,threads,block_size,leaf_size,task_threshold,build_params,rep,seconds,gflops
OPP2,2000,2000,1,0,0,0,"",0,0.002185979,3.663
OPP2,2000,2000,1,0,0,0,"",1,0.002183338,3.668
OPP2,2000,2000,1,0,0,0,"",2,0.002202407,3.636

 Performance counter stats for './bin/PolyMult.time -dA 2000 -dB 2000':

 %  tma_serialization %  tma_reorder_buffer      %  tma_register     %  tma_predecode      %  tma_other_fb          %  tma_nuke %  tma_non_mem_scheduler %  tma_mem_scheduler   %  tma_itlb_misses %  tma_icache_misses     %  tma_fast_nuke        %  tma_decode          %  tma_cisc %  tma_branch_resteer %  tma_branch_detect %  tma_allocation_restriction 
 17.5                   16.8                  0.0                  0.0                  0.1                  0.0                       5.3                  2.2                  0.0                  0.1                  nan                                                                                                                                                                                                                   

       0.008512086 seconds time elapsed

       0.005932000 seconds user
       0.001988000 seconds sys
```

<!-- 8 accumulators -->
```bash
cod:~/cs475/StudentPolyMult/01-OPP2$ taskset -c 16 perf stat --topdown --td-level=3 --cputype cpu_atom -- ./bin/PolyMult.time -dA 2000 -dB 2000
variant,dA,dB,threads,block_size,leaf_size,task_threshold,build_params,rep,seconds,gflops
OPP2,2000,2000,1,0,0,0,"",0,0.001442405,5.552
OPP2,2000,2000,1,0,0,0,"",1,0.001431249,5.595
OPP2,2000,2000,1,0,0,0,"",2,0.001425009,5.620

 Performance counter stats for './bin/PolyMult.time -dA 2000 -dB 2000':

 %  tma_serialization %  tma_reorder_buffer      %  tma_register     %  tma_predecode      %  tma_other_fb          %  tma_nuke %  tma_non_mem_scheduler %  tma_mem_scheduler   %  tma_itlb_misses %  tma_icache_misses     %  tma_fast_nuke        %  tma_decode          %  tma_cisc %  tma_branch_resteer %  tma_branch_detect %  tma_allocation_restriction 
 25.6                   38.5                  0.0                  0.0                  0.0                  0.0                       0.0                  4.5                                                                                                                                                                                                                                                                                                                                                 

       0.006328887 seconds time elapsed

       0.004507000 seconds user
       0.001133000 seconds sys
```

across acculator count 4, 6, 8, the serialization and reorder buffer are the greatest contributing factor to the backend-bound:resource-bound bottleneck.

---
## question 4

Build and test StudentPolyMult/EXQ where the loops are interchanged