// This method uses memory coalescing by aligning thread work across the GPU's
// thread and memory hierarchy so threads executing together in a warp access
// contiguous addresses in global memory at the same time, and each loop step
// advances by blockDim.x across stripes rather than spacing threads far apart.

__global__ void AddVectors(const float* A, const float* B, float* C, int N)
{
    int blockStartIndex  = blockIdx.x * blockDim.x * N;
    int threadStartIndex = blockStartIndex + threadIdx.x;
    int threadEndIndex   = threadStartIndex + (blockDim.x * N);

    // Because threads in a warp share the same instruction stream on each pass,
    // having thread 't' access 'base + t' means a warp touches a contiguous
    // block of floats rather than scattered addresses spaced N elements apart.
    // Stepping by blockDim.x moves each thread to the next stripe while keeping
    // adjacent threads on adjacent elements on every iteration.
    for (int i = threadStartIndex; i < threadEndIndex; i += blockDim.x) {
        C[i] = A[i] + B[i];
    }
}