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


4. From here, I have to write a new file that uses "memory coalescing" to improve performance