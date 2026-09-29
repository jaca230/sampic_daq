#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/../..")
CONFIG=${1:-"$PROJECT_DIR/config/internal_pulser_channel_discovery.default.json"}
BINARY="$PROJECT_DIR/build/bin/sampic_deadtime_scan"

command -v jq >/dev/null || { echo "jq is required." >&2; exit 1; }
[[ -x "$BINARY" ]] || { echo "Build tools first: scripts/tools/sampic_tests/scripts/build.sh" >&2; exit 1; }

OUTPUT_ROOT=$(jq -r '.output_root' "$CONFIG")
OUTPUT="$OUTPUT_ROOT/$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUTPUT"
cp "$CONFIG" "$OUTPUT/scan_config.json"
period=$(jq -r '.period_ticks' "$CONFIG")
threshold=$(jq -r '.threshold_v' "$CONFIG")
duration=$(jq -r '.duration_s' "$CONFIG")
width_ticks=$(jq -r '.pulser_width_ticks // 2' "$CONFIG")
crate_count=$(jq '.crates | length' "$CONFIG")

echo "Internal-pulser channel discovery: $crate_count crate(s)"
echo "Matched pulser-OFF and pulser-ON captures, $duration s each"
echo "output: $OUTPUT"

for ((index=0; index<crate_count; ++index)); do
  name=$(jq -r ".crates[$index].name" "$CONFIG")
  ip=$(jq -r ".crates[$index].ip" "$CONFIG")
  port=$(jq -r ".crates[$index].port" "$CONFIG")
  calibration=$(jq -r ".crates[$index].calibration_dir" "$CONFIG")
  disable_args=()
  while IFS= read -r channel; do
    disable_args+=(--disable-channel "$channel")
  done < <(jq -r ".crates[$index].disabled_hot_channels[]?" "$CONFIG")
  echo
  echo "$name ($ip:$port)"
  for state in off on; do
    extra=()
    [[ "$state" == off ]] && extra+=(--pulser-off)
    echo "  pulser $state"
    "$BINARY" --mode pulser-rate --ip "$ip" --port "$port" \
      --calibration-dir "$calibration" --period-ticks "$period" \
      --pulser-width-ticks "$width_ticks" --sync-pulser \
      --threshold "$threshold" --events 0 --duration "$duration" --quiet \
      --channel-counts-csv "$OUTPUT/${name}_${state}_channels.csv" \
      "${disable_args[@]}" "${extra[@]}" >"$OUTPUT/${name}_${state}.log" 2>&1
    sed -n '/^Pulser readback:/,/^Summary/p' "$OUTPUT/${name}_${state}.log" | sed '$d'
    sed -n '/^Summary/,$p' "$OUTPUT/${name}_${state}.log"
  done
done

echo
echo "Discovery complete: $OUTPUT"
