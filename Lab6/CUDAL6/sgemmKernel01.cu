#include "sgemmKernel.h"

/**
 * Tiled matrix multiply, C = A * B, for N x N matrices of floats.
 *
 * Each thread block computes one BLOCK_SIZE x BLOCK_SIZE tile of C. The block
 * works through A and B one tile at a time. In each step every thread loads
 * one element of the A tile and one of the B tile into shared memory, so each
 * element read from global memory is used BLOCK_SIZE times.
 *
 * N must be a multiple of BLOCK_SIZE.
 */
__global__ void MatMulKernel(const float* A, const float* B, float* C, int N)
{
    // Shared memory tiles for the current step
    __shared__ float As[BLOCK_SIZE][BLOCK_SIZE];
    __shared__ float Bs[BLOCK_SIZE][BLOCK_SIZE];

    int tx = threadIdx.x;
    int ty = threadIdx.y;

    // Element of C this thread computes
    int row = blockIdx.y * BLOCK_SIZE + ty;
    int col = blockIdx.x * BLOCK_SIZE + tx;

    float sum = 0.0f;

    // Walk across row-of-tiles of A and down column-of-tiles of B
    for (int t = 0; t < N / BLOCK_SIZE; t++) {
        // Each thread loads one element of each tile
        As[ty][tx] = A[row * N + (t * BLOCK_SIZE + tx)];
        Bs[ty][tx] = B[(t * BLOCK_SIZE + ty) * N + col];

        // Wait until the whole tile is loaded
        __syncthreads();

        // Partial dot product from shared memory
        for (int k = 0; k < BLOCK_SIZE; k++) {
            sum += As[ty][k] * Bs[k][tx];
        }

        // Wait until everyone is done before the next tile overwrites As/Bs
        __syncthreads();
    }

    C[row * N + col] = sum;
}
