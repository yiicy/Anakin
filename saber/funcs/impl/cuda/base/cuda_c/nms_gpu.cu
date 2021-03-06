
#include "saber/utils.h"
#include "saber/core/common.h"
#include <vector>

namespace anakin {

namespace saber {
// caffe util_nms.cu
#define DIVUP(m, n) ((m) / (n) + ((m) % (n) > 0))
int const threadsPerBlock = sizeof(unsigned long long) * 8;

__device__ inline float devIoU(float const *const a, float const *const b) {
    float left = max(a[0], b[0]), right = min(a[2], b[2]);
    float top = max(a[1], b[1]), bottom = min(a[3], b[3]);
    float width = max(right - left + 1, 0.f), height = max(bottom - top + 1, 0.f);
    float interS = width * height;
    float Sa = (a[2] - a[0] + 1) * (a[3] - a[1] + 1);
    float Sb = (b[2] - b[0] + 1) * (b[3] - b[1] + 1);
    return interS / (Sa + Sb - interS);
}

__global__ void nms_kernel(const int n_boxes, const float nms_overlap_thresh,
                           const float *dev_boxes, unsigned long long *dev_mask) {
    const int row_start = blockIdx.y;
    const int col_start = blockIdx.x;

    const int row_size =
            min(n_boxes - row_start * threadsPerBlock, threadsPerBlock);
    const int col_size =
            min(n_boxes - col_start * threadsPerBlock, threadsPerBlock);

    __shared__ float block_boxes[threadsPerBlock * 5];
    if (threadIdx.x < col_size) {
        block_boxes[threadIdx.x * 5 + 0] =
                dev_boxes[(threadsPerBlock * col_start + threadIdx.x) * 5 + 0];
        block_boxes[threadIdx.x * 5 + 1] =
                dev_boxes[(threadsPerBlock * col_start + threadIdx.x) * 5 + 1];
        block_boxes[threadIdx.x * 5 + 2] =
                dev_boxes[(threadsPerBlock * col_start + threadIdx.x) * 5 + 2];
        block_boxes[threadIdx.x * 5 + 3] =
                dev_boxes[(threadsPerBlock * col_start + threadIdx.x) * 5 + 3];
        block_boxes[threadIdx.x * 5 + 4] =
                dev_boxes[(threadsPerBlock * col_start + threadIdx.x) * 5 + 4];
    }
    __syncthreads();

    if (threadIdx.x < row_size) {
        const int cur_box_idx = threadsPerBlock * row_start + threadIdx.x;
        const float *cur_box = dev_boxes + cur_box_idx * 5;
        int i = 0;
        unsigned long long t = 0;
        int start = 0;
        if (row_start == col_start) {
            start = threadIdx.x + 1;
        }
        for (i = start; i < col_size; i++) {
            if (devIoU(cur_box, block_boxes + i * 5) > nms_overlap_thresh) {
                t |= 1ULL << i;
            }
        }
        const int col_blocks = DIVUP(n_boxes, threadsPerBlock);
        dev_mask[cur_box_idx * col_blocks + col_start] = t;
    }
}

const std::vector<bool> nms_voting0(const float *boxes_dev, unsigned long long *mask_dev,
                               int boxes_num, float nms_overlap_thresh,
                               const int max_candidates,
                               const int top_n) {

    if ((max_candidates > 0) && (boxes_num > max_candidates)) {
        boxes_num = max_candidates;
    }
//    float *boxes_dev = NULL;
//    unsigned long long *mask_dev = NULL;

    const int col_blocks = DIVUP(boxes_num, threadsPerBlock);
//    CUDA_CHECK(cudaMalloc(&mask_dev,
//                          boxes_num * col_blocks * sizeof(unsigned long long)));

    dim3 blocks(DIVUP(boxes_num, threadsPerBlock),
                DIVUP(boxes_num, threadsPerBlock));
    dim3 threads(threadsPerBlock);
    nms_kernel << < blocks, threads >> > (boxes_num,
            nms_overlap_thresh,
            boxes_dev,
            mask_dev);

    std::vector<unsigned long long> mask_host(boxes_num * col_blocks);

    CUDA_CHECK(cudaMemcpy(&mask_host[0],
                          mask_dev,
                          sizeof(unsigned long long) * boxes_num * col_blocks,
                          cudaMemcpyDeviceToHost));

    std::vector<unsigned long long> remv(col_blocks);
    memset(&remv[0], 0, sizeof(unsigned long long) * col_blocks);
    std::vector<bool> mask(boxes_num, false);
    int num_to_keep = 0;
    for (int i = 0; i < boxes_num; i++) {
        int nblock = i / threadsPerBlock;
        int inblock = i % threadsPerBlock;

        if (!(remv[nblock] & (1ULL << inblock))) {
            ++num_to_keep;
            mask[i] = true;
            unsigned long long *p = &mask_host[0] + i * col_blocks;
            for (int j = nblock; j < col_blocks; j++) {
                remv[j] |= p[j];
            }
            if ((top_n > 0) && (num_to_keep >= top_n)) {
                break;
            }
        }
    }

//    CUDA_CHECK(cudaFree(mask_dev));
    return mask;
}

}
}