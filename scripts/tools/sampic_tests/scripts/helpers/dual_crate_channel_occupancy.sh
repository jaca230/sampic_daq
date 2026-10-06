#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/../..")
REPO_ROOT=$(realpath "$PROJECT_DIR/../../..")
OCCUPANCY="$SCRIPT_DIR/channel_occupancy_mode.sh"
DEFAULT_CONFIG="$PROJECT_DIR/config/dual_crate_channel_occupancy.default.json"

CONFIG="$DEFAULT_CONFIG"
DURATION_OVERRIDE=""
THRESHOLD_OVERRIDE=""
MIN_RATE_OVERRIDE=""
OUTPUT_OVERRIDE=""
SEQUENTIAL=0
FORCE=0

usage() {
  cat <<'EOF'
Usage: dual_crate_channel_occupancy.sh [options]

Measure self-trigger channel rates on one or more configured SAMPIC crates.

Options:
  --config <file>       Configuration JSON
  --duration <seconds>  Override acquisition duration
  --threshold <volts>   Override self-trigger threshold
  --min-rate <Hz>       Only display channels at or above this rate
  --output-dir <dir>    Write results to this exact directory
  --sequential          Scan configured crates one after another
  --force               Run even if a sampic_frontend process is detected
  -h, --help            Show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --config) CONFIG=${2:?Missing value for --config}; shift 2 ;;
    --duration) DURATION_OVERRIDE=${2:?Missing value for --duration}; shift 2 ;;
    --threshold) THRESHOLD_OVERRIDE=${2:?Missing value for --threshold}; shift 2 ;;
    --min-rate) MIN_RATE_OVERRIDE=${2:?Missing value for --min-rate}; shift 2 ;;
    --output-dir) OUTPUT_OVERRIDE=${2:?Missing value for --output-dir}; shift 2 ;;
    --sequential) SEQUENTIAL=1; shift ;;
    --force) FORCE=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
done

command -v jq >/dev/null || { echo "jq is required." >&2; exit 1; }
[[ -x "$OCCUPANCY" ]] || { echo "Missing occupancy wrapper: $OCCUPANCY" >&2; exit 1; }
[[ -f "$CONFIG" ]] || { echo "Configuration not found: $CONFIG" >&2; exit 1; }

if [[ $FORCE -eq 0 ]] && pgrep -f '[/]sampic_frontend([[:space:]]|$)' >/dev/null; then
  echo "A sampic_frontend process is running. Stop it before reconfiguring the crates." >&2
  echo "Use --force only if you have verified it is not connected to these controllers." >&2
  exit 1
fi

if ! jq -e '.crates | type == "array"' "$CONFIG" >/dev/null; then
  echo "Expected a crates array in $CONFIG." >&2
  exit 2
fi
crate_count=$(jq '.crates | length' "$CONFIG")
if [[ "$crate_count" -lt 1 ]]; then
  echo "Expected at least one crate in $CONFIG; found none." >&2
  exit 2
fi
unique_name_count=$(jq '[.crates[].name] | unique | length' "$CONFIG")
if [[ "$unique_name_count" -ne "$crate_count" ]]; then
  echo "Configured crate names must be unique in $CONFIG." >&2
  exit 2
fi

DURATION=${DURATION_OVERRIDE:-$(jq -r '.duration_s' "$CONFIG")}
THRESHOLD=${THRESHOLD_OVERRIDE:-$(jq -r '.threshold_v' "$CONFIG")}
MIN_RATE=${MIN_RATE_OVERRIDE:-$(jq -r '.minimum_rate_hz // 0' "$CONFIG")}

if ! awk -v value="$DURATION" 'BEGIN { exit !(value > 0) }'; then
  echo "Duration must be positive: $DURATION" >&2
  exit 2
fi
if ! awk -v value="$THRESHOLD" 'BEGIN { exit !(value > 0) }'; then
  echo "Threshold must be positive: $THRESHOLD" >&2
  exit 2
fi
if ! awk -v value="$MIN_RATE" 'BEGIN { exit !(value >= 0) }'; then
  echo "Minimum rate must be non-negative: $MIN_RATE" >&2
  exit 2
