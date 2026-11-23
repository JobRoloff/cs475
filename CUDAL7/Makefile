###
### CS475 Fall 2026
### Makefile for CUDA Lab 7
### By Wim Bohm, Waruna Ranasinghe, and Louis Rabiet
### Updated Fall 2026: RTX 4000 Ada GPUs (sm_89), CUDA 12.8, system gcc 11.5
###

OPTIONS   :=  -O3 --ptxas-options -v --gpu-architecture=sm_89

EXECS :=  vecadd00
all:$(EXECS)

clean:
	rm -f $(EXECS) *.o

timer.o : timer.cu timer.h
	nvcc $< -c -o $@ $(OPTIONS)


vecaddKernel00.o : vecaddKernel00.cu
	nvcc $< -c -o $@ $(OPTIONS)

vecadd00 : vecadd.cu vecaddKernel.h vecaddKernel00.o timer.o
	nvcc $< vecaddKernel00.o -o $@ $(LIB) timer.o $(OPTIONS)


