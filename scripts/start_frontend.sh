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

# Define the path of the executable
EXECUTABLE_PATH="$script_directory/../bin/sampic_frontend"

# Check if the executable exists and is runnable
if [ ! -x "$EXECUTABLE_PATH" ]; then
    echo "Error: Executable '$EXECUTABLE_PATH' not found or not executable." >&2
    exit 1
fi

# Run the executable with the -i argument
echo "Running Data Simulator with index $INDEX_STR..."
"$EXECUTABLE_PATH" -i "$INDEX"

