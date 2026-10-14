#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument("--python", required=True)
p.add_argument("--clang", required=True)
p.add_argument("--opt", required=True)
p.add_argument("--plugin", required=True)
p.add_argument("--source-root", required=True)
a = p.parse_args()
root = Path(a.source_root)

with tempfile.TemporaryDirectory(prefix="forge-experiment-test-") as tmp:
    run = subprocess.run([
        a.python, root / "experiments" / "run_experiments.py",
        "--clang", a.clang, "--opt", a.opt, "--plugin", a.plugin,
        "--commit", "ctest", "--dirty", "--size", "2000", "--trials", "2",
        "--output-root", tmp,
    ], check=True, capture_output=True, text=True)
    result = json.loads(run.stdout)
    manifest_path = Path(result["manifest"])
    manifest = json.loads(manifest_path.read_text())
    if manifest["status"] != "completed" or len(manifest["schedules"]) != 8:
        raise SystemExit("experiment registry did not complete all schedules")
    if not all(s["correctness"]["validated"] for s in manifest["schedules"]):
        raise SystemExit("one or more schedules failed correctness validation")
    protocol = manifest.get("measurement_protocol", {})
    orders = protocol.get("execution_order", [])
    schedule_ids = {s["id"] for s in manifest["schedules"]}
    if protocol.get("name") != "seeded-randomized-interleaved" or len(orders) != 2:
        raise SystemExit("experiment did not record the interleaved protocol")
    if any(set(order) != schedule_ids or len(order) != len(schedule_ids)
           for order in orders):
        raise SystemExit("an interleaved round did not execute every schedule once")
    raw_rows = manifest_path.with_name("raw_trials.csv").read_text().splitlines()
    if len(raw_rows) != 1 + 8 * 7 * 2:
        raise SystemExit("raw trial count does not match schedules x workloads x rounds")
    subprocess.run([
        a.python, root / "experiments" / "generate_report.py", manifest_path,
    ], check=True)
    if not manifest_path.with_name("report.md").stat().st_size:
        raise SystemExit("Markdown report is empty")
    if not manifest_path.with_name("report.svg").stat().st_size:
        raise SystemExit("SVG report is empty")
    print("PASS: interleaved registry, eight schedules, Markdown and SVG reports")
