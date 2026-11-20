#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source_dir=$(cd -- "${script_dir}/.." && pwd)
build_dir=${SAMPIC_EVENT_BUILDER_BUILD_DIR:-"${source_dir}/build"}

exec "${build_dir}/sampic_event_builder_benchmark" "$@"
