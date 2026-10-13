# Validation record

Date: 2026-10-13

Environment: `ubuntu:24.04` container, LLVM/Clang 18.1.3, CMake 3.28.3,
Ninja 1.11.1.

## Build and tests

Command:

```bash
docker build -t forge-llvm-ci -f Dockerfile.ci .
docker run --rm -v "$PWD:/src" forge-llvm-ci
```

Observed result:

- plugin compilation and linking: PASS
- `ir.basic`: PASS
- `ir.control-flow`: PASS
- `ir.safety`: PASS
- `randomized.verifier`: PASS (100 cases, seed `20261013`)

Total: 4/4 CTest groups passed. This is not a formal correctness proof.

## Benchmark smoke test

A three-trial, 5,000,000-iteration smoke run completed the O0, O0+Forge, O2,
O2+Forge, O3, and O3+Forge pipelines. Every executable printed the identical
result `3096237853266002691`. The run was deliberately too short for credible
speedup claims, so timings are not presented as performance results.

## Remaining evidence

- GitHub Actions run `38395465720` independently passed build and test on the
  initial published commit.
- A full benchmark campaign on the target host is not yet complete.
- Alive2 or another formal refinement checker has not been run.

## Forge v2 local feature-branch evidence

The `feature/forge-v2` branch was validated separately before publication:

- Release and Debug LLVM 18 builds: PASS.
- Nine CTest groups: PASS in both configurations.
- GlobalCSE dominance-positive, sibling non-dominance, loop, and flag cases:
  PASS.
- Restricted LICM hoist, trapping division, variant operand, and missing
  preheader cases: PASS.
- Differential execution: PASS for 36 boundary combinations and 2,000 seeded
  inputs (`20261013`).
- `forge-analyze` JSON smoke execution: PASS.
- x86-64-v3, Neoverse V2, and Armv9-A+SVE2 assembly generation: PASS.
- x86-64 and Neoverse V2 `llvm-mca`: PASS as static scheduling analysis.
- Six-pipeline, seven-kernel HPC smoke campaign: PASS with matching digest
  `5304214321748023827`.
- Alive2: UNAVAILABLE in the validation container; no formal-proof claim.

The HPC run used only two trials at size 2,000 inside a container. It validates
the campaign workflow, not performance. No native AArch64 or Grace execution
has occurred.
