#!/bin/bash

# Function to display usage information
usage() {
    echo "Usage: $0 -i <index>"
    echo "  -i <index>  Set frontend index (integer between 0 and 99)"
    exit 1
}

# Initialize variables
INDEX=""

# Parse command-line arguments
while getopts ":i:" opt; do
    case ${opt} in
        i )
            INDEX=$OPTARG
            ;;
        \? )
            echo "Error: Invalid Option -$OPTARG" >&2
            usage
            ;;
        : )
            echo "Error: Option -$OPTARG requires an argument." >&2
            usage
            ;;
    esac
done
shift $((OPTIND -1))

# Check if -i was provided
if [ -z "$INDEX" ]; then
    echo "Error: -i <index> is required."
    usage
fi

# Validate that INDEX is a number
if ! [[ "$INDEX" =~ ^[0-9]+$ ]]; then
    echo "Error: Index must be a valid integer." >&2
    usage
fi

# Validate that INDEX is between 0 and 99
if [ "$INDEX" -lt 0 ] || [ "$INDEX" -gt 99 ]; then
    echo "Error: Index must be between 0 and 99." >&2
    usage
fi

# Convert INDEX to two-digit format (e.g., 1 -> 01)
INDEX_STR=$(printf "%02d" "$INDEX")

# Get the directory of the script
SOURCE="${BASH_SOURCE[0]}"
while [ -L "$SOURCE" ]; do
    DIR="$(cd -P "$(dirname "$SOURCE")" >/dev/null 2>&1 && pwd)"
    SOURCE="$(readlink "$SOURCE")"
    [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
done
script_directory="$(cd -P "$(dirname "$SOURCE")" >/dev/null 2>&1 && pwd)"

# Define the name of the screen session and the executable
SESSION_NAME="data_simulator${INDEX_STR}"
EXECUTABLE_PATH="$script_directory/../bin/data_simulator"

# Check if the executable exists and is runnable
if [ ! -x "$EXECUTABLE_PATH" ]; then
    echo "Error: Executable '$EXECUTABLE_PATH' not found or not executable." >&2
    exit 1
fi

# Check if a screen session with the name already exists
if screen -list | grep -q "\.${SESSION_NAME}"; then
    echo "Error: Screen session '$SESSION_NAME' already exists. Please kill the existing session before starting a new one."
    exit 1
fi

# Start a new screen session and run the executable with the -i argument
screen -S "$SESSION_NAME" -dm bash -c "$EXECUTABLE_PATH -i $INDEX"

# Verify if the screen session was created successfully
if screen -list | grep -q "\.${SESSION_NAME}"; then
    echo "Started '$EXECUTABLE_PATH' with index $INDEX in a new screen session named '$SESSION_NAME'."
else
    echo "Error: Failed to start screen session '$SESSION_NAME'." >&2
    exit 1
fi
