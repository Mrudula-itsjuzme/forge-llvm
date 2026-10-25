#!/usr/bin/env python3
"""Generate static x86-64/AArch64 assembly and optional llvm-mca reports."""
import argparse
import json
from pathlib import Path
import platform
import re
import shutil
import subprocess


def assembly_metrics(path):
    instructions = []
    vector_instructions = 0
    sve_instructions = 0
    stack_accesses = 0
    vector_registers = set()
    register_pattern = re.compile(r"%(?:xmm|ymm|zmm)(\d+)|\b([vpz])(\d+)(?:\.[0-9a-z]+)?")
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if (not line or line.startswith((".", "#", "//", ";"))
                or line.endswith(":") or "\t" not in raw):
            continue
        instructions.append(line)
        registers = list(register_pattern.finditer(line))
        if registers:
            vector_instructions += 1
            for match in registers:
                vector_registers.add(match.group(0).split(".")[0])
        if any(match.group(2) in ("z", "p") for match in registers):
            sve_instructions += 1
        if re.search(r"\(%r(?:sp|bp)\)|\[sp(?:,|\])", line):
            stack_accesses += 1
    return {
        "static_instruction_count": len(instructions),
        "vector_instruction_count": vector_instructions,
        "sve_instruction_count": sve_instructions,
        "distinct_vector_registers": len(vector_registers),
        "stack_access_instruction_count": stack_accesses,
        "register_pressure_proxy": (
            "distinct vector registers in whole assembly; not liveness analysis"),
    }


def mca_summary(text):
    patterns = {
        "iterations": r"^Iterations:\s+([0-9]+)",
        "instructions": r"^Instructions:\s+([0-9]+)",
        "total_cycles": r"^Total Cycles:\s+([0-9]+)",
        "total_uops": r"^Total uOps:\s+([0-9]+)",
        "ipc": r"^IPC:\s+([0-9.]+)",
        "block_rthroughput": r"^Block RThroughput:\s+([0-9.]+)",
    }
    result = {}
    for name, pattern in patterns.items():
        match = re.search(pattern, text, re.MULTILINE)
        if match:
            result[name] = float(match.group(1)) if "." in match.group(1) else int(match.group(1))
    return result

p = argparse.ArgumentParser()
p.add_argument("--clang", default="clang")
p.add_argument("--llvm-mca", default="llvm-mca")
p.add_argument("--output", default="analysis/results")
p.add_argument("--commit", default="unrecorded")
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
    "schema_version": 1,
    "commit": a.commit,
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
    item = {"command": cmd, "assembly": str(asm.relative_to(root)),
            "compiled": run.returncode == 0}
    if run.returncode:
        item["error"] = run.stderr.strip()
    else:
        item["metrics"] = assembly_metrics(asm)
    if not run.returncode and shutil.which(a.llvm_mca) and name != "aarch64-sve2":
        mca = out / f"{name}.mca.txt"
        mca_cmd = [a.llvm_mca]
        if name.startswith("aarch64"):
            mca_cmd += ["-mtriple=aarch64", "-mcpu=neoverse-v2"]
        mca_run = subprocess.run([*mca_cmd, asm], capture_output=True, text=True)
        mca_text = mca_run.stdout + mca_run.stderr
        mca.write_text(mca_text)
        item.update({"llvm_mca": str(mca.relative_to(root)),
                     "llvm_mca_exit": mca_run.returncode,
                     "llvm_mca_summary": mca_summary(mca_text)})
    report["targets"][name] = item

(out / "manifest.json").write_text(json.dumps(report, indent=2) + "\n")
(out / "report.md").write_text("\n".join([
    "# Static backend analysis", "",
    f"Commit: `{a.commit}`; compiler: `{report['clang']}`; host: `{report['host']}`.", "",
    "| Target | Compiled | Instructions | Vector instructions | SVE instructions | Distinct vector registers | Stack accesses | MCA IPC | MCA throughput |",
    "|---|---:|---:|---:|---:|---:|---:|---:|---:|",
    *[
        f"| {name} | {item['compiled']} | "
        f"{item.get('metrics', {}).get('static_instruction_count', 'n/a')} | "
        f"{item.get('metrics', {}).get('vector_instruction_count', 'n/a')} | "
        f"{item.get('metrics', {}).get('sve_instruction_count', 'n/a')} | "
        f"{item.get('metrics', {}).get('distinct_vector_registers', 'n/a')} | "
        f"{item.get('metrics', {}).get('stack_access_instruction_count', 'n/a')} | "
        f"{item.get('llvm_mca_summary', {}).get('ipc', 'n/a')} | "
        f"{item.get('llvm_mca_summary', {}).get('block_rthroughput', 'n/a')} |"
        for name, item in report["targets"].items()
    ],
    "", "## Evidence boundary", "",
    "All results are static code-generation or scheduling-model evidence. Distinct "
    "vector-register count is a coarse whole-file proxy, not peak live-register "
    "pressure. Stack-access counts are syntactic and do not prove spills. AArch64 "
    "assembly was not executed on this x86-64 host; llvm-mca is not hardware timing.", "",
]))
print(json.dumps(report, indent=2))
if not all(item["compiled"] for item in report["targets"].values()):
    raise SystemExit("one or more static backend compilations failed")
