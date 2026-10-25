# Forge v3 baseline audit

Date: 2026-10-13

Branch point: local merge commit `5a4bf98` (`feature/forge-v2` merged into
`main`). Forge v3 work begins on `feature/forge-v3-evidence`.

## Repository baseline

Forge v2 provides three independently schedulable LLVM 18 passes:

- flag-aware same-block LocalCSE;
- scoped dominator-tree GlobalCSE; and
- restricted, speculation-checked integer LICM for innermost loops.

The repository also contains FileCheck/verifier tests, seeded generated IR,
executable differential tests, seven deterministic HPC kernels, paired pass
pipelines, static x86-64/AArch64 code generation, `llvm-mca` integration, a
native C++ IR analysis CLI, Release/Debug presets, and CI configuration.

## Reproduced baseline

Environment: Ubuntu 24.04 container, LLVM/Clang 18.1.3.

- Release compile and plugin link: PASS.
- `forge-analyze` compile and link: PASS.
- CTest: 9/9 groups PASS.
- Seeded verifier corpus: 100 cases, seed `20261013`, PASS.
- Differential execution: boundary and seeded inputs, PASS.

The baseline run establishes reproducibility, not universal correctness.

## Native host inventory

- Architecture: x86-64.
- CPU: 12th Gen Intel Core i5-12450H, 8 cores / 12 threads.
- ISA features include AVX2, FMA, BMI1/2, and AVX-VNNI.
- Cache inventory reported by `lscpu`: 320 KiB L1d, 384 KiB L1i, 7 MiB L2,
  12 MiB L3.
- Maximum reported frequency: 4.4 GHz.
- Scaling governor during audit: `powersave`.
- Kernel perf paranoia setting: `2`.

## Tool availability and consequences

| Capability | Status | Consequence |
|---|---|---|
| LLVM 18 container | available | Reproducible compilation and tests |
| Native `perf` | unavailable | No hardware-counter case-study claims yet |
| Alive2 `alive-tv` | unavailable | No formal-refinement claim |
| Host Clang/`opt` | unavailable | Toolchain execution remains containerized |
| Native x86-64 execution | available | Runtime studies are possible with container-built binaries |
| Native AArch64/Grace | unavailable | Static analysis only |

Containerized compilation can still produce binaries that execute on the host
CPU, but experiment metadata must identify the container toolchain and the
host execution context. Measurements taken under the `powersave` governor need
larger samples and careful variance reporting; they are not directly comparable
to fixed-frequency laboratory measurements.

## v2 gaps mapped to v3 tracks

1. Pass statistics count successful transformations but do not report examined
   candidates, rejection reasons, IR deltas, or pass time.
2. Benchmark schedules are fixed in code and do not isolate tuning workloads
   from final evaluation workloads.
3. Raw results are stored, but there is no schema-versioned experiment registry
   or report generator.
4. The benchmark smoke run has no performance claim and no three-case failure
   investigation.
5. Differential testing saves no minimized failure artifact.
6. Alive2 and hardware performance counters are unavailable.
7. Static backend artifacts lack automated instruction/vectorization summaries.

## v3 implementation order

1. Define the experiment registry schema and stable/tuning/evaluation workload
   split before collecting results.
2. Add opt-in diagnostics outside timed regions, with rejection reason enums and
   structured JSON aggregation.
3. Build the configurable schedule runner and report generator.
4. Run native timing campaigns and select at least three phenomena for case
   studies, including neutral or negative outcomes.
5. Expand mutation/differential reliability and preserve reproductions.
6. Attempt Alive2 only through a version-compatible, recorded environment.

No automatic scheduler will be added unless tuning evidence supports a policy
that is then evaluated on a sealed workload split.
