#!/usr/bin/env python3
import argparse
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument("--clang", required=True)
p.add_argument("--opt", required=True)
p.add_argument("--plugin", required=True)
p.add_argument("--seed", type=int, required=True)
p.add_argument("--sanitize", action="store_true")
p.add_argument("--allow-sanitizer-unavailable", action="store_true")
a = p.parse_args()

if a.sanitize:
    runtime = subprocess.run(
        [a.clang, "--print-file-name=libclang_rt.asan-x86_64.a"],
        check=True, capture_output=True, text=True).stdout.strip()
    if not Path(runtime).is_file():
        message = f"sanitizer runtime unavailable: {runtime}"
        if a.allow_sanitizer_unavailable:
            print(f"SKIP: {message}")
            raise SystemExit(77)
        raise SystemExit(message)

here = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix="forge-differential-") as tmp:
    tmp = Path(tmp)
    transformed = tmp / "transformed.ll"
    before = tmp / "before"
    after = tmp / "after"

    subprocess.run([
        a.opt, "-load-pass-plugin", a.plugin,
        "-passes=forge-global-cse,forge-licm,verify", "-S",
        here / "program.ll", "-o", transformed,
    ], check=True)
    flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"] if a.sanitize else []
    subprocess.run([a.clang, "-O0", *flags, here / "program.ll", here / "harness.c",
                    "-o", before], check=True)
    subprocess.run([a.clang, "-O0", *flags, transformed, here / "harness.c",
                    "-o", after], check=True)

    baseline = subprocess.run([before, str(a.seed)], check=True,
                              capture_output=True, text=True).stdout
    optimized = subprocess.run([after, str(a.seed)], check=True,
                               capture_output=True, text=True).stdout
    if baseline != optimized:
        raise SystemExit(
            f"FAIL seed={a.seed}: before={baseline!r}, after={optimized!r}")
    print(f"PASS: differential execution seed={a.seed}, digest={baseline.strip()}")
