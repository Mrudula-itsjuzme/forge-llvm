#!/usr/bin/env python3
import argparse
import html
import json
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument("manifest")
p.add_argument("--output", default=None)
a = p.parse_args()

manifest_path = Path(a.manifest)
data = json.loads(manifest_path.read_text())
output = Path(a.output) if a.output else manifest_path.with_name("report.md")
baseline = next(s for s in data["schedules"] if s["id"] == "minimal")
kernels = sorted(baseline["summary"])

lines = [
    f"# Forge experiment {data['experiment_id']}", "",
    f"Status: **{data['status']}**  ",
    f"Commit: `{data['commit']}`  ",
    f"Dataset size: {data['dataset']['size']:,}; trials: {data['dataset']['trials']}", "",
    "## Correctness and artifact summary", "",
    "| Schedule | Validated | IR instructions | Binary bytes | Optimizer ms |", 
    "|---|---:|---:|---:|---:|",
]
for schedule in data["schedules"]:
    lines.append(
        f"| {schedule['id']} | {schedule['correctness']['validated']} | "
        f"{schedule['ir_instruction_count']} | {schedule['binary_bytes']} | "
        f"{schedule['optimizer_nanoseconds'] / 1e6:.3f} |")

for split in ("tuning", "evaluation"):
    lines += ["", f"## {split.title()} workloads", "",
              "Median runtime in milliseconds; ratio is relative to the minimal baseline.", "",
              "| Kernel | Schedule | Median ms | Ratio |", "|---|---|---:|---:|"]
    selected = data["dataset"]["split"][split]
    for kernel in selected:
        base = baseline["summary"][kernel]["median_nanoseconds"]
        for schedule in data["schedules"]:
            value = schedule["summary"][kernel]["median_nanoseconds"]
            lines.append(f"| {kernel} | {schedule['id']} | {value / 1e6:.4f} | {value / base:.3f} |")

lines += ["", "## Evidence boundary", "",
          "This report is generated from the adjacent manifest and raw CSV. Runtime ratios are observations for the recorded host and configuration, not universal speedups. Hardware counters marked unavailable were not estimated.", ""]
output.write_text("\n".join(lines))

# Dependency-free SVG: median evaluation-runtime totals per schedule.
totals = []
for schedule in data["schedules"]:
    total = sum(schedule["summary"][k]["median_nanoseconds"]
                for k in data["dataset"]["split"]["evaluation"])
    totals.append((schedule["id"], total))
maximum = max(value for _, value in totals) or 1
width, row_height = 900, 34
svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{50 + row_height * len(totals)}">',
       '<style>text{font:13px sans-serif}.bar{fill:#4f46e5}</style>',
       '<text x="10" y="20">Evaluation-kernel median runtime total (lower is better)</text>']
for index, (name, value) in enumerate(totals):
    y = 40 + index * row_height
    bar = int(600 * value / maximum)
    svg.append(f'<text x="10" y="{y + 16}">{html.escape(name)}</text>')
    svg.append(f'<rect class="bar" x="220" y="{y}" width="{bar}" height="20"/>')
    svg.append(f'<text x="{230 + bar}" y="{y + 16}">{value / 1e6:.3f} ms</text>')
svg.append('</svg>')
output.with_suffix(".svg").write_text("\n".join(svg) + "\n")
print(output)
