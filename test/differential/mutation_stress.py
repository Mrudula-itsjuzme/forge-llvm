#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
import random
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument("--clang", required=True)
p.add_argument("--opt", required=True)
p.add_argument("--plugin", required=True)
p.add_argument("--seed", type=int, required=True)
p.add_argument("--cases", type=int, default=1000)
p.add_argument("--failure-dir", required=True)
a = p.parse_args()
rng = random.Random(a.seed)
ops = ["add", "mul", "xor", "and"]


def preserve(tmp, reason, before=None, after=None):
    target = Path(a.failure_dir) / f"seed-{a.seed}"
    target.mkdir(parents=True, exist_ok=True)
    for name in ("mutations.ll", "harness.c", "transformed.ll"):
        source = Path(tmp) / name
        if source.exists():
            shutil.copy2(source, target / name)
    metadata = {"seed": a.seed, "cases": a.cases, "reason": reason,
                "before_stdout": before, "after_stdout": after,
                "reproduce": "rerun mutation_stress.py with the recorded seed"}
    (target / "failure.json").write_text(json.dumps(metadata, indent=2) + "\n")
    raise SystemExit(f"FAIL: preserved reproduction at {target}: {reason}")


with tempfile.TemporaryDirectory(prefix="forge-mutations-") as tmp_name:
    tmp = Path(tmp_name)
    module = []
    prototypes = []
    calls = []
    for index in range(a.cases):
        op = rng.choice(ops)
        commuted = rng.choice([True, False])
        flags = " nuw" if op in {"add", "mul"} and rng.randrange(5) == 0 else ""
        rhs = "%bb, %aa" if commuted else "%aa, %bb"
        # The root dominates both arms. The duplicate must be reusable while
        # the branch-local mix and PHI stress CFG traversal and SSA replacement.
        module.append(f"""define i32 @case_{index}(i32 %a, i32 %b, i1 %c) {{
entry:
  %aa = and i32 %a, 65535
  %bb = and i32 %b, 65535
  %root = {op}{flags} i32 %aa, %bb
  br i1 %c, label %left, label %right
left:
  %l = {op}{flags} i32 {rhs}
  %lmix = xor i32 %l, {rng.randrange(0, 2**16)}
  br label %merge
right:
  %r = {op}{flags} i32 %aa, %bb
  %rmix = xor i32 %r, {rng.randrange(0, 2**16)}
  br label %merge
merge:
  %selected = phi i32 [ %lmix, %left ], [ %rmix, %right ]
  %out = add i32 %selected, %root
  ret i32 %out
}}
""")
        prototypes.append(f"extern uint32_t case_{index}(uint32_t,uint32_t,_Bool);")
        calls.append(f"  hash = mix(hash, case_{index}(a ^ {index}U, b + {index}U, (seed >> {index % 31}) & 1U));")

    ir = tmp / "mutations.ll"
    ir.write_text("\n".join(module))
    harness = tmp / "harness.c"
    harness.write_text("""#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
""" + "\n".join(prototypes) + """
static uint64_t mix(uint64_t h, uint32_t v) { return (h ^ v) * UINT64_C(1099511628211); }
int main(int argc, char **argv) {
  uint32_t seed = argc > 1 ? (uint32_t)strtoul(argv[1], 0, 10) : 1U;
  uint32_t a = seed * 1664525U + 1013904223U;
  uint32_t b = a * 22695477U + 1U;
  uint64_t hash = UINT64_C(1469598103934665603);
""" + "\n".join(calls) + """
  printf("%" PRIu64 "\\n", hash);
  return 0;
}
""")
    transformed = tmp / "transformed.ll"
    before = tmp / "before"
    after = tmp / "after"
    try:
        subprocess.run([a.opt, "-load-pass-plugin", a.plugin,
                        "-passes=forge-local-cse,forge-global-cse,verify", "-S",
                        ir, "-o", transformed], check=True, capture_output=True)
        subprocess.run([a.clang, "-O0", ir, harness, "-o", before], check=True,
                       capture_output=True)
        subprocess.run([a.clang, "-O0", transformed, harness, "-o", after],
                       check=True, capture_output=True)
    except subprocess.CalledProcessError as error:
        preserve(tmp, f"tool failure: {error.stderr.decode(errors='replace')}")
    baseline = subprocess.run([before, str(a.seed)], check=True,
                              capture_output=True, text=True).stdout
    optimized = subprocess.run([after, str(a.seed)], check=True,
                               capture_output=True, text=True).stdout
    if baseline != optimized:
        preserve(tmp, "output mismatch", baseline, optimized)
    print(f"PASS: {a.cases} mutation cases, seed={a.seed}, digest={baseline.strip()}")
