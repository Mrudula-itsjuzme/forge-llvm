#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static volatile uint64_t input_seed = 0x9e3779b97f4a7c15ULL;
static volatile uint64_t output_sink;

int main(int argc, char **argv) {
  uint64_t iterations = argc > 1 ? strtoull(argv[1], NULL, 10) : 200000000ULL;
  uint64_t state = input_seed;
  for (uint64_t i = 0; i < iterations; ++i) {
    uint64_t a = state ^ i;
    uint64_t b = (state << 7) | (state >> 57);
    uint64_t x = a + b;
    uint64_t y = b + a;
    state = (x * 33U) ^ (y + i);
  }
  output_sink = state;
  printf("%" PRIu64 "\n", state);
  return 0;
}

