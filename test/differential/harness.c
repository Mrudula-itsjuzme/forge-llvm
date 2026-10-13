#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern uint32_t forge_compute(uint32_t, uint32_t, _Bool, uint32_t);

static uint64_t mix(uint64_t hash, uint32_t value) {
  return (hash ^ value) * UINT64_C(1099511628211);
}

int main(int argc, char **argv) {
  uint32_t seed = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 10) : 20261013U;
  const uint32_t boundary[] = {0U, 1U, UINT32_MAX, UINT32_MAX - 1U,
                               0x80000000U, 0x7fffffffU};
  uint64_t hash = UINT64_C(1469598103934665603);

  for (unsigned i = 0; i < sizeof(boundary) / sizeof(boundary[0]); ++i)
    for (unsigned j = 0; j < sizeof(boundary) / sizeof(boundary[0]); ++j)
      hash = mix(hash, forge_compute(boundary[i], boundary[j], (i + j) & 1U,
                                     1U + ((i * 7U + j) % 31U)));

  for (unsigned i = 0; i < 2000; ++i) {
    seed = seed * 1664525U + 1013904223U;
    uint32_t a = seed;
    seed = seed * 1664525U + 1013904223U;
    uint32_t b = seed;
    hash = mix(hash, forge_compute(a, b, seed & 1U, 1U + (seed % 31U)));
  }
  printf("%" PRIu64 "\n", hash);
  return 0;
}

