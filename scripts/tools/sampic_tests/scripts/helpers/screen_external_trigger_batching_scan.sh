#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/../..")
SESSION_NAME=${SCREEN_SESSION:-external_trigger_batching_scan}
LOG_DIR="$PROJECT_DIR/data/external_trigger_batching_scan/screen_logs"
LOG_FILE=${SCREEN_LOG:-"$LOG_DIR/${SESSION_NAME}_$(date +%Y%m%d_%H%M%S).log"}
CMD="$SCRIPT_DIR/external_trigger_batching_scan.sh"

if ! command -v screen >/dev/null 2>&1; then
  echo "Error: 'screen' is not installed." >&2
  exit 1
fi

if [[ ! -x "$CMD" ]]; then
  echo "Error: $CMD not found or not executable." >&2
  exit 1
fi

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  "$CMD" --help
  exit 0
fi

if screen -ls | grep -q "[.]${SESSION_NAME}[[:space:]]"; then
  echo "Error: screen session '$SESSION_NAME' already exists." >&2
  echo "Attach with: screen -r $SESSION_NAME" >&2
  exit 1
fi

mkdir -p "$LOG_DIR"
CMD_STR=$(printf '%q ' "$CMD" "$@")
SCREEN_CMD="${CMD_STR}; scan_status=\$?; printf '\\n[screen wrapper] scan exited with status %s\\n' \"\$scan_status\"; exit \"\$scan_status\""
screen \
  -L \
  -Logfile "$LOG_FILE" \
  -dmS "$SESSION_NAME" \
  bash -lc "$SCREEN_CMD"

echo "Started external-trigger batching scan in screen session '$SESSION_NAME'."
echo "Attach:  screen -r $SESSION_NAME"
echo "Detach:  Ctrl-A, then D"
echo "Status:  screen -ls"
echo "Log:     tail -f $LOG_FILE"
echo "Stop:    attach and press Ctrl-C once; the active SAMPIC run will finish safely before the scan exits."
