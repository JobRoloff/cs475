///
/// sgemmKernel00.cu
/// For CSU CS475 Fall 2026, Lab 6
/// Created: 2026-10-27
///
/// This Kernel multiplies two N x N matrices A and B into C on GPU. Each thread computes one element of C and reads a whole row of A and a whole column of B from global memory.

#include "sgemmKernel.h"

__global__ void MatMulKernel(const float* A, const float* B, float* C, int N)
{
    // load an entire row and column into the thread's memory
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    
    // instantiate an accumulator
    float sum = 0.0f;

    // declare a counter
    int k;

    // iterate through the local row and column.
    for( k=0; k<N; ++k ){
        // accumulate the output operand's final value
        sum += A[row*N + k] * B[k*N + col];
    }
    
    // update the output operand's value with the accumulated value
    C[row*N + col] = sum;
}
