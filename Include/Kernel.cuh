__global__ void kernel(cudaSurfaceObject_t surface, float width, float height);
void runCudaKernel(cudaSurfaceObject_t surface, float width, float height);