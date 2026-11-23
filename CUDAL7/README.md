## Getting Started

1. Load Cuda 12.8 environment modules onto the machine

```bash
# This updates this machine's PATH variables
module load cuda/12.8
```

2. Compile the code

```bash
make
```

3. Run vecadd00 with 2,000 values per thread

```bash
./vecadd00 2000

# faulty output:=
Total vector size: 147456000
cuda malloc or cuda thread exit failed 

# if you get the above error, you've ran out of memory.. switch machines to one that has more available GPU space. You can double check with the nvidia-smi terminal command after you've loaded cuda first 


# corrected output:
Total vector size: 147456000
Time: 0.022017 (sec), GFlopsS: 6.697370, GBytesS: 80.368435
Test PASSED 
```

*Notice*
- Total vecor size is constant across failed / desired profile command usage
- The output includes: time, GFlopsS, GBytesS


4. Run the memory coalesced AddVectors method on the same number of values per thread as the provided program

```bash
blowfish:~/cs475/CUDAL7$ ./vecadd01 2000
Total vector size: 147456000
Time: 0.005650 (sec), GFlopsS: 26.098206, GBytesS: 313.178474
```

*Notice*
- Time went from .022017 to .005650 seconds. This is about 3.9 times faster (74.3% reduction in time)
- GFlopsS went from 6.697370 to 26.098206. This is about a 3.9 time higer compute throughput
- GBytesS went from 80.368435 to 313.178474. This is about a 3.9 times higher effective memory bandwidth.

## Lab Takeaways

Using memory coalescing in a simple vector addition method improves performance by roughly 3.9x (reducing runtime from 0.022017 s to 0.005650 s and increasing effective bandwidth from ~80.4 GB/s to ~313.2 GB/s).


## Questions

- Why is everything improving by the same factor of 3.9 times?