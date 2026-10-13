#!/usr/bin/env python3
import argparse
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import time

p = argparse.ArgumentParser()
p.add_argument("--clang", default="clang")
p.add_argument("--opt", default="opt")
p.add_argument("--plugin", required=True)
p.add_argument("--size", type=int, default=200_000)
p.add_argument("--trials", type=int, default=9)
p.add_argument("--output", default="benchmark/results/campaign.json")
a = p.parse_args()

root = Path(__file__).resolve().parents[2]
work = root / "build" / "campaign"
work.mkdir(parents=True, exist_ok=True)
source = root / "benchmark" / "kernels" / "hpc_suite.c"
raw = work / "suite.ll"
subprocess.run([a.clang, "-O0", "-Xclang", "-disable-O0-optnone", "-S",
                "-emit-llvm", source, "-o", raw], check=True)

pipelines = {
    "minimal": "mem2reg",
    "minimal+forge-v2": "mem2reg,forge-local-cse,forge-global-cse,forge-licm",
    "O2": "default<O2>",
    "O2+forge-v2": "default<O2>,function(forge-local-cse,forge-global-cse,forge-licm)",
    "O3": "default<O3>",
    "O3+forge-v2": "default<O3>,function(forge-local-cse,forge-global-cse,forge-licm)",
}
report = {
    "host_architecture": platform.machine(),
    "host_processor": platform.processor() or "unavailable",
    "size": a.size,
    "trials": a.trials,
    "toolchain": subprocess.run([a.clang, "--version"], check=True,
                                capture_output=True, text=True).stdout.splitlines()[0],
    "pipelines": {},
}
reference_digest = None
for name, pipeline in pipelines.items():
    safe = name.replace("+", "-")
    ir = work / f"{safe}.ll"
    exe = work / safe
    started = time.perf_counter_ns()
    subprocess.run([a.opt, "-load-pass-plugin", a.plugin,
                    f"-passes={pipeline},verify", "-S", raw, "-o", ir], check=True)
    opt_ns = time.perf_counter_ns() - started
    subprocess.run([a.clang, "-O0", ir, "-o", exe], check=True)
    run = subprocess.run([exe, str(a.size), str(a.trials)], check=True,
                         capture_output=True, text=True)
    digest_match = re.search(r"^VALIDATION,(\d+)$", run.stdout, re.MULTILINE)
    if not digest_match:
        raise RuntimeError(f"missing validation digest for {name}")
    digest = digest_match.group(1)
    if reference_digest is None:
        reference_digest = digest
    if digest != reference_digest:
        raise RuntimeError(f"output mismatch for {name}: {digest} != {reference_digest}")
    rows = []
    for line in run.stdout.splitlines():
        if line.startswith("RESULT,"):
            _, kernel, trial, ns, value = line.split(",")
            rows.append({"kernel": kernel, "trial": int(trial),
                         "nanoseconds": int(ns), "value": value})
    report["pipelines"][name] = {
        "pipeline": pipeline,
        "optimizer_nanoseconds": opt_ns,
        "binary_bytes": os.path.getsize(exe),
        "validation_digest": digest,
        "validated": True,
        "raw_trials": rows,
    }

destination = root / a.output
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps({"output": str(destination), "validation": reference_digest,
                  "pipelines": list(pipelines)}, indent=2))

