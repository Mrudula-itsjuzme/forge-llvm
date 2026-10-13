# Performance report

## Status

The benchmark pipeline and seven deterministic kernels are implemented. A
two-trial, size-2,000 smoke campaign completed all six pipelines and every
pipeline produced validation digest `5304214321748023827`. This establishes
that the harness builds, runs, records raw trials, and rejects output mismatch.

The smoke run is too short and containerized to support runtime comparisons.
No speedup table is published yet.

## Required full campaign

Before making a performance claim:

1. Record CPU model, microcode, architecture, OS/kernel, compiler version,
   frequency governor, affinity, and whether turbo is enabled.
2. Use release builds, fixed workload sizes, at least one warm-up, and enough
   trials to report median and dispersion.
3. Compare identical pass placement: minimal versus minimal+Forge, O2 versus
   O2+Forge, and O3 versus O3+Forge.
4. Retain every raw timing and validation digest.
5. Separate IR instruction count, machine instruction count, binary size,
   optimizer time, and runtime.
6. Explain neutral or negative results. LLVM's standard pipelines are expected
   to subsume many Forge opportunities.

## AArch64 boundary

Current AArch64 results are generated assembly and `llvm-mca` scheduling-model
output. They are static estimates. Native NVIDIA Grace results are unavailable
until the campaign runs on identified Grace hardware.

