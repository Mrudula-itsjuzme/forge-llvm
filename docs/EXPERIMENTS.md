# Experiment registry and scheduling

Forge's experiment runner reads schedules and workload splits from versioned
JSON files. Schedule selection is data, not hard-coded control flow.

```bash
python3 experiments/run_experiments.py \
  --clang clang-18 --opt /usr/lib/llvm-18/bin/opt \
  --plugin build/lib/ForgePass.so \
  --commit "$(git rev-parse HEAD)" \
  --size 2000000 --trials 15
```

If the source tree is not clean, pass `--dirty`. A clean commit identifier and
the dirty flag are separate registry fields; experiments must not imply that
uncommitted source matches the named commit.

The eight initial schedules cover a minimal baseline, two Forge orders, a
comparable LLVM scalar pipeline (`early-cse`, `gvn`, and `licm`), O2/O3, and
O2/O3 followed by Forge. Every schedule runs with identical source, workload
size, trial count, and final code-generation command.

The runner first builds every schedule, then executes one timed sample per
kernel per schedule in each round. Schedule order is shuffled independently in
every round with the frozen workload seed. The exact order is retained in the
manifest. Each process performs its own untimed warm-up before the timed sample.
This interleaving reduces systematic drift from running all samples for one
schedule before starting the next; it does not eliminate frequency scaling,
thermal, operating-system, or shared-host noise.

The workload split is frozen in `experiments/config/workloads.json`. Reduction,
repeated-expression, and loop-recurrence are tuning workloads. Dot product,
stencil, strided access, and matrix multiplication are evaluation workloads.
No automatic schedule policy may inspect the evaluation split during tuning.

Each run directory contains:

- a schema-versioned manifest with commit, toolchain, hardware, invocation,
  pipelines, correctness digest, compile time, IR counts, binary size,
  measurement protocol, per-round schedule order, summary, and artifact paths;
- raw CSV trials;
- optimized IR, assembly, and executables for every schedule; and
- generated Markdown and SVG reports.

Generate a report only from a saved manifest:

```bash
python3 experiments/generate_report.py experiments/runs/<id>/manifest.json
python3 experiments/analyze_case_studies.py experiments/runs/<id>/manifest.json
```

The case-study analyzer pairs candidate and baseline observations by interleaved
round, reports the median runtime ratio, and computes a deterministic percentile
bootstrap interval. It also checks IR, assembly, and executable hashes. If code
is identical, runtime variation is explicitly treated as measurement noise, not
as an optimization effect.

When running Docker with a bind mount, use the host UID/GID if host-side tools
must edit generated files:

```bash
docker run --rm --user "$(id -u):$(id -g)" \
  -v "$PWD:/src" -w /src forge-llvm-ci <command>
```

Short smoke runs validate the machinery but are not performance evidence.
Full campaigns need sufficiently long kernels, stable conditions, warm-up,
repeated trials, and explicit variance/threats-to-validity analysis.

No automatic scheduler exists yet. It will remain absent until tuning results
support a conservative policy that can be evaluated without consulting the
sealed evaluation workloads.
