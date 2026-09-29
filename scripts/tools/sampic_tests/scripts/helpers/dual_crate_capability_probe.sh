#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/../..")
CONFIG=${1:-"$PROJECT_DIR/config/dual_crate_capability_probe.default.json"}
PULSER="$PROJECT_DIR/build/bin/sampic_deadtime_scan"
BATCH_RUNNER="$SCRIPT_DIR/external_trigger_batching_scan.sh"
HARDWARE_TEMPLATE="$PROJECT_DIR/config/double_pulse_deadtime_scan.default.json"
EXTERNAL_TEMPLATE="$PROJECT_DIR/config/external_trigger_startup_waveform_probe.default.json"

for dependency in jq "$PULSER" "$BATCH_RUNNER"; do
  if [[ "$dependency" == jq ]]; then
    command -v jq >/dev/null || { echo "jq is required." >&2; exit 1; }
  elif [[ ! -x "$dependency" ]]; then
    echo "Required executable not found: $dependency" >&2
    exit 1
  fi
done

OUTPUT_ROOT=$(jq -r '.output_root' "$CONFIG")
STAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT="$OUTPUT_ROOT/$STAMP"
mkdir -p "$OUTPUT/configs" "$OUTPUT/internal_pulser" "$OUTPUT/external_trigger"
cp "$CONFIG" "$OUTPUT/dual_crate_capability_probe.json"

mapfile -t CRATE_NAMES < <(jq -r '.crates[].name' "$CONFIG")
if [[ ${#CRATE_NAMES[@]} -ne 2 ]]; then
  echo "This capability probe requires exactly two crates." >&2
  exit 2
fi

for index in 0 1; do
  name=$(jq -r ".crates[$index].name" "$CONFIG")
  ip=$(jq -r ".crates[$index].ip" "$CONFIG")
  port=$(jq -r ".crates[$index].port" "$CONFIG")
  calibration=$(jq -r ".crates[$index].calibration_dir" "$CONFIG")
  jq --arg ip "$ip" --arg calibration "$calibration" --argjson port "$port" \
    '.connection.ip=$ip | .connection.port=$port |
     .connection.calibration_dir=$calibration |
     .scan.digitizer_rates_mhz=[6400] | .scan.channels=[25] |
     .scan.board_index=0' \
    "$HARDWARE_TEMPLATE" >"$OUTPUT/configs/$name.hardware.json"
done

period=$(jq -r '.internal_pulser.period_ticks' "$CONFIG")
threshold=$(jq -r '.internal_pulser.threshold_v' "$CONFIG")
duration=$(jq -r '.internal_pulser.duration_s' "$CONFIG")

echo "Dual-crate capability probe"
echo "output: $OUTPUT"
echo
echo "Phase 1: simultaneous independent internal-pulser acquisition"

declare -a PIDS=()
for index in 0 1; do
  name=$(jq -r ".crates[$index].name" "$CONFIG")
  ip=$(jq -r ".crates[$index].ip" "$CONFIG")
  port=$(jq -r ".crates[$index].port" "$CONFIG")
  calibration=$(jq -r ".crates[$index].calibration_dir" "$CONFIG")
  "$PULSER" --mode pulser-rate --ip "$ip" --port "$port" \
    --calibration-dir "$calibration" --period-ticks "$period" \
    --threshold "$threshold" --events 0 --duration "$duration" --quiet \
    >"$OUTPUT/internal_pulser/$name.log" 2>&1 &
  PIDS+=("$!")
done

parallel_failure=0
for index in 0 1; do
  if ! wait "${PIDS[$index]}"; then
    parallel_failure=1
  fi
done
for name in "${CRATE_NAMES[@]}"; do
  echo "--- $name internal-pulser summary"
  sed -n '/^Summary/,$p' "$OUTPUT/internal_pulser/$name.log"
done
echo "parallel_internal_pulser_failure=$parallel_failure" \
  >"$OUTPUT/internal_pulser/status.env"

echo
echo "Phase 2: sequential Lecroy external-trigger connectivity"
external_rate=$(jq -r '.external_trigger.lecroy_rate_hz' "$CONFIG")
external_duration=$(jq -r '.external_trigger.duration_s' "$CONFIG")
channels=$(jq -c '.external_trigger.enabled_channels' "$CONFIG")

for index in 0 1; do
  name=$(jq -r ".crates[$index].name" "$CONFIG")
  point_config="$OUTPUT/configs/$name.external.json"
  jq --arg hardware "$OUTPUT/configs/$name.hardware.json" \
     --argjson rate "$external_rate" --argjson duration "$external_duration" \
     --argjson channels "$channels" \
    '.scan.lecroy_rates_hz=[$rate] | .scan.acquisition_schemes=["external"] |
     .scan.enabled_channels=$channels | .scan.repetitions=1 |
     .acquisition.duration_s=$duration | .acquisition.startup_waveform_hits=0 |
     .probe.hardware_config=$hardware' \
    "$EXTERNAL_TEMPLATE" >"$point_config"
  echo "Testing external trigger on $name"
  "$BATCH_RUNNER" --config "$point_config" \
    --output-dir "$OUTPUT/external_trigger/$name" \
    >"$OUTPUT/external_trigger/$name.log" 2>&1 || true
  metadata=$(find "$OUTPUT/external_trigger/$name" -mindepth 2 -maxdepth 2 \
    -name metadata.json -print -quit 2>/dev/null || true)
  if [[ -n "$metadata" ]]; then
    run_dir=$(dirname "$metadata")
    triggers=$(( $(wc -l <"$run_dir/triggers.csv") - 1 ))
    hits=$(( $(wc -l <"$run_dir/hits.csv") - 1 ))
    echo "$name: external trigger records=$triggers, hits=$hits"
  else
    echo "$name: external-trigger acquisition failed; inspect $OUTPUT/external_trigger/$name.log"
  fi
done

echo
echo "Capability probe complete: $OUTPUT"
