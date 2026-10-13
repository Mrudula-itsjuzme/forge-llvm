#!/usr/bin/env bash
set -euo pipefail
ctest --test-dir "${1:-build}" --output-on-failure

