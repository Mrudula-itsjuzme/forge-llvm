#!/usr/bin/env python3
"""Generate static x86-64/AArch64 assembly and optional llvm-mca reports."""
import argparse
import json
from pathlib import Path
import platform
import shutil
import subprocess

p = argparse.ArgumentParser()
p.add_argument("--clang", default="clang")
p.add_argument("--llvm-mca", default="llvm-mca")
p.add_argument("--output", default="analysis/results")
a = p.parse_args()

root = Path(__file__).resolve().parents[1]
source = root / "analysis" / "backend_kernels.c"
out = root / a.output
out.mkdir(parents=True, exist_ok=True)

targets = {
    "x86_64": ["-target", "x86_64-unknown-linux-gnu", "-march=x86-64-v3"],
    "aarch64-neoverse-v2": ["-target", "aarch64-unknown-linux-gnu",
                            "-mcpu=neoverse-v2"],
    "aarch64-sve2": ["-target", "aarch64-unknown-linux-gnu",
                     "-march=armv9-a+sve2"],
}
report = {
    "host": platform.machine(),
    "clang": subprocess.run([a.clang, "--version"], check=True,
                            capture_output=True, text=True).stdout.splitlines()[0],
    "targets": {},
    "evidence": "static-code-generation-only",
}

for name, flags in targets.items():
    asm = out / f"{name}.s"
    cmd = [a.clang, "-O3", "-ffreestanding", "-fno-math-errno", *flags,
           "-S", str(source), "-o", str(asm)]
    run = subprocess.run(cmd, capture_output=True, text=True)
    item = {"command": cmd, "assembly": str(asm), "compiled": run.returncode == 0}
    if run.returncode:
        item["error"] = run.stderr.strip()
    elif shutil.which(a.llvm_mca) and name != "aarch64-sve2":
        mca = out / f"{name}.mca.txt"
        mca_cmd = [a.llvm_mca]
        if name.startswith("aarch64"):
            mca_cmd += ["-mtriple=aarch64", "-mcpu=neoverse-v2"]
        mca_run = subprocess.run([*mca_cmd, asm], capture_output=True, text=True)
        mca.write_text(mca_run.stdout + mca_run.stderr)
        item.update({"llvm_mca": str(mca), "llvm_mca_exit": mca_run.returncode})
    report["targets"][name] = item

(out / "manifest.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
