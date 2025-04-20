// opt.c
// Blocked (“opt”) matrix–multiply implementing Algorithm 2 exactly

#define _GNU_SOURCE
#include <stdlib.h>
#include "common/types.h"
#include "include/types.h"

#pragma GCC push_options
// Use the same floating-point ordering as the reference (no vectorization or aggressive FP optimizations)
#pragma GCC optimize("O1")
#pragma GCC optimize("no-tree-vectorize")
#pragma GCC optimize("no-fast-math")
void* impl_scalar_opt(void* args_) {
    args_t* mmult_args = (args_t*) args_;
    float* A = mmult_args->A;
    float* B = mmult_args->B;
    float* R = mmult_args->R;

    size_t M = mmult_args->M;
    size_t N = mmult_args->N;
    size_t P = mmult_args->P;
    size_t b = mmult_args->b;

    // Initialize output matrix R to zero
    for (size_t i = 0; i < M; ++i) {
        for (size_t j = 0; j < P; ++j) {
            R[i * P + j] = 0.0f;
        }
    }

    // Blocked matrix multiplication (blocks of size b × b)
    for (size_t ii = 0; ii < M; ii += b) {
        size_t i_max = (ii + b < M) ? ii + b : M;
        for (size_t jj = 0; jj < P; jj += b) {
            size_t j_max = (jj + b < P) ? jj + b : P;
            for (size_t kk = 0; kk < N; kk += b) {
                size_t k_max = (kk + b < N) ? kk + b : N;
                for (size_t i = ii; i < i_max; ++i) {
                    for (size_t j = jj; j < j_max; ++j) {
                        // accumulate partial sum for R[i][j]
                        float sum = R[i * P + j];
                        for (size_t k = kk; k < k_max; ++k) {
                            sum += A[i * N + k] * B[k * P + j];
                        }
                        R[i * P + j] = sum;
                    }
                }
            }
        }
    }

    return NULL;
}
#pragma GCC pop_options

