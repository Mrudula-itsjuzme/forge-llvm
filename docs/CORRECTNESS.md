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

Forge v3 also generates 250 deterministic mutation cases per CI run, including
commuted operands, CFG diamonds, semantic flags on range-limited operands, and
SSA replacement across dominated blocks. It compiles and executes the original
and transformed modules and compares digests. A failure preserves the seed,
input IR, transformed IR, harness, outputs, and reproduction metadata under the
configured failure directory.

The Alive2 bridge produces a status JSON. Missing `alive-tv` exits with CTest's
skip code and records `unavailable`; unsupported or failed checks remain
failures/inconclusive and never become proof claims. In the current container,
Alive2 is unavailable.

The sanitizer-backed differential test is similarly dependency-aware. CI
installs `libclang-rt-18-dev`; the current local image lacks that runtime because
the image rebuild was blocked by a stalled package mirror, so the local result
is explicitly SKIPPED rather than passed.
