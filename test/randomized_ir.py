#!/usr/bin/env python3
import argparse
import random
import subprocess
import tempfile
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument("--opt", required=True)
p.add_argument("--plugin", required=True)
p.add_argument("--seed", type=int, required=True)
a = p.parse_args()
rng = random.Random(a.seed)
ops = ["add", "sub", "mul", "and", "xor"]

with tempfile.TemporaryDirectory(prefix="forge-random-") as d:
    for case in range(100):
        op = rng.choice(ops)
        reverse = rng.choice([False, True]) and op in {"add", "mul", "and", "xor"}
        rhs = "%b, %a" if reverse else "%a, %b"
        text = f"""define i32 @case_{case}(i32 %a, i32 %b) {{
entry:
  %x = {op} i32 %a, %b
  %y = {op} i32 {rhs}
  %r = xor i32 %x, %y
  ret i32 %r
}}
"""
        path = Path(d) / f"case-{case}.ll"
        path.write_text(text)
        subprocess.run(
            [a.opt, "-load-pass-plugin", a.plugin,
             "-passes=forge-redundant-expr-elim,verify", "-disable-output", path],
            check=True)
print(f"PASS: 100 verifier cases, seed={a.seed}")
