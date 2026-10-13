# Correctness model

## Shared expression contract

LocalCSE and GlobalCSE accept only LLVM integer binary operators with matching
opcode, result type, canonical operands, and semantic flags. Literal undef,
literal poison, constants containing undef/poison elements, and bitwise `or`
are rejected. `or` remains excluded because LLVM versions differ in support for
the semantic `disjoint` flag.

LocalCSE reuses only earlier definitions in one block. GlobalCSE walks the
dominator tree and maintains scoped available-expression stacks; a candidate is
visible only to its dominated subtree. The walk is approximately O(I * C),
where I is eligible instructions and C is the typically tiny collision list for
one expression key. It is not a complete GVN implementation.

## Restricted LICM contract

Forge LICM considers only eligible integer binary operators in innermost loops.
It requires a real loop preheader, loop-invariant operands, and approval from
LLVM's `isSafeToSpeculativelyExecute`. It does not move loads, stores, calls,
atomics, convergent operations, or arbitrary divisions. Outer-loop motion and
preheader creation are deliberately deferred.

## Evidence and limitations

The suite checks structural output, verifier validity, negative cases, 100
seeded generated modules, and executable differential behavior over boundary
and seeded inputs. These tests are empirical. Alive2 integration is deferred
until a compatible binary is available and must be reported separately from
the current evidence.

