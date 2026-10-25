#!/usr/bin/env python3
import argparse
from pathlib import Path
import sys

p = argparse.ArgumentParser()
p.add_argument("--source-root", required=True)
a = p.parse_args()

sys.path.insert(0, str(Path(a.source_root) / "benchmark"))
from validation import validate_trial_output  # noqa: E402

expected = validate_trial_output(None, "42", "O0", 0)
if expected != "42":
    raise SystemExit("first trial did not establish the reference output")
validate_trial_output(expected, "42", "O0", 1)

try:
    validate_trial_output(expected, "41", "O0", 2)
except RuntimeError as error:
    if "O0 trial 2" not in str(error):
        raise SystemExit("failure did not preserve pipeline and trial context")
else:
    raise SystemExit("an intermediate output mismatch was not rejected")

print("PASS: every benchmark trial is validated")
