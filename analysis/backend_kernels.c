#include <stddef.h>

long dot_i32(const int *a, const int *b, size_t n) {
  long sum = 0;
  for (size_t i = 0; i < n; ++i)
    sum += (long)a[i] * b[i];
  return sum;
}

void saxpy_f32(float *restrict out, const float *restrict x,
               const float *restrict y, float scale, size_t n) {
  for (size_t i = 0; i < n; ++i)
    out[i] = scale * x[i] + y[i];
}

void stencil_i32(int *restrict out, const int *restrict in, size_t n) {
  for (size_t i = 1; i + 1 < n; ++i)
    out[i] = in[i - 1] + 2 * in[i] + in[i + 1];
}

