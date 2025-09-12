#!/bin/bash

# ---------------------------------------------------------------
# Script Name: find_data_dir.sh
# Description: Finds the directory associated with the label
#              specified in MIDAS_EXPT_NAME from the experiment
#              table file pointed to by MIDAS_EXPTAB and writes
#              it to experiment_dir.txt.
# ---------------------------------------------------------------

# Exit immediately if a command exits with a non-zero status
set -e

# Function to display error messages and exit
error_exit() {
    echo "Error: $1" >&2
    exit 1
}

# Function to display usage information
usage() {
    echo "Usage: $0"
    echo "Finds the directory associated with MIDAS_EXPT_NAME from MIDAS_EXPTAB and writes it to experiment_dir.txt."
    echo "Ensure that MIDAS_EXPTAB and MIDAS_EXPT_NAME environment variables are set."
    exit 1
}

# Optional: Handle help flag
if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    usage
fi

# ---------------------------------------------------------------
# Determine the directory where the script resides
# ---------------------------------------------------------------

# Get the directory of the script
SOURCE="${BASH_SOURCE[0]}"
while [ -L "$SOURCE" ]; do
    DIR="$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )"
    SOURCE="$(readlink "$SOURCE")"
    [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
done
script_directory="$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )"

# ---------------------------------------------------------------
# Define paths relative to the script directory
# ---------------------------------------------------------------

# Path to experiment_dir.txt
OUTPUT_FILE="$script_directory/experiment_dir.txt"

# ---------------------------------------------------------------
# Main Functionality
# ---------------------------------------------------------------

# Check if MIDAS_EXPTAB environment variable is set
if [ -z "$MIDAS_EXPTAB" ]; then
    error_exit "MIDAS_EXPTAB environment variable is not set."
fi

# Check if MIDAS_EXPT_NAME environment variable is set
if [ -z "$MIDAS_EXPT_NAME" ]; then
    error_exit "MIDAS_EXPT_NAME environment variable is not set."
fi

# Check if the MIDAS_EXPTAB file exists and is readable
if [ ! -r "$MIDAS_EXPTAB" ]; then
    error_exit "MIDAS_EXPTAB file '$MIDAS_EXPTAB' does not exist or is not readable."
fi

# Extract the directory associated with MIDAS_EXPT_NAME using awk
directory=$(awk -v label="$MIDAS_EXPT_NAME" '
    $1 == label {print $2; found=1; exit}
    END {
        if (!found) {
            exit 1
        }
    }
' "$MIDAS_EXPTAB") || error_exit "No entry found for label '$MIDAS_EXPT_NAME' in '$MIDAS_EXPTAB'."

# Write the directory to the output file
echo "$directory" > "$OUTPUT_FILE"

# Confirm successful operation
echo "Directory for '$MIDAS_EXPT_NAME' has been written to '$OUTPUT_FILE'."
