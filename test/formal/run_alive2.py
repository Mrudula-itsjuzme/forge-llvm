#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument("--alive-tv", required=True)
p.add_argument("--opt", required=True)
p.add_argument("--plugin", required=True)
p.add_argument("--input", required=True)
p.add_argument("--status", required=True)
a = p.parse_args()
status_path = Path(a.status)
status_path.parent.mkdir(parents=True, exist_ok=True)

alive = shutil.which(a.alive_tv)
if not alive:
    status_path.write_text(json.dumps({
        "status": "unavailable", "tool": "alive-tv",
        "claim": "no formal refinement result",
    }, indent=2) + "\n")
    print("SKIP: alive-tv unavailable; status recorded")
    raise SystemExit(77)

with tempfile.TemporaryDirectory(prefix="forge-alive2-") as tmp:
    target = Path(tmp) / "target.ll"
    subprocess.run([a.opt, "-load-pass-plugin", a.plugin,
                    "-passes=forge-local-cse,verify", "-S", a.input,
                    "-o", target], check=True)
    run = subprocess.run([alive, a.input, target], capture_output=True, text=True)
    outcome = "passed" if run.returncode == 0 else "failed_or_inconclusive"
    status_path.write_text(json.dumps({
        "status": outcome,
        "tool": subprocess.run([alive, "--version"], capture_output=True,
                               text=True).stdout.strip(),
        "input": a.input,
        "stdout": run.stdout,
        "stderr": run.stderr,
        "returncode": run.returncode,
        "claim": "formal refinement" if run.returncode == 0
                 else "no formal refinement result",
    }, indent=2) + "\n")
    if run.returncode:
        print("Alive2 returned failure or inconclusive; see status JSON")
        raise SystemExit(1)
    print("PASS: Alive2 refinement check")
