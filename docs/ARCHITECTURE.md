# Architecture

Clang performs lexing, parsing, semantic analysis, and LLVM IR generation.
Forge implements no frontend. LLVM's middle end runs Forge's New Pass Manager
plugin over that IR, and existing LLVM backends perform instruction selection,
register allocation, scheduling, and assembly emission. Forge invokes those
backends for analysis but does not implement backend internals.

## SSA, CFGs, and dominance

In static single assignment form each SSA name has one definition. Basic blocks
form a control-flow graph, and PHIs select values based on incoming edges. A
definition dominates a use when every path to that use passes through the
definition. This is the availability condition that separates GlobalCSE from a
syntactic whole-function scan.

GlobalCSE performs a depth-first dominator-tree walk. Each expression key maps
to a stack. Entering a dominated block pushes newly available definitions;
leaving its subtree pops them. Sibling CFG regions therefore cannot incorrectly
reuse one another's values.

## Pass pipeline

```mermaid
sequenceDiagram
  participant opt
  participant Plugin
  participant LocalCSE
  participant GlobalCSE
  participant LICM
  opt->>Plugin: Parse pipeline names
  Plugin->>LocalCSE: Function pass
  LocalCSE->>GlobalCSE: CFG preserved
  GlobalCSE->>GlobalCSE: Query DominatorTree
  GlobalCSE->>LICM: CFG and DominatorTree preserved
  LICM->>LICM: Query LoopInfo and DominatorTree
  LICM-->>opt: Preserve CFG, LoopInfo, DominatorTree
```

The transformations change instructions but not edges or blocks. LocalCSE
preserves the CFG analysis set. GlobalCSE additionally preserves the
DominatorTree it consumed. LICM moves instructions without changing the CFG and
preserves LoopInfo and DominatorTree. Other analyses are invalidated.

## Relationship to LLVM passes

LLVM EarlyCSE and GVN support broader equivalence reasoning and memory-aware
optimization. LLVM LICM includes alias analysis, promotion, profitability, and
more complete control-flow handling. Forge's smaller contracts make the
algorithms suitable for audit and experimentation, while standard pipelines are
the production baseline in every benchmark comparison.

