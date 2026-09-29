#!/usr/bin/env bash

set -euo pipefail

SESSION_NAME="sampic-data01-move"
SCRIPT_PATH="$(readlink -f "${BASH_SOURCE[0]}")"
REPO_ROOT="$(git -C "$(dirname "$SCRIPT_PATH")" rev-parse --show-toplevel)"
DATA_DIRECTORY="$REPO_ROOT/scripts/tools/sampic_tests/data"
SOURCE="$DATA_DIRECTORY/external_trigger_batching_scan"
DESTINATION_ROOT="/data01/pioneer/midas_sampic/sampic_daq/sampic_tests_data"
DESTINATION="$DESTINATION_ROOT/external_trigger_batching_scan"
LOG_DIRECTORY="$REPO_ROOT/.artifacts"
LOG_FILE="$LOG_DIRECTORY/move_sampic_test_data_to_data01.log"

session_is_running() {
  screen -ls 2>/dev/null | grep -q "[.]${SESSION_NAME}[[:space:]]"
}

show_usage() {
  cat <<EOF
Usage: $0 [start|status|attach|log]

  start   Start the transfer in a detached screen session (default).
  status  Show whether the screen session is still running.
  attach  Attach to the running screen session.
  log     Follow the transfer log.
EOF
}

run_transfer() {
  mkdir -p "$LOG_DIRECTORY" "$DESTINATION_ROOT"
  exec > >(tee -a "$LOG_FILE") 2>&1

  echo "[$(date --iso-8601=seconds)] Starting SAMPIC data migration"
  echo "Source:      $SOURCE"
  echo "Destination: $DESTINATION"

  if [[ -L "$SOURCE" ]]; then
    echo "Source is already a symbolic link; nothing to transfer."
    exit 0
  fi

  if [[ ! -d "$SOURCE" ]]; then
    echo "Source directory does not exist: $SOURCE" >&2
    exit 1
  fi

  if [[ ! -w "$DESTINATION_ROOT" ]]; then
    echo "Destination is not writable: $DESTINATION_ROOT" >&2
    exit 1
  fi

  # The destination already contains files moved by the earlier interrupted
  # migration. GNU cp merges the remaining source tree into that directory.
  # The source is removed only if the complete copy exits successfully.
  time cp -a "$SOURCE" "$DESTINATION_ROOT/"

  echo "[$(date --iso-8601=seconds)] Copy completed; removing local source"
  rm -rf -- "$SOURCE"
  ln -s "$DESTINATION" "$SOURCE"

  # Remove only rsync temporary files left by the interrupted migration.
  find "$DESTINATION" -type f \( \
    -name '.hits.csv.??????' -o \
    -name '.triggers.csv.??????' -o \
    -name '.packets.csv.??????' -o \
    -name '.metadata.json.??????' -o \
    -name '.collection_timing.csv.??????' -o \
    -name '.run.log.??????' \
  \) -delete

  echo "[$(date --iso-8601=seconds)] Migration complete"
  ls -ld "$SOURCE"
  df -h "$DESTINATION_ROOT"
}

start_transfer() {
  command -v screen >/dev/null || {
    echo "screen is not installed or not on PATH" >&2
    exit 1
  }

  mkdir -p "$LOG_DIRECTORY" "$DESTINATION_ROOT"

  if session_is_running; then
    echo "Screen session '$SESSION_NAME' is already running."
    echo "Attach with: $SCRIPT_PATH attach"
    exit 1
  fi

  screen -dmS "$SESSION_NAME" bash "$SCRIPT_PATH" _run
  echo "Started detached screen session: $SESSION_NAME"
  echo "Check status: $SCRIPT_PATH status"
  echo "Attach:       $SCRIPT_PATH attach"
  echo "Follow log:   $SCRIPT_PATH log"
}

case "${1:-start}" in
  start)
    start_transfer
    ;;
  status)
    if session_is_running; then
      echo "Screen session '$SESSION_NAME' is running."
      screen -ls | grep "[.]${SESSION_NAME}[[:space:]]"
    else
      echo "Screen session '$SESSION_NAME' is not running."
      [[ -f "$LOG_FILE" ]] && tail -n 10 "$LOG_FILE"
    fi
    ;;
  attach)
    exec screen -r "$SESSION_NAME"
    ;;
  log)
    mkdir -p "$LOG_DIRECTORY"
    touch "$LOG_FILE"
    exec tail -F "$LOG_FILE"
    ;;
  _run)
    run_transfer
    ;;
  -h|--help|help)
    show_usage
    ;;
  *)
    show_usage >&2
    exit 2
    ;;
esac
