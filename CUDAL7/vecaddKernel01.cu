// TODO: use memory coalescing

__global__ void AddVectors(const float* A, const float* B, float* C, int N)
{
    int blockStartIndex  = blockIdx.x * blockDim.x * N;
    
    // remove the * N from threadIds.x
    int threadStartIndex = blockStartIndex + threadIdx.x;
    // multiply N by blockDim.x 
    int threadEndIndex   = threadStartIndex + (blockDim.x * N);
    
    // int i;
    // increment each loop iteration by blockDim.x rather than incrementing by 1
    for(int i=threadStartIndex; i<threadEndIndex; i += blockDim.x ){
        C[i] = A[i] + B[i];
    }
}
