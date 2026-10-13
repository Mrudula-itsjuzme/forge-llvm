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

- GitHub Actions has not run until the repository is published.
- A full benchmark campaign on the target host is not yet complete.
- Alive2 or another formal refinement checker has not been run.