fi

if [[ -n "$OUTPUT_OVERRIDE" ]]; then
  OUTPUT="$OUTPUT_OVERRIDE"
else
  output_root=$(jq -r '.output_root' "$CONFIG")
  [[ "$output_root" = /* ]] || output_root="$REPO_ROOT/$output_root"
  OUTPUT="$output_root/$(date +%Y%m%d_%H%M%S)"
fi
mkdir -p "$OUTPUT"
cp "$CONFIG" "$OUTPUT/config.json"

run_crate() {
  local index=$1
  local name ip port calibration
  name=$(jq -r ".crates[$index].name" "$CONFIG")
  ip=$(jq -r ".crates[$index].ip" "$CONFIG")
  port=$(jq -r ".crates[$index].port" "$CONFIG")
  calibration=$(jq -r ".crates[$index].calibration_dir" "$CONFIG")
  [[ "$calibration" = /* ]] || calibration="$REPO_ROOT/$calibration"

  if [[ ! -d "$calibration" ]]; then
    echo "$name calibration directory not found: $calibration" >&2
    return 1
  fi

  echo "Starting $name ($ip:$port)" >&2
  "$OCCUPANCY" \
    --ip "$ip" --port "$port" \
    --events 0 --duration "$DURATION" --threshold "$THRESHOLD" \
    --calibration-dir "$calibration" --quiet \
    --json-file "$OUTPUT/$name.json" \
    >"$OUTPUT/$name.stdout.log" 2>"$OUTPUT/$name.stderr.log"

  if ! jq empty "$OUTPUT/$name.json" 2>/dev/null; then
    echo "$name did not produce valid JSON; inspect $OUTPUT/$name.json and $OUTPUT/$name.stderr.log" >&2
    return 1
  fi
}

echo "Configured-crate channel occupancy ($crate_count crate(s))"
echo "  duration:  $DURATION s"
echo "  threshold: $THRESHOLD V"
echo "  output:    $OUTPUT"

failures=0
if [[ $SEQUENTIAL -eq 1 ]]; then
  for ((index=0; index<crate_count; ++index)); do
    run_crate "$index" || failures=$((failures + 1))
  done
else
  declare -a pids=()
  for ((index=0; index<crate_count; ++index)); do
    run_crate "$index" &
    pids+=("$!")
  done
  for pid in "${pids[@]}"; do
    wait "$pid" || failures=$((failures + 1))
  done
fi

if [[ $failures -ne 0 ]]; then
  echo "$failures crate occupancy scan(s) failed; inspect $OUTPUT." >&2
  exit 1
fi

combined_entries="$OUTPUT/combined_entries.jsonl"
: >"$combined_entries"
for ((index=0; index<crate_count; ++index)); do
  name=$(jq -r ".crates[$index].name" "$CONFIG")
  jq --arg name "$name" '. + {name: $name}' "$OUTPUT/$name.json" \
    >>"$combined_entries"
done
jq -s '{crates: .}' "$combined_entries" >"$OUTPUT/combined.json"
unlink "$combined_entries"

printf 'crate\tfeb\tsampic\tchannel\thits\trate_hz\n' >"$OUTPUT/channel_rates.tsv"
for ((index=0; index<crate_count; ++index)); do
  name=$(jq -r ".crates[$index].name" "$CONFIG")
  jq -r --arg crate "$name" --argjson minimum "$MIN_RATE" '
    .channels
    | sort_by(-.hit_rate_hz)[]
    | select(.hit_rate_hz >= $minimum)
    | [$crate, .feb, .sampic, .channel, .hits,
       ((.hit_rate_hz * 100.0 | round) / 100.0)]
    | @tsv
  ' "$OUTPUT/$name.json" >>"$OUTPUT/channel_rates.tsv"
done

echo
if command -v column >/dev/null; then
  column -t -s $'\t' "$OUTPUT/channel_rates.tsv"
else
  cat "$OUTPUT/channel_rates.tsv"
fi
echo
echo "Results: $OUTPUT"
