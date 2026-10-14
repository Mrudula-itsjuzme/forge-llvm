# Forge v3 performance evidence

## Campaign of record

The campaign `20261014T195403Z-06973af` ran from clean commit `06973af` in the
Ubuntu 24.04 LLVM/Clang 18.1.3 container. It used seven deterministic kernels,
5,000,000 elements, 21 rounds, eight schedules, and Docker CPU affinity to CPU
0. Within every round, schedule order was shuffled with seed `20261013`. Every
one of the 1,176 timed observations is retained in `raw_trials.csv`; all
schedules and rounds produced matching validation digests.

Generated experiment directories are intentionally Git-ignored. Reproduce the
campaign and reports with:

```bash
docker run --rm --cpuset-cpus=0 --user "$(id -u):$(id -g)" \
  -v "$PWD:/src" -w /src forge-llvm-ci:latest \
  python3 experiments/run_experiments.py \
  --clang /usr/lib/llvm-18/bin/clang \
  --opt /usr/lib/llvm-18/bin/opt \
  --plugin build-docker/lib/ForgePass.so \
  --commit "$(git rev-parse --short HEAD)" \
  --size 5000000 --trials 21 \
  --execution-affinity docker-cpuset:0

python3 experiments/generate_report.py experiments/runs/<id>/manifest.json
python3 experiments/analyze_case_studies.py experiments/runs/<id>/manifest.json
```

## Artifact results

| Schedule | IR instructions | ASM instructions | Binary bytes |
|---|---:|---:|---:|
| minimal | 524 | 819 | 20,984 |
| Forge canonical | 512 | 817 | 20,984 |
| Forge alternate | 512 | 817 | 20,984 |
| LLVM scalar | 434 | 794 | 20,984 |
| O2 | 747 | 1,315 | 20,984 |
| O2 + Forge | 747 | 1,315 | 20,984 |
| O3 | 1,447 | 2,379 | 25,080 |
| O3 + Forge | 1,447 | 2,379 | 25,080 |

Both Forge orders produced identical IR, assembly, and executable hashes. O2
and O2+Forge were also identical at all three artifact levels; the same was true
for O3 and O3+Forge. Forge therefore found no incremental transformation after
these standard LLVM pipelines on this suite.

## Case-study results

The analyzer uses the median of within-round candidate/baseline runtime ratios
and a deterministic 10,000-resample percentile bootstrap interval. These are
descriptive intervals for this campaign, not general confidence guarantees.

Canonical Forge versus minimal removed 12 IR and 2 assembly instructions. Five
kernels had intervals crossing 1.0. Matrix multiply was lower at a median paired
ratio of 0.912, interval [0.875, 0.952]. Stencil was higher at 1.062, interval
[1.050, 1.077]. This is direct evidence that smaller IR is not sufficient to
claim uniformly faster code.

LLVM scalar versus canonical Forge removed another 78 IR and 23 assembly
instructions. Reduction, repeated expression, dot, and matrix multiply had
lower-runtime intervals; recurrence, stencil, and strided access crossed 1.0.
The result is consistent with LLVM's broader EarlyCSE/GVN/LICM machinery, while
also showing that broader optimization did not establish a win on every kernel.

The two Forge orders and both post-O2/O3 comparisons had identical artifacts.
Every timing interval in those comparisons crossed 1.0. Their runtime variation
must therefore be treated as measurement noise, not a Forge effect.

## Threats to validity

- One Intel Core i5-12450H host was used under the `powersave` governor.
- Affinity constrained scheduling to CPU 0 but did not lock frequency, disable
  turbo, isolate the core, or control thermal state.
- `perf` was unavailable, so there are no hardware-counter explanations.
- Process startup, allocation, cache state, and container overhead remain.
- Seven purpose-built kernels are not representative of arbitrary programs.
- Bootstrap intervals summarize observed rounds and do not repair systematic
  host bias or establish performance on other processors.
- No native AArch64 or NVIDIA Grace timing was performed.

The defensible conclusion is narrow: Forge performs auditable conservative
transformations before standard pipelines, with workload-dependent runtime
outcomes, and is subsumed by O2/O3 on this suite.
