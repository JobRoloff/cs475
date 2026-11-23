###
### CS475 Fall 2026
### Makefile for CUDA Lab 6
### Based on the Lab 5 Makefile by Wim Bohm, Waruna Ranasinghe, and Louis Rabiet
### RTX 4000 Ada GPUs (sm_89), CUDA 12.8, system gcc 11.5
### 

OPTIONS   :=  -O3 --ptxas-options -v --gpu-architecture=sm_89

EXECS :=  sgemm00 sgemm01
all:$(EXECS)

clean:
	rm -f $(EXECS) *.o

timer.o : timer.cu timer.h
	nvcc $< -c -o $@ $(OPTIONS)

sgemmKernel00.o : sgemmKernel00.cu sgemmKernel.h
	nvcc $< -c -o $@ $(OPTIONS)

sgemm00 : sgemm.cu sgemmKernel.h sgemmKernel00.o timer.o
	nvcc $< sgemmKernel00.o -o $@ $(LIB) timer.o $(OPTIONS)

# Add rules to build "sgemm01" from sgemmKernel01.cu
# Notice how its done from lines 19 - 23

sgemmKernel01.o : sgemmKernel01.cu sgemmKernel.h
	nvcc $< -c -o $@ $(OPTIONS)

sgemm01 : sgemm.cu sgemmKernel.h sgemmKernel01.o timer.o
	nvcc $< sgemmKernel01.o -o $@ $(LIB) timer.o $(OPTIONS)
