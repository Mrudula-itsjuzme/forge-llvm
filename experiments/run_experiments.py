#!/usr/bin/env python3
import argparse
import csv
from datetime import datetime, timezone
import json
import hashlib
import os
from pathlib import Path
import platform
import random
import shutil
import statistics
import subprocess
import sys
import time


def instruction_count(path):
    count = 0
    inside = False
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if line.startswith("define "):
            inside = True
        elif inside and line == "}":
            inside = False
        elif inside and line and not line.startswith(";") and not line.endswith(":"):
            count += 1
    return count


def assembly_instruction_count(path):
    count = 0
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if (line and not line.startswith((".", "#", ";"))
                and not line.endswith(":") and "\t" in raw):
            count += 1
    return count


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def version(command):
    run = subprocess.run([command, "--version"], check=True,
                         capture_output=True, text=True)
    return run.stdout.splitlines()[0]


def host_metadata():
    data = {"architecture": platform.machine(), "processor": platform.processor()}
    lscpu = subprocess.run(["lscpu", "-J"], capture_output=True, text=True)
    if lscpu.returncode == 0:
        data["lscpu"] = json.loads(lscpu.stdout)
    governor = Path("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor")
    data["scaling_governor"] = governor.read_text().strip() if governor.exists() else "unavailable"
    data["performance_counters"] = (
        {"tool": shutil.which("perf"), "status": "available_unprobed"}
        if shutil.which("perf") else {"status": "unavailable"})
    return data


p = argparse.ArgumentParser()
p.add_argument("--clang", default="clang")
p.add_argument("--opt", default="opt")
p.add_argument("--plugin", required=True)
p.add_argument("--commit", required=True)
p.add_argument("--dirty", action="store_true",
               help="Record that the source tree had uncommitted changes")
p.add_argument("--size", type=int, default=2_000_000)
p.add_argument("--trials", type=int, default=15)
p.add_argument("--schedules", default="experiments/config/schedules.json")
p.add_argument("--workloads", default="experiments/config/workloads.json")
p.add_argument("--output-root", default="experiments/runs")
p.add_argument("--execution-affinity", default="uncontrolled")
a = p.parse_args()

root = Path(__file__).resolve().parents[1]
schedules_doc = json.loads((root / a.schedules).read_text())
workloads = json.loads((root / a.workloads).read_text())
expected_workloads = workloads["tuning"] + workloads["evaluation"]
timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
experiment_id = f"{timestamp}-{a.commit[:8]}"
out = root / a.output_root / experiment_id
artifacts = out / "artifacts"
artifacts.mkdir(parents=True)
source = root / "benchmark" / "kernels" / "hpc_suite.c"
raw_ir = artifacts / "hpc-suite-raw.ll"

subprocess.run([a.clang, "-O0", "-Xclang", "-disable-O0-optnone", "-S",
                "-emit-llvm", source, "-o", raw_ir], check=True)

manifest = {
    "schema_version": 1,
    "experiment_id": experiment_id,
    "status": "incomplete",
    "commit": a.commit,
    "dirty_worktree": a.dirty,
    "toolchain": {"clang": version(a.clang), "opt": version(a.opt)},
    "hardware": host_metadata(),
    "invocation": sys.argv,
    "dataset": {"source": str(source.relative_to(root)), "size": a.size,
                "trials": a.trials, "split": workloads},
    "seed": workloads["seed"],
    "measurement_protocol": {
        "name": "seeded-randomized-interleaved",
        "unit": "one timed sample per kernel per schedule per round",
        "warmup": "one untimed invocation per kernel and process",
        "randomization_seed": workloads["seed"],
        "execution_order": [],
    },
    "schedules": [],
}
manifest["hardware"]["execution_affinity"] = a.execution_affinity
all_rows = []
reference_digest = None
prepared = []

