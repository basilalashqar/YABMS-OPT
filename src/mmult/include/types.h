#ifndef __INCLUDE_TYPES_H_
#define __INCLUDE_TYPES_H_

#include <stddef.h>

typedef struct {
  float* A;    // Input matrix A
  float* B;    // Input matrix B
  float* R;    // Output matrix R

  size_t M;    // Rows in A and R
  size_t N;    // Columns in A, Rows in B
  size_t P;    // Columns in B and R

  int cpu;      // CPU scheduling
  int nthreads; // Number of threads for parallel execution

  size_t b;     // Block size for optimized implementation
} args_t;

#endif //__INCLUDE_TYPES_H_

