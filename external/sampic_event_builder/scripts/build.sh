#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source_dir=$(cd -- "${script_dir}/.." && pwd)
build_dir=${SAMPIC_EVENT_BUILDER_BUILD_DIR:-"${source_dir}/build"}

cmake -S "${source_dir}" -B "${build_dir}" -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF -DSAMPIC_EVENT_BUILDER_BUILD_BENCHMARKS=OFF \
  -DSAMPIC_EVENT_BUILDER_BUILD_MIDAS_FRONTEND=ON
cmake --build "${build_dir}" --parallel
