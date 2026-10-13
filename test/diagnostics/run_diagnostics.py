#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument("--opt", required=True)
p.add_argument("--plugin", required=True)
p.add_argument("--input", required=True)
a = p.parse_args()

with tempfile.TemporaryDirectory(prefix="forge-diagnostics-") as tmp:
    report = Path(tmp) / "diagnostics.jsonl"
    command = [
        a.opt, "-load-pass-plugin", a.plugin,
        f"-forge-diagnostics-jsonl={report}",
        "-pass-remarks=forge-.*",
        "-passes=forge-local-cse,forge-global-cse,forge-licm,verify",
        "-disable-output", a.input,
    ]
    run = subprocess.run(command, check=True, capture_output=True, text=True)
    records = [json.loads(line) for line in report.read_text().splitlines()]

    if not records or any(r["schema_version"] != 1 for r in records):
        raise SystemExit("missing or invalid diagnostics records")
    for required in ("forge-local-cse", "forge-global-cse", "forge-licm"):
        if not any(r["pass"] == required for r in records):
            raise SystemExit(f"missing diagnostics for {required}")

    local = sum(r["expressions_eliminated"] for r in records
                if r["pass"] == "forge-local-cse")
    cross_block = sum(r["cross_block_reuses"] for r in records
                      if r["pass"] == "forge-global-cse")
    hoisted = sum(r["instructions_hoisted"] for r in records
                  if r["pass"] == "forge-licm")
    unsafe = sum(r["rejections"].get("unsafe_to_speculate", 0) for r in records
                 if r["pass"] == "forge-licm")
    if min(local, cross_block, hoisted, unsafe) < 1:
        raise SystemExit(
            f"unexpected counters local={local} cross={cross_block} "
            f"hoisted={hoisted} unsafe={unsafe}")
    if "remark:" not in run.stderr:
        raise SystemExit("LLVM optimization remarks were not emitted")
    if any(r["elapsed_nanoseconds"] <= 0 for r in records):
        raise SystemExit("missing pass timings")
    print(f"PASS: {len(records)} records, remarks and counters validated")
