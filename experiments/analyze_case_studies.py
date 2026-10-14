#!/usr/bin/env python3
import argparse
import csv
import json
import math
from pathlib import Path
import random
import statistics


COMPARISONS = [
    ("forge-local-global-licm", "minimal", "Forge canonical vs minimal"),
    ("forge-licm-global-local", "forge-local-global-licm", "Forge alternate vs canonical"),
    ("llvm-scalar", "forge-local-global-licm", "LLVM scalar vs Forge canonical"),
    ("O2-plus-forge", "O2", "Forge appended after O2"),
    ("O3-plus-forge", "O3", "Forge appended after O3"),
]


def percentile(values, probability):
    ordered = sorted(values)
    position = (len(ordered) - 1) * probability
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    return (ordered[lower] * (upper - position)
            + ordered[upper] * (position - lower))


def bootstrap_median_interval(values, rng, samples):
    estimates = []
    for _ in range(samples):
        draw = [values[rng.randrange(len(values))] for _ in values]
        estimates.append(statistics.median(draw))
    return percentile(estimates, 0.025), percentile(estimates, 0.975)


def verdict(lower, upper):
    if upper < 1.0:
        return "lower runtime"
    if lower > 1.0:
        return "higher runtime"
    return "interval crosses 1.0"


p = argparse.ArgumentParser()
p.add_argument("manifest")
p.add_argument("--bootstrap-samples", type=int, default=10_000)
p.add_argument("--output", default=None)
a = p.parse_args()

manifest_path = Path(a.manifest)
manifest = json.loads(manifest_path.read_text())
if manifest.get("status") != "completed":
    raise SystemExit("case-study analysis requires a completed manifest")
if manifest.get("measurement_protocol", {}).get("name") != "seeded-randomized-interleaved":
    raise SystemExit("case-study analysis requires seeded randomized interleaving")

schedules = {schedule["id"]: schedule for schedule in manifest["schedules"]}
with manifest_path.with_name("raw_trials.csv").open(newline="") as handle:
    raw = list(csv.DictReader(handle))

observations = {}
for row in raw:
    key = (row["schedule"], row["kernel"], int(row["trial"]))
    if key in observations:
        raise SystemExit(f"duplicate observation: {key}")
    observations[key] = int(row["nanoseconds"])

kernels = manifest["dataset"]["split"]["tuning"] + manifest["dataset"]["split"]["evaluation"]
trial_count = manifest["dataset"]["trials"]
rng = random.Random(manifest["seed"] + 1)
results = []
for candidate, baseline, title in COMPARISONS:
    if candidate not in schedules or baseline not in schedules:
        raise SystemExit(f"missing comparison schedule: {candidate} or {baseline}")
    artifact_equal = {
        kind: schedules[candidate]["artifact_sha256"][kind]
        == schedules[baseline]["artifact_sha256"][kind]
        for kind in ("ir", "assembly", "binary")
    }
    kernel_results = []
    for kernel in kernels:
        ratios = []
        for trial in range(trial_count):
            candidate_value = observations[(candidate, kernel, trial)]
            baseline_value = observations[(baseline, kernel, trial)]
            ratios.append(candidate_value / baseline_value)
        lower, upper = bootstrap_median_interval(ratios, rng, a.bootstrap_samples)
        kernel_results.append({
            "kernel": kernel,
            "median_paired_ratio": statistics.median(ratios),
            "bootstrap_95_percent_interval": [lower, upper],
            "classification": verdict(lower, upper),
        })
    results.append({
        "title": title,
        "candidate": candidate,
        "baseline": baseline,
        "artifact_equal": artifact_equal,
        "ir_instruction_delta": schedules[candidate]["ir_instruction_count"]
        - schedules[baseline]["ir_instruction_count"],
        "assembly_instruction_delta": schedules[candidate]["assembly_instruction_count"]
        - schedules[baseline]["assembly_instruction_count"],
        "kernels": kernel_results,
    })

analysis = {
    "schema_version": 1,
    "experiment_id": manifest["experiment_id"],
    "commit": manifest["commit"],
    "method": {
        "estimator": "median of within-round candidate/baseline runtime ratios",
        "interval": "deterministic nonparametric percentile bootstrap of paired ratios",
        "confidence_level": 0.95,
        "bootstrap_samples": a.bootstrap_samples,
        "bootstrap_seed": manifest["seed"] + 1,
        "inference_boundary": "descriptive interval; not a universal speedup claim",
    },
    "comparisons": results,
}

output = Path(a.output) if a.output else manifest_path.with_name("case-studies.json")
output.write_text(json.dumps(analysis, indent=2) + "\n")

lines = [
    f"# Forge case studies: {manifest['experiment_id']}", "",
    f"Commit: `{manifest['commit']}`. Each result uses {trial_count} paired, "
    f"randomized-interleaved rounds and {a.bootstrap_samples:,} deterministic "
    "bootstrap resamples.", "",
    "Ratios below 1.0 favor the candidate. Intervals are descriptive uncertainty "
    "summaries for this recorded host and run, not universal speedup claims.", "",
]
for comparison in results:
    equal = comparison["artifact_equal"]
    lines += [
        f"## {comparison['title']}", "",
        f"IR delta: {comparison['ir_instruction_delta']:+d}; assembly delta: "
        f"{comparison['assembly_instruction_delta']:+d}. Artifact equality "
        f"(IR/assembly/binary): {equal['ir']}/{equal['assembly']}/{equal['binary']}.", "",
        "| Kernel | Median paired ratio | 95% bootstrap interval | Classification |",
        "|---|---:|---:|---|",
    ]
    for result in comparison["kernels"]:
        lower, upper = result["bootstrap_95_percent_interval"]
        lines.append(
            f"| {result['kernel']} | {result['median_paired_ratio']:.3f} | "
            f"[{lower:.3f}, {upper:.3f}] | {result['classification']} |")
    lines.append("")

lines += [
    "## Interpretation boundary", "",
    "Identical artifact hashes mean observed timing differences cannot be attributed "
    "to generated-code differences. Non-identical code plus an interval excluding "
    "1.0 is evidence for this host and workload only. CPU frequency, thermal state, "
    "process startup, allocator behavior, and a single physical host remain threats "
    "to validity.", "",
]
output.with_suffix(".md").write_text("\n".join(lines))
print(output)
