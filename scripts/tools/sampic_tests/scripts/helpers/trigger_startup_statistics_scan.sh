#!/usr/bin/env bash
set -euo pipefail

# Repeated statistics scan for the focused trigger-startup readiness test.
# The C++ test remains a single-capture program; this script supplies only the
# repetition needed to compare the same four rates statistically.

SCRIPT_DIR=$(dirname "$(realpath "$0")")
RUN_ONE="$SCRIPT_DIR/trigger_startup_readiness_test.sh"

TRIALS=${TRIALS:-20}
DURATION_S=${DURATION_S:-5}
MAX_ATTEMPTS=${MAX_ATTEMPTS:-5}
OUTPUT_ROOT=${OUTPUT_ROOT:-data/trigger_startup_readiness_test/preallocated_statistics_scan}
RATES=(1000 5000 10000 20000)

if [[ ! "$TRIALS" =~ ^[1-9][0-9]*$ ]]; then
  echo "TRIALS must be a positive integer." >&2
  exit 2
fi

if [[ ! "$MAX_ATTEMPTS" =~ ^[1-9][0-9]*$ ]]; then
  echo "MAX_ATTEMPTS must be a positive integer." >&2
  exit 2
fi

mkdir -p "$OUTPUT_ROOT"
MANIFEST="$OUTPUT_ROOT/manifest.csv"
echo "rate_hz,trial,status,run_dir" > "$MANIFEST"

for rate in "${RATES[@]}"; do
  for ((trial = 1; trial <= TRIALS; ++trial)); do
    trial_name=$(printf 'trial_%03d' "$trial")
    run_dir="$OUTPUT_ROOT/rate_${rate}/${trial_name}"
    relative_dir="rate_${rate}/${trial_name}"

    echo
    echo "=== ${rate} Hz, trial ${trial}/${TRIALS} ==="

    success=false

    for ((attempt = 1; attempt <= MAX_ATTEMPTS; ++attempt)); do
      echo "--- Attempt ${attempt}/${MAX_ATTEMPTS} ---"

      if "$RUN_ONE" \
          --rate-hz "$rate" \
          --duration-s "$DURATION_S" \
          --output "$run_dir"; then
        success=true
        break
      fi

      echo "Capture failed on attempt ${attempt}/${MAX_ATTEMPTS}." >&2

      if (( attempt < MAX_ATTEMPTS )); then
        echo "Retrying..." >&2
      fi
    done

    if [[ "$success" == true ]]; then
      echo "${rate},${trial},complete,${relative_dir}" >> "$MANIFEST"
    else
      echo "${rate},${trial},failed,${relative_dir}" >> "$MANIFEST"
      echo "Capture failed after ${MAX_ATTEMPTS} attempts; stopping statistics scan." >&2
      exit 1
    fi
  done
done

echo
echo "Statistics scan complete: $((${#RATES[@]} * TRIALS)) captures."
echo "Manifest: $MANIFEST"