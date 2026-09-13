__global__ void kernel(cudaSurfaceObject_t surface, float width, float height)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height)
        return;

    float fx = x / width - 0.5f;
    float fy = y / height - 0.5f;

    unsigned char green =
        128 + 127 * sinf(fabsf(fx * 100.0f) - fabsf(fy * 100.0f));

    uchar4 pixel;

    pixel.x = 0;
    pixel.y = green;
    pixel.z = 0;
    pixel.w = 255;

    surf2Dwrite(
        pixel,
        surface,
        x * sizeof(uchar4),
        y
    );
}

void runCudaKernel(cudaSurfaceObject_t surface, float width, float height)
{

    dim3 blockDim(32, 32);

    dim3 gridDim((width + blockDim.x - 1) / blockDim.x,
        (height + blockDim.y - 1) / blockDim.y);

    kernel << <gridDim, blockDim >> > (surface, width, height);
}