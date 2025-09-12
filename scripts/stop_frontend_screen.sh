#!/bin/bash

# Get the directory of the script
SOURCE="${BASH_SOURCE[0]}"
while [ -L "$SOURCE" ]; do
    DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
    SOURCE=$(readlink "$SOURCE")
    [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
done
script_directory=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )

# Define the name of the screen session
SESSION_NAME="data_simulator"

# Check if a screen session with the name exists
if ! screen -list | grep -q "$SESSION_NAME"; then
    echo "No screen session named '$SESSION_NAME' found."
    exit 1
fi

# Kill the screen session
screen -S "$SESSION_NAME" -X quit

echo "Killed screen session named '$SESSION_NAME'."
