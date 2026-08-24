#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/../..")
BUILD_DIR="$PROJECT_DIR/build"
REPO_ROOT=$(realpath "$PROJECT_DIR/../../..")
BINARY="$BUILD_DIR/bin/external_trigger_probe"
DEFAULT_CONFIG="$PROJECT_DIR/config/external_trigger_batching_scan.default.json"
VENDOR_LIB_DIR="$REPO_ROOT/external/sampic_256ch_lib/lib"
LPDEVC_LIB_DIR="$REPO_ROOT/external/sampic_256ch_lib/lpdevc_install/lib"

if [[ ! -x "$BINARY" ]]; then
  echo "external_trigger_probe binary not found at $BINARY." >&2
  echo "Build the tools first with: scripts/tools/sampic_tests/scripts/build.sh" >&2
  exit 1
fi

declare -a RUNTIME_LIB_PATHS=()
[[ -d "$VENDOR_LIB_DIR" ]] && RUNTIME_LIB_PATHS+=("$VENDOR_LIB_DIR")
[[ -d "$LPDEVC_LIB_DIR" ]] && RUNTIME_LIB_PATHS+=("$LPDEVC_LIB_DIR")
if ((${#RUNTIME_LIB_PATHS[@]} > 0)); then
  RUNTIME_LIB_PATH=$(IFS=:; echo "${RUNTIME_LIB_PATHS[*]}")
  export LD_LIBRARY_PATH="$RUNTIME_LIB_PATH${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

declare -a FORWARDED_ARGS=()
HAS_CONFIG=false
while (($# > 0)); do
  case "$1" in
    --config)
      if (($# < 2)); then
        echo "--config requires a JSON file path." >&2
        exit 2
      fi
      FORWARDED_ARGS+=(--batch-scan-config "$2")
      HAS_CONFIG=true
      shift 2
      ;;
    -h|--help)
      cat <<EOF
Usage: external_trigger_batching_scan.sh [options]

  --config FILE       Batching-scan JSON (default: $DEFAULT_CONFIG)
  --output-dir PATH   Exact output directory
  --resume            Reuse points containing metadata.json
  --dry-run           Parse and print the scan without hardware access
  -h, --help          Show this help

One C++ process owns the complete grid and keeps the crate and Lecroy
connections open between stopped SAMPIC runs. Ctrl-C requests a safe stop after
the active point has disabled the generator, drained data, and called StopRun().
EOF
      exit 0
      ;;
    --output-dir|--resume|--dry-run)
      FORWARDED_ARGS+=("$1")
      if [[ "$1" == "--output-dir" ]]; then
        if (($# < 2)); then
          echo "--output-dir requires a path." >&2
          exit 2
        fi
        FORWARDED_ARGS+=("$2")
        shift 2
      else
        shift
      fi
      ;;
    *)
      echo "Unknown batching-scan option: $1" >&2
      exit 2
      ;;
  esac
done

if [[ "$HAS_CONFIG" == false ]]; then
  FORWARDED_ARGS=(--batch-scan-config "$DEFAULT_CONFIG" "${FORWARDED_ARGS[@]}")
fi

exec "$BINARY" "${FORWARDED_ARGS[@]}"
