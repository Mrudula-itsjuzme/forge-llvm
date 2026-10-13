# Development workflow

Forge uses an evidence-gated workflow designed for compiler transformations,
where a build success is much weaker evidence than semantic correctness.

## Branch and checkpoint policy

1. Start from a green `main` and work on a named feature branch.
2. Write or update the safety contract before broadening a transformation.
3. Refactor without behavior changes and rerun the old corpus.
4. Add a minimal positive case and at least one prohibited-transformation case.
5. Implement the narrowest rule that satisfies those tests.
6. Run FileCheck, the verifier, generated tests, and differential execution.
7. Commit one reviewable logical checkpoint. Do not combine unrelated cleanup.
8. Run static backend analysis and benchmarks only after correctness gates pass.
9. Publish raw results and environment metadata; review claims separately.
10. Merge only after hosted CI independently reproduces build and tests.

## Evidence ladder

| Level | Evidence | What it establishes |
|---|---|---|
| 1 | Compile and plugin load | API/build compatibility |
| 2 | FileCheck | Intended structural IR change |
| 3 | LLVM verifier | Structural IR validity |
| 4 | Negative regressions | Known unsafe cases remain unchanged |
| 5 | Seeded generated IR | Broader crash/verifier resistance |
| 6 | Differential execution | Matching outputs for sampled defined inputs |
| 7 | Alive2 refinement | Formal refinement for supported IR fragments |
| 8 | Native benchmark campaign | Performance on the named hardware/configuration |

No lower level is described as proof of a higher one. In particular, verifier
success is not semantic equivalence, differential testing is not formal proof,
and `llvm-mca` is a static scheduling estimate rather than runtime evidence.

## Review checklist for a new pass

- Is the transformation's scope named more narrowly than the LLVM pass it may
  resemble?
- Does the old instruction dominate every replacement use?
- Are types, operands, poison-generating flags, and relevant metadata handled?
- Can the instruction trap, read/write memory, converge, or otherwise depend on
  where it executes?
- Are `undef`, poison, freeze, PHIs, unreachable blocks, and loop exits covered?
- Is `PreservedAnalyses` accurate?
- Is complexity documented and bounded?
- Are pass order and baseline comparisons fair?
- Is every reported measurement traceable to raw data and environment metadata?

## Debugging commands

```bash
opt -load-pass-plugin build/lib/ForgePass.so \
  -passes='forge-global-cse,verify' -S input.ll -o -

opt -debug-pass-manager -load-pass-plugin build/lib/ForgePass.so \
  -passes='forge-local-cse,forge-global-cse,forge-licm,verify' \
  -disable-output input.ll

ctest --test-dir build --output-on-failure
```

Use `-stats` with a statistics-enabled LLVM build to inspect Forge's local,
global, and loop transformation counters.

