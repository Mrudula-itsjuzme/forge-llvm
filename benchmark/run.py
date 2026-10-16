#!/usr/bin/env python3
"""Build paired pipelines and report real wall-clock measurements as JSON."""
import argparse
import json
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import time

from validation import validate_trial_output


def count_ir_instructions(path):
    """Count textual instructions inside function bodies (not machine instructions)."""
    count = 0
    in_function = False
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if line.startswith("define "):
            in_function = True
            continue
        if in_function and line == "}":
            in_function = False
            continue
        if (in_function and line and not line.startswith(";")
                and not line.endswith(":")):
            count += 1
    return count

p = argparse.ArgumentParser()
p.add_argument("--clang", default="clang")
p.add_argument("--opt", default="opt")
p.add_argument("--plugin", required=True)
p.add_argument("--trials", type=int, default=9)
p.add_argument("--iterations", type=int, default=200_000_000)
p.add_argument("--output", default="benchmark/results/latest.json")
a = p.parse_args()

root = Path(__file__).resolve().parents[1]
work = root / "build" / "benchmark"
work.mkdir(parents=True, exist_ok=True)
source = root / "benchmark" / "kernel.c"
raw_ir = work / "kernel.ll"

subprocess.run([a.clang, "-O0", "-Xclang", "-disable-O0-optnone", "-S",
                "-emit-llvm", source, "-o", raw_ir], check=True)

pipelines = {
    "O0": "mem2reg",
    "O0+Forge": "mem2reg,forge-redundant-expr-elim",
    "O2": "default<O2>",
    "O2+Forge": "default<O2>,function(forge-redundant-expr-elim)",
    "O3": "default<O3>",
    "O3+Forge": "default<O3>,function(forge-redundant-expr-elim)",
}
results = {"trials": a.trials, "iterations": a.iterations, "pipelines": {}}
expected = None

for name, pipeline in pipelines.items():
    stem = name.replace("+", "-")
    optimized_ir = work / f"{stem}.ll"
    binary = work / stem
    opt_cmd = [a.opt, "-load-pass-plugin", a.plugin, f"-passes={pipeline},verify",
               "-S", raw_ir, "-o", optimized_ir]
    started = time.perf_counter_ns()
    subprocess.run(opt_cmd, check=True)
    opt_ns = time.perf_counter_ns() - started
    subprocess.run([a.clang, "-O0", optimized_ir, "-o", binary], check=True)

    times = []
    output = None
    for trial in range(a.trials):
        started = time.perf_counter_ns()
        run = subprocess.run([binary, str(a.iterations)], check=True,
                             capture_output=True, text=True)
        times.append((time.perf_counter_ns() - started) / 1e9)
        output = run.stdout.strip()
        expected = validate_trial_output(expected, output, name, trial)
    results["pipelines"][name] = {
        "median_seconds": statistics.median(times),
        "all_seconds": times,
        "opt_milliseconds": opt_ns / 1e6,
        "binary_bytes": os.path.getsize(binary),
        "ir_instruction_count": count_ir_instructions(optimized_ir),
        "output": output,
    }

out = root / a.output
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(results, indent=2) + "\n")
print(json.dumps(results, indent=2))
