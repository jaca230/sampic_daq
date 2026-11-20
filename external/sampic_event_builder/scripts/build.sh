#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source_dir=$(cd -- "${script_dir}/.." && pwd)
build_dir=${SAMPIC_EVENT_BUILDER_BUILD_DIR:-"${source_dir}/build"}

cmake -S "${source_dir}" -B "${build_dir}" -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON -DSAMPIC_EVENT_BUILDER_BUILD_BENCHMARKS=ON
cmake --build "${build_dir}" --parallel
ctest --test-dir "${build_dir}" --output-on-failure
