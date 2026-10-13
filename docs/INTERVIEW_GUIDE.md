# Forge interview guide

## Why is GlobalCSE not called GVN?

It performs scoped expression reuse along the dominator tree. It has no complete
value-numbering lattice, memory dependence model, PHI translation, or PRE, so
calling it LLVM GVN would overstate the implementation. See `GlobalCSE.cpp`.

## Why is dominance required?

SSA replacement is valid only if the reused definition is available on every
path to each replaced use. Sibling diamond arms do not dominate one another or
the merge. The scoped dominator-tree walk encodes exactly that availability.

## Why compare `nuw`, `nsw`, and `exact`?

Those flags change when an instruction produces poison. Replacing an unflagged
instruction with a flagged one can therefore change defined behavior. Forge
compares them explicitly in `Expression.cpp`.

## Why can calls remain between two CSE candidates?

Eligible expressions consume immutable SSA operands and do not access memory.
A call can change memory, but not the values of existing SSA definitions.

## Why is Forge LICM so restricted?

Moving an instruction changes whether and how often it executes. Forge requires
a preheader, invariant operands, and LLVM's speculation-safety query, and moves
no memory operations. This creates a reviewable safety boundary before adding
alias analysis or control-flow-sensitive execution reasoning.

## What do the tests prove?

FileCheck proves expected IR shape for the fixtures; the verifier proves IR
well-formedness; generated tests broaden robustness; differential tests compare
sampled executions. None is a universal semantic proof. Alive2 would add formal
refinement evidence for supported fragments.

## Why might Forge have no effect after `-O2`?

LLVM already schedules stronger EarlyCSE, GVN, and LICM implementations. Forge
is educational and auditable; its incremental benefit depends on pass placement
and IR patterns not already consumed by the standard pipeline.

