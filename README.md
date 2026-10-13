# Forge: LLVM CPU Optimization Toolkit

Forge is a modular, out-of-tree LLVM 18 New Pass Manager toolkit for studying
conservative CPU middle-end transformations and their performance consequences.
It implements three distinct passes:

| Pipeline name | Scope | Safety boundary |
|---|---|---|
| `forge-local-cse` | One basic block | Exact integer expressions and flags |
| `forge-global-cse` | Dominated CFG regions | Scoped dominator-tree availability |
| `forge-licm` | Innermost loops | Preheader + invariant operands + safe speculation |

The older `forge-redundant-expr-elim` spelling remains an alias for LocalCSE.
Forge is intentionally narrower than LLVM EarlyCSE, GVN, and LICM and does not
claim to replace them.

## Current evidence

Forge v2 was built with LLVM/Clang 18.1.3 in Ubuntu 24.04. The current local
feature branch passes nine CTest groups:

- pass-specific FileCheck and verifier regressions;
- positive and prohibited dominance/loop transformations;
- 100 generated verifier cases with seed `20261013`; and
- executable differential comparison over 36 boundary combinations plus 2,000
  deterministic generated inputs.

Static code generation succeeded for x86-64-v3, AArch64 Neoverse V2, and
Armv9-A+SVE2. `llvm-mca` succeeded for the x86-64 and Neoverse V2 scheduling
models. A short six-pipeline, seven-kernel HPC smoke campaign produced matching
validation digests. Those are workflow checks, not credible speedup results or
Grace hardware measurements. Alive2 has not yet been run.

## Architecture

```mermaid
flowchart LR
  IR[LLVM IR] --> PM[New Pass Manager]
  REG[Plugin registration] --> PM
  POLICY[Shared expression safety policy] --> LCSE[LocalCSE]
  POLICY --> GCSE[Dominance-aware GlobalCSE]
  POLICY --> LICM[Restricted LICM]
  PM --> LCSE --> GCSE --> LICM --> V[LLVM verifier]
  V --> DIFF[Differential execution]
  V --> BACKEND[x86-64 / AArch64 codegen]
  V --> BENCH[HPC campaign]
```

The shared policy in `lib/Expression.cpp` rejects floating point, memory
operations, literal poison/undef, constants containing poison/undef, and
bitwise `or` (whose `disjoint` semantics vary across LLVM versions). It compares
`nuw`, `nsw`, and `exact` explicitly.

GlobalCSE walks the dominator tree with scoped available-expression stacks. It
does not implement PHI translation, partial redundancy elimination, or memory
value numbering. Forge LICM moves only eligible integer binary operations from
innermost loops when a preheader exists and LLVM reports speculation safe.

See [correctness](docs/CORRECTNESS.md), [workflow](docs/DEVELOPMENT_WORKFLOW.md),
and the [v1 audit](docs/AUDIT_V1.md).

## Build and test

```bash
cmake -S . -B build -G Ninja \
  -DLLVM_DIR="$(llvm-config --cmakedir)" \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The repository also supplies `CMakePresets.json` for the Ubuntu LLVM 18
container and `Dockerfile.ci` for reproducibility.

```bash
opt -load-pass-plugin ./build/lib/ForgePass.so \
  -passes='forge-local-cse,forge-global-cse,forge-licm,verify' \
  -S input.ll -o output.ll

./build/tools/forge-analyze/forge-analyze input.ll
```

`forge-analyze` emits JSON containing function, block, instruction, eligible
expression, and local opportunity counts plus the configured pipeline.

## Static backend analysis

```bash
python3 analysis/analyze_backends.py \
  --clang clang-18 --llvm-mca /usr/lib/llvm-18/bin/llvm-mca
```

This produces ignored raw artifacts and a manifest under `analysis/results/`.
It never executes AArch64 output on an x86-64 host. See
[AARCH64.md](docs/AARCH64.md) for the evidence boundary.

## HPC benchmark campaign

```bash
python3 benchmark/scripts/run_campaign.py \
  --clang clang-18 --opt opt \
  --plugin ./build/lib/ForgePass.so \
  --size 200000 --trials 9
```

The campaign exercises matrix multiplication, reduction, dot product, stencil,
strided traversal, repeated arithmetic, and a loop recurrence. It records raw
trial timings, optimizer time, binary size, toolchain and host metadata, pass
pipelines, and validation digests for minimal, O2, and O3 baselines with and
without Forge. Benchmark output is rejected if any pipeline's digest differs.

Performance claims require a full campaign on named native hardware. CI is for
correctness and build reproducibility, not benchmark comparisons.

## Repository map

- `include/forge/` — pass and shared expression contracts
- `lib/` — plugin registration and three pass implementations
- `tools/forge-analyze/` — native LLVM IR reporting CLI
- `test/` — structural, generated, and differential validation
- `analysis/` — x86-64/AArch64 assembly and `llvm-mca` workflow
- `benchmark/` — HPC kernels and reproducible campaign scripts
- `docs/` — audit, correctness, architecture, workflow, and interview material

## Known limitations

- No memory expression elimination, floating-point transformations, PHI
  translation, PRE, or complete GVN.
- LICM neither creates preheaders nor moves memory operations.
- No general-purpose loop unroller; adding one before stronger correctness
  evidence would broaden risk without deepening the core project.
- Alive2 refinement checks are not yet integrated.
- No native AArch64 or NVIDIA Grace measurements have been made.

## Resume bullet

Developed a modular C++17 LLVM 18 optimization toolkit with flag-aware local
CSE, dominator-tree global CSE, and speculation-checked integer LICM; added
FileCheck/verifier regressions, seeded and differential execution tests,
AArch64/Neoverse static analysis, a seven-kernel HPC campaign, a native IR
analysis CLI, container builds, and CI-ready reproducibility.

## License

MIT
