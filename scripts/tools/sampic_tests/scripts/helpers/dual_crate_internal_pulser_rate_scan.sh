#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/../..")
REPO_ROOT=$(realpath "$PROJECT_DIR/../../..")
CONFIG=${1:-"$PROJECT_DIR/config/dual_crate_internal_pulser_rate_scan.default.json"}
BINARY="$PROJECT_DIR/build/bin/sampic_deadtime_scan"

command -v jq >/dev/null || { echo "jq is required." >&2; exit 1; }
[[ -x "$BINARY" ]] || {
  echo "Test binary not found: $BINARY" >&2
  echo "Build with: scripts/tools/sampic_tests/scripts/build.sh" >&2
  exit 1
}

mapfile -t CRATE_NAMES < <(jq -r '.crates[].name' "$CONFIG")
if [[ ${#CRATE_NAMES[@]} -ne 2 ]]; then
  echo "The scan requires exactly two crates." >&2
  exit 2
fi

OUTPUT_ROOT=$(jq -r '.output_root' "$CONFIG")
STAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT="$OUTPUT_ROOT/$STAMP"
mkdir -p "$OUTPUT/logs"
cp "$CONFIG" "$OUTPUT/scan_config.json"
RESULTS="$OUTPUT/results.csv"
echo "condition,crate,nominal_rate_hz,configured_rate_hz,period_ticks,events,total_hits,total_bytes,elapsed_s,events_per_s,hits_per_s,data_MiB_per_s,exit_status,log" >"$RESULTS"

threshold=$(jq -r '.threshold_v' "$CONFIG")
duration=$(jq -r '.duration_s' "$CONFIG")
width_ticks=$(jq -r '.pulser_width_ticks // 2' "$CONFIG")
frames_per_block=$(jq -r '.frames_per_block // 31' "$CONFIG")
max_attempts=$(jq -r '.max_point_attempts // 3' "$CONFIG")
retry_delay_s=$(jq -r '.retry_delay_s // 1' "$CONFIG")
common_channel_count=$(jq -r '.common_channel_count // 128' "$CONFIG")
declare -a COMMON_ENABLE_ARGS=() COMMON_CHANNELS=()
for feb in 0 1 2 3; do
  for channel in $(seq 0 63); do
    identity="$feb:$channel"
    if jq -e --arg identity "$identity" \
      '.common_excluded_channels // [] | index($identity) != null' \
      "$CONFIG" >/dev/null; then
      continue
    fi
    COMMON_ENABLE_ARGS+=(--enable-channel "$identity")
    COMMON_CHANNELS+=("$identity")
    [[ ${#COMMON_CHANNELS[@]} -ge $common_channel_count ]] && break 2
  done
done
if [[ ${#COMMON_CHANNELS[@]} -ne $common_channel_count ]]; then
  echo "Could only construct ${#COMMON_CHANNELS[@]} of $common_channel_count common channels." >&2
  exit 2
fi
printf '%s\n' "${COMMON_CHANNELS[@]}" >"$OUTPUT/common_enabled_channels.txt"

run_crate() {
  local index=$1 condition=$2 rate=$3 ticks=$4 log=$5
  local ip port calibration channel_csv
  ip=$(jq -r ".crates[$index].ip" "$CONFIG")
  port=$(jq -r ".crates[$index].port" "$CONFIG")
  calibration=$(jq -r ".crates[$index].calibration_dir" "$CONFIG")
  [[ "$calibration" = /* ]] || calibration="$REPO_ROOT/$calibration"
  channel_csv="${log%.log}_channels.csv"
  "$BINARY" --mode pulser-rate --ip "$ip" --port "$port" \
    --calibration-dir "$calibration" --period-ticks "$ticks" \
    --pulser-width-ticks "$width_ticks" --sync-pulser \
    --frames-per-block "$frames_per_block" \
    --threshold "$threshold" --events 0 --duration "$duration" --quiet \
    --channel-counts-csv "$channel_csv" "${COMMON_ENABLE_ARGS[@]}" \
    >"$log" 2>&1
}

valid_capture() {
  local status=$1 log=$2 hits
  [[ $status -eq 0 ]] || return 1
  hits=$(field "Total hits" "$log")
  [[ "$hits" =~ ^[0-9]+$ && $hits -gt 0 ]]
}

run_solo_with_retries() {
  local index=$1 rate=$2 ticks=$3 final_log=$4
  local attempt attempt_log status=1
  for ((attempt=1; attempt<=max_attempts; ++attempt)); do
    attempt_log="${final_log%.log}_attempt_${attempt}.log"
    if run_crate "$index" solo "$rate" "$ticks" "$attempt_log"; then
      status=0
    else
      status=$?
    fi
    cp "$attempt_log" "$final_log"
    if valid_capture "$status" "$attempt_log"; then
      return 0
    fi
    if ((attempt < max_attempts)); then
      echo "    attempt $attempt/$max_attempts invalid (status=$status, hits=$(field "Total hits" "$attempt_log")); retrying"
      sleep "$retry_delay_s"
    else
      echo "    attempt $attempt/$max_attempts invalid (status=$status, hits=$(field "Total hits" "$attempt_log")); giving up"
    fi
  done
  return 1
}

run_parallel_with_retries() {
  local rate=$1 ticks=$2
  local attempt index name attempt_log status all_valid
  for ((attempt=1; attempt<=max_attempts; ++attempt)); do
    declare -a attempt_logs=() pids=() statuses=()
    for index in 0 1; do
      name=$(jq -r ".crates[$index].name" "$CONFIG")
      attempt_log="$OUTPUT/logs/parallel_${name}_rate_${rate}_attempt_${attempt}.log"
      attempt_logs+=("$attempt_log")
      run_crate "$index" parallel "$rate" "$ticks" "$attempt_log" &
      pids+=("$!")
    done
    all_valid=1
    for index in 0 1; do
      if wait "${pids[$index]}"; then
        status=0
      else
        status=$?
      fi
      statuses+=("$status")
      name=$(jq -r ".crates[$index].name" "$CONFIG")
      cp "${attempt_logs[$index]}" "$OUTPUT/logs/parallel_${name}_rate_${rate}.log"
      if ! valid_capture "$status" "${attempt_logs[$index]}"; then
        all_valid=0
        statuses[$index]=1
      fi
    done
    if [[ $all_valid -eq 1 ]]; then
      PARALLEL_STATUSES=("${statuses[@]}")
      return 0
    fi
    if ((attempt < max_attempts)); then
      echo "    parallel attempt $attempt/$max_attempts invalid; retrying both crates"
      sleep "$retry_delay_s"
    else
      echo "    parallel attempt $attempt/$max_attempts invalid; giving up"
    fi
  done
  PARALLEL_STATUSES=("${statuses[@]}")
  return 1
}

field() {
  local label=$1 log=$2
  awk -F: -v label="$label" '$1 ~ "^" label "[[:space:]]*$" {
    value=$2; gsub(/^[[:space:]]+|[[:space:]]+$/, "", value); print value
  }' "$log" | tail -1
}

append_result() {
  local index=$1 condition=$2 rate=$3 ticks=$4 status=$5 log=$6
  local name configured_rate events hits bytes elapsed event_rate hit_rate data_rate
  name=$(jq -r ".crates[$index].name" "$CONFIG")
  configured_rate=$(awk -v ticks="$ticks" 'BEGIN {printf "%.9f", 2000000.0/ticks}')
  events=$(field "Events" "$log")
  hits=$(field "Total hits" "$log")
  bytes=$(field "Total bytes" "$log")
  elapsed=$(field "Elapsed" "$log" | awk '{print $1}')
  event_rate=$(field "Events/s" "$log")
  hit_rate=$(field "Hits/s" "$log")
  data_rate=$(field "Data MB/s" "$log")
  printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
    "$condition" "$name" "$rate" "$configured_rate" "$ticks" "${events:-}" "${hits:-}" \
    "${bytes:-}" "${elapsed:-}" "${event_rate:-}" "${hit_rate:-}" \
    "${data_rate:-}" "$status" "$log" >>"$RESULTS"
}

points=$(jq '.pulser_points | length' "$CONFIG")
if jq -e '.pulser_points[] | select((.period_ticks % 2) != 0)' "$CONFIG" >/dev/null; then
  echo "All internal-pulser period_ticks values must be even; odd values reproducibly produce no pulses." >&2
  exit 2
fi
echo "Dual-crate internal-pulser rate scan: $points rate point(s)"
echo "Each point: each crate alone, then both crates in parallel"
echo "Pulser: synchronous AUTO mode, width=$width_ticks ticks ($((width_ticks * 10)) ns)"
echo "Packetization: $frames_per_block frames/block; external triggers/event left at default"
echo "Common enabled mask: $common_channel_count identical non-hot channels per crate"
echo "Invalid/zero-hit points are retried up to $max_attempts time(s)"
echo "output: $OUTPUT"

for ((point=0; point<points; ++point)); do
  rate=$(jq -r ".pulser_points[$point].nominal_rate_hz" "$CONFIG")
  ticks=$(jq -r ".pulser_points[$point].period_ticks" "$CONFIG")
  configured_rate=$(awk -v ticks="$ticks" 'BEGIN {printf "%.3f", 2000000.0/ticks}')
  echo
  echo "[$((point+1))/$points] nominal rate=$rate Hz, period=$ticks ticks (register rate=$configured_rate Hz)"

  for index in 0 1; do
    name=$(jq -r ".crates[$index].name" "$CONFIG")
    log="$OUTPUT/logs/solo_${name}_rate_${rate}.log"
    echo "  solo: $name"
    if run_solo_with_retries "$index" "$rate" "$ticks" "$log"; then
      status=0
    else
      status=$?
    fi
    append_result "$index" solo "$rate" "$ticks" "$status" "$log"
    if [[ $status -ne 0 ]]; then
      echo "    failed (status $status); see $log"
    else
      echo "    hits/s=$(field "Hits/s" "$log")"
    fi
  done

  echo "  parallel: ${CRATE_NAMES[0]} + ${CRATE_NAMES[1]}"
  declare -a PARALLEL_LOGS=() PARALLEL_STATUSES=()
  for index in 0 1; do
    name=$(jq -r ".crates[$index].name" "$CONFIG")
    log="$OUTPUT/logs/parallel_${name}_rate_${rate}.log"
    PARALLEL_LOGS+=("$log")
  done
  if run_parallel_with_retries "$rate" "$ticks"; then
    parallel_status=0
  else
    parallel_status=$?
  fi
  for index in 0 1; do
    status=${PARALLEL_STATUSES[$index]:-$parallel_status}
    append_result "$index" parallel "$rate" "$ticks" "$status" \
      "${PARALLEL_LOGS[$index]}"
  done
  first_rate=$(field "Hits/s" "${PARALLEL_LOGS[0]}")
  second_rate=$(field "Hits/s" "${PARALLEL_LOGS[1]}")
  if [[ -n "$first_rate" && -n "$second_rate" ]]; then
    combined=$(awk -v a="$first_rate" -v b="$second_rate" 'BEGIN {printf "%.2f", a+b}')
    echo "    hits/s: ${CRATE_NAMES[0]}=$first_rate, ${CRATE_NAMES[1]}=$second_rate, combined=$combined"
  else
    echo "    one or both parallel runs failed; inspect logs"
  fi
done

echo
echo "Scan complete: $OUTPUT"
echo "Results: $RESULTS"
