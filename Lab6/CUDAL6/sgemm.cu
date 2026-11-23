///
/// sgemm.cu
/// For CSU CS475 Fall 2026, Lab 6
/// Based on vecadd.cu (CS575 Spring 2011, Wim Bohm and David Newman)
/// Created: 2026-10-27
///
/// Multiply two N x N matrices of floats, C = A * B, on GPU using
/// a kernel defined according to sgemmKernel.h.
/// The NVTX ranges (nvtxRangePush / nvtxRangePop) name the phases
/// of the program so that they show up in Nsight Systems.
/// Students must not modify this file. The GTA
/// will grade your submission using an unmodified
/// copy of this file.
///

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <nvtx3/nvToolsExt.h>
#include "timer.h"
#include "sgemmKernel.h"

// Variables for host and device matrices.
float* h_A;
float* h_B;
float* h_C;
float* d_A;
float* d_B;
float* d_C;

// Utility Functions
void Cleanup(bool);

// Host code performs setup and calls the kernel.
int main(int argc, char** argv)
{
    int N; // matrix dimension

	// Parse arguments.
    if(argc != 2){
     printf("Usage: %s N\n", argv[0]);
     printf("Multiplies two N x N matrices. N must be a multiple of %d.\n", BLOCK_SIZE);
     exit(0);
    } else {
      sscanf(argv[1], "%d", &N);
    }
    if(N <= 0 || N % BLOCK_SIZE != 0){
     printf("N must be a positive multiple of %d.\n", BLOCK_SIZE);
     exit(0);
    }
    printf("Matrix size: %d x %d\n", N, N);
    // size is the total number of bytes for a matrix.
    size_t size = (size_t)N * N * sizeof(float);

    // Tell CUDA how big to make the grid and thread blocks.
    // Each thread block computes a BLOCK_SIZE x BLOCK_SIZE block of C.
    dim3 dimGrid(N / BLOCK_SIZE, N / BLOCK_SIZE);
    dim3 dimBlock(BLOCK_SIZE, BLOCK_SIZE);

    // Allocate input matrices h_A and h_B and result h_C in host memory
    nvtxRangePush("Initialize A and B on host");
    h_A = (float*)malloc(size);
    if (h_A == 0) Cleanup(false);
    h_B = (float*)malloc(size);
    if (h_B == 0) Cleanup(false);
    h_C = (float*)malloc(size);
    if (h_C == 0) Cleanup(false);

    // Initialize host matrices h_A and h_B with small integers,
    // so every product and sum is exact and C can be checked exactly.
    int i, j, k;
    for(i=0; i<N; ++i){
      for(j=0; j<N; ++j){
        h_A[(size_t)i*N + j] = (float)((i + j) % 7 - 3);
        h_B[(size_t)i*N + j] = (float)((i + 2*j) % 5 - 2);
      }
    }
    nvtxRangePop();

    // Allocate matrices in device memory.
    cudaError_t error;
    error = cudaMalloc((void**)&d_A, size);
    if (error != cudaSuccess) Cleanup(false);
    error = cudaMalloc((void**)&d_B, size);
    if (error != cudaSuccess) Cleanup(false);
    error = cudaMalloc((void**)&d_C, size);
    if (error != cudaSuccess) Cleanup(false);

    // Copy host matrices h_A and h_B to device matrices d_A and d_B
    nvtxRangePush("Copy A and B to GPU");
    error = cudaMemcpy(d_A, h_A, size, cudaMemcpyHostToDevice);
    if (error != cudaSuccess) Cleanup(false);
    error = cudaMemcpy(d_B, h_B, size, cudaMemcpyHostToDevice);
    if (error != cudaSuccess) Cleanup(false);
    nvtxRangePop();

    // Warm up
    nvtxRangePush("Warm-up kernel");
    MatMulKernel<<<dimGrid, dimBlock>>>(d_A, d_B, d_C, N);
    error = cudaGetLastError();
    if (error != cudaSuccess) Cleanup(false);
    cudaDeviceSynchronize();
    nvtxRangePop();

    // Initialize timer
    initialize_timer();
    start_timer();

    // Invoke kernel
    nvtxRangePush("Timed kernel");
    MatMulKernel<<<dimGrid, dimBlock>>>(d_A, d_B, d_C, N);
    error = cudaGetLastError();
    if (error != cudaSuccess) Cleanup(false);
    cudaDeviceSynchronize();
    nvtxRangePop();

    // Compute elapsed time
    stop_timer();
    double time = elapsed_time();

    // Compute floating point operations per second.
    double nFlops = 2.0 * N * N * N; // N*N dot products of length N
    double nFlopsPerSec = nFlops/time;
    double nGFlopsPerSec = nFlopsPerSec*1e-9;

	// Report timing data.
    printf( "Time: %lf (sec), GFlopsS: %lf\n", time, nGFlopsPerSec);

    // Copy result from device memory to host memory
    nvtxRangePush("Copy C to host");
    error = cudaMemcpy(h_C, d_C, size, cudaMemcpyDeviceToHost);
    if (error != cudaSuccess) Cleanup(false);
    nvtxRangePop();

    // Verify & report result: recompute a 64 x 64 sample of C,
    // spread over the whole matrix, on the CPU.
    nvtxRangePush("Check C on host");
    int errors = 0;
    for (j = 0; j < 64; ++j) {
        int col = (int)((long)j * (N-1) / 63);
        for (i = 0; i < 64; ++i) {
            int row = (int)((long)i * (N-1) / 63);
            float sum = 0.0f;
            for (k = 0; k < N; ++k)
                sum += h_A[(size_t)row*N + k] * h_B[(size_t)k*N + col];
            if (h_C[(size_t)row*N + col] != sum)
                ++errors;
        }
    }
    nvtxRangePop();
    printf("Test %s \n", (errors == 0) ? "PASSED" : "FAILED");

    // Clean up and exit.
    Cleanup(true);
}

void Cleanup(bool noError) {  // simplified version from CUDA SDK
    cudaError_t error;

    // Free device matrices
    if (d_A)
        cudaFree(d_A);
    if (d_B)
        cudaFree(d_B);
    if (d_C)
        cudaFree(d_C);

    // Free host memory
    if (h_A)
        free(h_A);
    if (h_B)
        free(h_B);
    if (h_C)
        free(h_C);

    error = cudaDeviceReset();

    if (!noError || error != cudaSuccess)
        printf("cuda malloc or cuda thread exit failed \n");

    fflush( stdout);
    fflush( stderr);

    exit(0);
}
