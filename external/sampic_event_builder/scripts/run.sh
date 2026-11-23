#!/usr/bin/env bash
set -euo pipefail
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source_dir=$(cd -- "${script_dir}/.." && pwd)
build_dir=${SAMPIC_EVENT_BUILDER_BUILD_DIR:-"${source_dir}/build"}
executable="${build_dir}/sampic_event_builder_frontend"
if [[ ! -x "$executable" ]]; then
  echo "Builder frontend not found: $executable. Run ${script_dir}/build.sh first." >&2
  exit 1
fi
exec "$executable" "$@"
