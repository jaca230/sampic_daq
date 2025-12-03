#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
SESSION_NAME=${SCREEN_SESSION:-double_pulse_scan}
CMD="$SCRIPT_DIR/double_pulse_scan.sh"

if ! command -v screen >/dev/null 2>&1; then
  echo "Error: 'screen' is not installed." >&2
  exit 1
fi

if [[ ! -x "$CMD" ]]; then
  echo "Error: $CMD not found or not executable." >&2
  exit 1
fi

CMD_STR=$(printf '%q ' "$CMD" "$@")
screen -S "$SESSION_NAME" -dm bash -lc "$CMD_STR"
echo "Started double_pulse_scan in screen session '$SESSION_NAME'. Use 'screen -r $SESSION_NAME' to attach."
