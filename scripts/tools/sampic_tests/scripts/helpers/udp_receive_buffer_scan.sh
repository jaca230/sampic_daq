#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/../..")
CONFIG=${1:-"$PROJECT_DIR/config/udp_receive_buffer_scan.default.json"}
PRELOAD="$PROJECT_DIR/build/lib/libsampic_udp_rcvbuf_preload.so"
RUNNER="$SCRIPT_DIR/external_trigger_batching_scan.sh"

if [[ ! -f "$PRELOAD" ]]; then
  echo "UDP receive-buffer preload library not found: $PRELOAD" >&2
  echo "Build with: scripts/tools/sampic_tests/scripts/build.sh" >&2
  exit 1
fi
if ! command -v jq >/dev/null; then
  echo "jq is required to read the scan configuration." >&2
  exit 1
fi

POINT_CONFIG=$(jq -r '.point_scan_config' "$CONFIG")
OUTPUT_ROOT=$(jq -r '.output_root' "$CONFIG")
[[ "$POINT_CONFIG" = /* ]] || POINT_CONFIG="$PROJECT_DIR/$POINT_CONFIG"
[[ "$OUTPUT_ROOT" = /* ]] || OUTPUT_ROOT="$PROJECT_DIR/$OUTPUT_ROOT"
STAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT="$OUTPUT_ROOT/$STAMP"
mkdir -p "$OUTPUT"
cp "$CONFIG" "$OUTPUT/udp_receive_buffer_scan.json"

mapfile -t BUFFER_VALUES < <(jq -r '.receive_buffer_bytes[]' "$CONFIG")
echo "UDP receive-buffer scan: ${#BUFFER_VALUES[@]} setting(s)"
echo "point config: $POINT_CONFIG"
echo "output: $OUTPUT"

capture_udp_counters() {
  local destination=$1
  if command -v nstat >/dev/null; then
    nstat -az UdpRcvbufErrors UdpInErrors >"$destination" 2>&1 || true
  else
    awk '/^Udp:/{print}' /proc/net/snmp >"$destination" 2>&1 || true
  fi
}

for requested in "${BUFFER_VALUES[@]}"; do
  if [[ "$requested" -eq 0 ]]; then
    label="inherited_default"
  else
    label="requested_${requested}"
  fi
  destination="$OUTPUT/$label"
  mkdir -p "$destination"
  capture_udp_counters "$destination/udp_counters_before.txt"
  echo
  echo "[$label] starting fresh process and crate socket"
  SAMPIC_UDP_RCVBUF_BYTES="$requested" LD_PRELOAD="$PRELOAD" \
    "$RUNNER" --config "$POINT_CONFIG" --output-dir "$destination/data" \
    2>&1 | tee "$destination/scan.log"
  capture_udp_counters "$destination/udp_counters_after.txt"
done

echo "UDP receive-buffer scan complete: $OUTPUT"
