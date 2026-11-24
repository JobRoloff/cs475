
lab goal: Learn how to think about the factors that govern the performance of CUDA programs

## Getting Started

1. Load Cuda 12.8 engironment modules onto the machine. Without this step, youll get an error from the Makefile because the Makefile depends on the Cuda modules...

```bash
# This updates this machine's PATH variables
module load cuda/12.8
```

2. Compile the code

```bash
make
```

3. Run the provided code

```bash
blowfish:~/cs475/PA5$ ./vecMax00 320 128 1280000000
Result: 1280000000.000000
Time to copy input: 1.031336
Compute time: 0.052705
Time to  copy output and generate final answer: 0.000375
```

```bash
blowfish:~/cs475/PA5$ ./matmult00 200
Data dimensions: 3200x3200 
Grid Dimensions: 200x200 
Block Dimensions: 16x16 
Footprint Dimensions: 16x16 
Time: 0.030307 (sec), nFlops: 65536000000, GFlopsS: 2162.417846
```

4. Write vecMax01 and matmult01 to use memory coalescence ()