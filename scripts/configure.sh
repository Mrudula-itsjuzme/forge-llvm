#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-build}"
cmake -S . -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "$build_dir"

