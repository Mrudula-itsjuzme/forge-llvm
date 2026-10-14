# AArch64 and Grace-oriented analysis

`analysis/analyze_backends.py` compiles the same freestanding kernels for
x86-64-v3, AArch64 Neoverse V2, and generic Armv9-A+SVE2. When `llvm-mca` is
available, it also records scheduling-model output for x86-64 and Neoverse V2.

```bash
python3 analysis/analyze_backends.py \
  --clang clang-18 --llvm-mca /usr/lib/llvm-18/bin/llvm-mca \
  --commit "$(git rev-parse HEAD)"
```

The script writes assembly, `llvm-mca` text, a JSON manifest, and a Markdown
summary under the ignored `analysis/results/` directory. The manifest records
static instruction counts, instructions referencing vector registers, SVE
instructions, distinct vector registers, syntactic stack accesses, and selected
`llvm-mca` summary fields. Generated AArch64 programs are never executed on the
x86-64 development host.

Neoverse V2 is used as a relevant static scheduling model for Grace-oriented
study. This is not evidence from NVIDIA Grace hardware. SVE2 output indicates
code-generation capability only; actual vectorization depends on loop legality,
cost models, alignment, and target features. Runtime, thermal, memory-system,
and frequency behavior require native measurements on explicitly identified
hardware.

The distinct vector-register count is a whole-file proxy, not liveness-based
register pressure. Stack accesses are not automatically spills. These metrics
support code-shape inspection but cannot replace compiler remarks, register
allocation diagnostics, or native hardware counters.
