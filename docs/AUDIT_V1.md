# Forge v1 architectural audit

Baseline: commit `fa4d8a4`, LLVM 18.1.3 container validation on 2026-10-13.

## Verified baseline

- The pass plugin compiled and linked.
- All four original CTest groups passed.
- The plugin used the New Pass Manager and preserved the CFG.
- Semantic overflow flags and literal poison/undef had regression coverage.

## Strengths to preserve

- The transformation was deliberately narrow: integer binary expressions in
  one basic block, exact semantic-flag matching, no memory operations.
- FileCheck, the LLVM verifier, seeded generated IR, container builds, and
  hosted CI already formed a useful validation base.
- Benchmark output equality was checked before timings were accepted.

## Architectural limitations

- Registration, equivalence policy, and transformation code lived in one C++
  file, making additional passes likely to duplicate subtle safety rules.
- No dominance-aware availability model existed.
- No loop analysis was consumed.
- The test runner hard-coded one pipeline name.
- Benchmark coverage was a single arithmetic kernel and was not a campaign.
- There was no differential execution suite, AArch64 inspection workflow, or
  native analysis CLI.

## Correctness risks identified

- LLVM's `isSameOperationAs` does not by itself establish equality of all
  poison-generating flags; v1 already compensated for LLVM 18 `nuw`, `nsw`,
  and `exact`, and that policy must remain shared across passes.
- Future LLVM versions add semantics such as `or disjoint`; Forge excludes
  bitwise `or` until the relevant flag can be compared portably.
- Cross-block elimination is valid only when the selected definition dominates
  every replaced use; syntactic equality is insufficient.
- Hoisting needs a real preheader, invariant operands, and LLVM's speculation
  safety query. Memory operations and arbitrary calls remain out of scope.

## v2 direction

The v2 refactor centralizes expression eligibility and equivalence in
`Expression.cpp`, keeps plugin registration separate, and exposes three
independently schedulable passes: LocalCSE, dominance-aware GlobalCSE, and a
restricted integer LICM. Each pass receives its own positive and negative
regression corpus.
