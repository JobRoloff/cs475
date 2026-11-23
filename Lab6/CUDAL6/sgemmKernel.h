///
/// sgemmKernel.h
/// For CSU CS475 Fall 2026, Lab 6
/// Created: 2026-10-27
///
/// Kernels written for use with this header multiply two
/// N x N matrices of floats, C = A * B, on the GPU.
/// Each thread block computes a BLOCK_SIZE x BLOCK_SIZE
/// block of C, one element per thread.
///

#ifndef SGEMMKERNEL_H
#define SGEMMKERNEL_H

#define BLOCK_SIZE 16

__global__ void MatMulKernel(const float* A, const float* B, float* C, int N);

#endif
