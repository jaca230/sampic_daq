#!/bin/bash

# --------------------------------------------------------------------------
# Resolve script directory
# --------------------------------------------------------------------------
SOURCE="${BASH_SOURCE[0]}"
while [ -L "$SOURCE" ]; do
    DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
    SOURCE=$(readlink "$SOURCE")
    [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
done
SCRIPT_DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )

# Optional index argument
INDEX="$1"
if [ -n "$INDEX" ]; then
    SESSION_NAME="sampic_frontend$(printf "%02d" "$INDEX")"
else
    SESSION_NAME="sampic_frontend"
fi

# Kill the screen session
if screen -list | grep -q "\.${SESSION_NAME}"; then
    screen -S "$SESSION_NAME" -X quit
    echo "Killed screen session '$SESSION_NAME'."
else
    echo "No screen session named '$SESSION_NAME' found."
fi