for schedule in schedules_doc["schedules"]:
    sid = schedule["id"]
    optimized = artifacts / f"{sid}.ll"
    assembly = artifacts / f"{sid}.s"
    binary = artifacts / sid
    opt_command = [a.opt, "-load-pass-plugin", a.plugin,
                   f"-passes={schedule['pipeline']},verify", "-S", raw_ir,
                   "-o", optimized]
    started = time.perf_counter_ns()
    subprocess.run(opt_command, check=True)
    optimizer_ns = time.perf_counter_ns() - started
    subprocess.run([a.clang, "-O0", optimized, "-o", binary], check=True)
    subprocess.run([a.clang, "-O0", "-S", optimized, "-o", assembly], check=True)
    prepared.append({
        "schedule": schedule, "sid": sid, "optimized": optimized,
        "assembly": assembly, "binary": binary, "opt_command": opt_command,
        "optimizer_ns": optimizer_ns, "rows": [], "digest": None,
    })

rng = random.Random(workloads["seed"])
for trial in range(a.trials):
    order = prepared.copy()
    rng.shuffle(order)
    manifest["measurement_protocol"]["execution_order"].append(
        [item["sid"] for item in order])
    for item in order:
        run_command = [str(item["binary"]), str(a.size), "1"]
        run = subprocess.run(run_command, check=True, capture_output=True, text=True)
        digest = None
        result_count = 0
        observed_workloads = set()
        for line in run.stdout.splitlines():
            fields = line.split(",")
            if fields[0] == "RESULT":
                row = {"schedule": item["sid"], "kernel": fields[1],
                       "trial": trial, "nanoseconds": int(fields[3]),
                       "value": fields[4]}
                item["rows"].append(row)
                all_rows.append(row)
                result_count += 1
                observed_workloads.add(fields[1])
            elif fields[0] == "VALIDATION":
                digest = fields[1]
        if digest is None:
            raise RuntimeError(f"missing validation digest for {item['sid']} round {trial}")
        if result_count != len(expected_workloads) or observed_workloads != set(expected_workloads):
            raise RuntimeError(
                f"workload result mismatch for {item['sid']}: expected "
                f"{sorted(expected_workloads)}, got {sorted(observed_workloads)} "
                f"across {result_count} rows")
        if reference_digest is None:
            reference_digest = digest
        if digest != reference_digest:
            raise RuntimeError(f"validation mismatch for {item['sid']} round {trial}")
        item["digest"] = digest

for item in prepared:
    schedule = item["schedule"]
    sid = item["sid"]
    optimized = item["optimized"]
    assembly = item["assembly"]
    binary = item["binary"]
    rows = item["rows"]
    summaries = {}
    for kernel in sorted({row["kernel"] for row in rows}):
        values = [row["nanoseconds"] for row in rows if row["kernel"] == kernel]
        summaries[kernel] = {
            "median_nanoseconds": statistics.median(values),
            "mean_nanoseconds": statistics.mean(values),
            "stdev_nanoseconds": statistics.stdev(values) if len(values) > 1 else 0,
            "min_nanoseconds": min(values),
            "max_nanoseconds": max(values),
        }
    manifest["schedules"].append({
        **schedule,
        "opt_command": [str(x) for x in item["opt_command"]],
        "run_command_template": [str(binary), str(a.size), "1"],
        "correctness": {"validated": True, "digest": item["digest"]},
        "optimizer_nanoseconds": item["optimizer_ns"],
        "ir_instruction_count": instruction_count(optimized),
        "assembly_instruction_count": assembly_instruction_count(assembly),
        "binary_bytes": os.path.getsize(binary),
        "artifact_sha256": {"ir": sha256(optimized),
                            "assembly": sha256(assembly),
                            "binary": sha256(binary)},
        "artifacts": {"ir": str(optimized.relative_to(out)),
                      "assembly": str(assembly.relative_to(out)),
                      "binary": str(binary.relative_to(out))},
        "summary": summaries,
    })

with (out / "raw_trials.csv").open("w", newline="") as handle:
    writer = csv.DictWriter(handle, fieldnames=["schedule", "kernel", "trial",
                                                      "nanoseconds", "value"])
    writer.writeheader()
    writer.writerows(all_rows)
manifest["status"] = "completed"
(out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(json.dumps({"experiment_id": experiment_id, "status": "completed",
                  "manifest": str(out / "manifest.json")}, indent=2))
