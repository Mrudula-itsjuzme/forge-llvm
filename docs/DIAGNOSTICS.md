# Explainable optimization diagnostics

Forge diagnostics are opt-in so report serialization and pass timers do not run
inside normal benchmark pipelines.

```bash
opt -load-pass-plugin build/lib/ForgePass.so \
  -forge-diagnostics-jsonl=diagnostics.jsonl \
  -pass-remarks='forge-.*' \
  -passes='forge-local-cse,forge-global-cse,forge-licm,verify' \
  -disable-output input.ll
```

Each JSONL record is schema version 1 and identifies one pass/function pair.
Fields include:

- IR instructions before, after, and delta;
- expressions examined and eliminated;
- dominating cross-block reuses;
- loop-invariant candidates and instructions hoisted;
- elapsed pass time; and
- categorized rejection counts.

Current rejection categories include non-integer types, unsupported `or`
semantics, undef/poison operands, unavailable or non-dominating equivalents,
semantic-flag mismatch, variant loop operands, unsafe speculation, absent loop
preheaders, and intentionally skipped non-innermost loops.

Successful transformations also use LLVM optimization remarks. JSONL is the
machine-readable experiment input; remarks are intended for source/IR-oriented
developer inspection.

## Performance boundary

The diagnostics output option is absent from timed benchmark commands. When it
is disabled Forge does not start its diagnostics timers, increment diagnostics
counters, allocate rejection maps, or write files. Small condition checks remain
at instrumentation sites, so any claim about zero overhead would still require
measurement. Diagnostic-run elapsed times describe pass behavior but must not be
mixed with non-diagnostic compilation-overhead results.
