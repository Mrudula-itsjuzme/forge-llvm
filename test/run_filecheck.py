#!/usr/bin/env python3
import argparse
import subprocess

p = argparse.ArgumentParser()
p.add_argument("--opt", required=True)
p.add_argument("--filecheck", required=True)
p.add_argument("--plugin", required=True)
p.add_argument("--test", required=True)
a = p.parse_args()

opt = subprocess.run(
    [a.opt, "-load-pass-plugin", a.plugin,
     "-passes=forge-redundant-expr-elim,verify", "-S", a.test],
    check=True, capture_output=True)
subprocess.run([a.filecheck, a.test], input=opt.stdout, check=True)

