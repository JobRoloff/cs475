///
/// matmultKernel00.cu
/// For CSU CS575 Spring 2024
/// Instructor: Wim Bohm
/// Based on code from the CUDA Programming Guide
/// Modified by Wim Bohm and David Newman
/// Created: 2011-01-27
/// Last Modified: 2024-03-04 Louis Narmour
///
/// Multiplies two matrices using CUDA: A x B = C
///
/// Copy this file and modify the MatMultKernel device function for
/// each of your experiments. 
///

#include "matmultKernel.h"

// defines a gpu kernel to perform matrix multiplication of A x B = C.
__global__ void MatMulKernel(Matrix A, Matrix B, Matrix C){

  // matrix blocks
  float *Asub, *Bsub, *Csub;

  // put these into registers speeds access
  int thread_row = threadIdx.y;
  int thread_col = threadIdx.x;
  int block_row = blockIdx.y;
  int block_col = blockIdx.x;

  // each THREAD BLOCK computes one sub matrix Csub of C
  // each THREAD creates its own matrix descriptor Csub
  Csub = &C.elements[C.stride * BLOCK_SIZE * block_row + BLOCK_SIZE * block_col];

  // each thread computes one element of Csub in its copy of Cvalue
  float Cvalue = 0;

  // 1) loop over sub matrices in block_row of A and block_col of B required to compute Csub
  // 2) block multiply each pair of sub matrices
  // 3) accumulate results
  for (int m = 0;  m < (A.width / BLOCK_SIZE); ++m){
    // get Asub and Bsub descriptors
    Asub = &A.elements[A.stride * BLOCK_SIZE * block_row + BLOCK_SIZE * m];
    Bsub = &B.elements[B.stride * BLOCK_SIZE * m + BLOCK_SIZE * block_col];

    // copy elems of ASub and Bsub into shared memory
    // each thread loads ONE elem of ASub and ONE elem of Bsub
    // Notice: it does not need to be the element it requires to
    //         compute its Cvalue, as long as all elements are 
    //         collaboratively read. 

    // NOTICE: every thread declares shared_A and shared_B in shared memory
    //         even though a thread block has only one shared_A and one shared_B
    __shared__ float shared_A[BLOCK_SIZE][BLOCK_SIZE];
    __shared__ float shared_B[BLOCK_SIZE][BLOCK_SIZE];

    // each thread copies just one element of shared_A and one element of shared_B
    shared_A[thread_row][thread_col] = Asub[thread_row * A.stride + thread_col];
    shared_B[thread_row][thread_col] = Bsub[thread_row * B.stride + thread_col];

    // synchronize to ensure all elements are read
    __syncthreads();

    // do an inner product of one row of shared_A and one col of shared_B
    // to compute one Cvalue by accumulation
#pragma unroll
    for(int e=0; e<BLOCK_SIZE; ++e)
       Cvalue += shared_A[thread_row][e] * shared_B[e][thread_col];

    // synchronize to ensure all Cvalues have been incremented
    // before reading in the next shared_A AND shared_B BLOCKS
    __syncthreads();
  }

  // write Csub to GLOBAL memory.
  // each thread writes its own cell value.
  Csub[thread_row * C.stride + thread_col] = Cvalue;
}

