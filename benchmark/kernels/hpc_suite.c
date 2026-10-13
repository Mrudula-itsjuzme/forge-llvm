#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__clang__) || defined(__GNUC__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

static volatile uint64_t sink;

static uint64_t now_ns(void) {
  struct timespec t;
  if (clock_gettime(CLOCK_MONOTONIC, &t) != 0)
    abort();
  return (uint64_t)t.tv_sec * UINT64_C(1000000000) + (uint64_t)t.tv_nsec;
}

NOINLINE static uint64_t reduction(const uint32_t *a, size_t n) {
  uint64_t sum = 0;
  for (size_t i = 0; i < n; ++i)
    sum += a[i];
  return sum;
}

NOINLINE static uint64_t dot_product(const uint32_t *a, const uint32_t *b,
                                     size_t n) {
  uint64_t sum = 0;
  for (size_t i = 0; i < n; ++i)
    sum += (uint64_t)a[i] * b[i];
  return sum;
}

NOINLINE static uint64_t stencil(uint32_t *out, const uint32_t *in, size_t n) {
  uint64_t sum = 0;
  for (size_t i = 1; i + 1 < n; ++i) {
    out[i] = in[i - 1] + 2U * in[i] + in[i + 1];
    sum += out[i];
  }
  return sum;
}

NOINLINE static uint64_t strided(const uint32_t *a, size_t n, size_t stride) {
  uint64_t sum = 0;
  for (size_t offset = 0; offset < stride; ++offset)
    for (size_t i = offset; i < n; i += stride)
      sum += a[i];
  return sum;
}

NOINLINE static uint64_t repeated_expression(const uint32_t *a,
                                              const uint32_t *b, size_t n) {
  uint64_t sum = 0;
  for (size_t i = 0; i < n; ++i) {
    uint32_t x = a[i] + b[i];
    uint32_t y = b[i] + a[i];
    sum += (uint64_t)x * y;
  }
  return sum;
}

NOINLINE static uint64_t loop_recurrence(uint64_t seed, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    uint64_t invariant = seed * UINT64_C(33);
    seed = (seed ^ invariant) + i;
  }
  return seed;
}

NOINLINE static uint64_t matrix_multiply(const uint32_t *a, const uint32_t *b,
                                         uint64_t *c, size_t n) {
  memset(c, 0, n * n * sizeof(*c));
  for (size_t i = 0; i < n; ++i)
    for (size_t k = 0; k < n; ++k)
      for (size_t j = 0; j < n; ++j)
        c[i * n + j] += (uint64_t)a[i * n + k] * b[k * n + j];
  uint64_t hash = 0;
  for (size_t i = 0; i < n * n; ++i)
    hash = (hash * UINT64_C(1099511628211)) ^ c[i];
  return hash;
}

static void emit(const char *kernel, unsigned trial, uint64_t ns,
                 uint64_t value) {
  printf("RESULT,%s,%u,%" PRIu64 ",%" PRIu64 "\n", kernel, trial, ns,
         value);
}

int main(int argc, char **argv) {
  size_t n = argc > 1 ? strtoull(argv[1], NULL, 10) : 200000U;
  unsigned trials = argc > 2 ? (unsigned)strtoul(argv[2], NULL, 10) : 9U;
  if (n < 64 || trials == 0)
    return 2;

  uint32_t *a = malloc(n * sizeof(*a));
  uint32_t *b = malloc(n * sizeof(*b));
  uint32_t *out = calloc(n, sizeof(*out));
  if (!a || !b || !out)
    return 3;
  for (size_t i = 0; i < n; ++i) {
    a[i] = (uint32_t)(i * 1664525U + 1013904223U);
    b[i] = (uint32_t)(i * 22695477U + 1U);
  }

  size_t matrix_n = n < 64 ? n : 64;
  uint64_t *matrix = calloc(matrix_n * matrix_n, sizeof(*matrix));
  if (!matrix)
    return 3;

  // Warm-up populates caches and establishes deterministic reference values.
  uint64_t expected[7];
  expected[0] = reduction(a, n);
  expected[1] = dot_product(a, b, n);
  expected[2] = stencil(out, a, n);
  expected[3] = strided(a, n, 16);
  expected[4] = repeated_expression(a, b, n);
  expected[5] = loop_recurrence(UINT64_C(0x9e3779b97f4a7c15), n);
  expected[6] = matrix_multiply(a, b, matrix, matrix_n);

  uint64_t digest = UINT64_C(1469598103934665603);
  for (unsigned trial = 0; trial < trials; ++trial) {
    uint64_t start, value;
#define RUN(ID, NAME, EXPR)                                                     \
    do {                                                                        \
      start = now_ns();                                                         \
      value = (EXPR);                                                           \
      uint64_t elapsed = now_ns() - start;                                      \
      if (value != expected[ID]) {                                              \
        fprintf(stderr, "validation failure: %s\n", NAME);                    \
        return 4;                                                               \
      }                                                                         \
      emit(NAME, trial, elapsed, value);                                        \
      digest = (digest ^ value) * UINT64_C(1099511628211);                     \
    } while (0)
    RUN(0, "reduction", reduction(a, n));
    RUN(1, "dot", dot_product(a, b, n));
    RUN(2, "stencil2d-row", stencil(out, a, n));
    RUN(3, "strided-access", strided(a, n, 16));
    RUN(4, "repeated-expression", repeated_expression(a, b, n));
    RUN(5, "loop-recurrence",
        loop_recurrence(UINT64_C(0x9e3779b97f4a7c15), n));
    RUN(6, "matrix-multiply", matrix_multiply(a, b, matrix, matrix_n));
#undef RUN
  }

  sink = digest;
  printf("VALIDATION,%" PRIu64 "\n", digest);
  free(matrix);
  free(out);
  free(b);
  free(a);
  return 0;
}

