#!/bin/bash

# Get the directory of the script
SOURCE=${BASH_SOURCE[0]}
while [ -L "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
  SOURCE=$(readlink "$SOURCE")
  [[ $SOURCE != /* ]] && SOURCE=$DIR/$SOURCE # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
script_dir=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )

# Check if MIDASSYS is defined
if [ -z "$MIDASSYS" ]; then
    echo "MIDASSYS is not defined. Please set it before running this script."
    exit 1
fi

# Check if the screen_names.txt file exists in the script directory
screen_names_file="$script_dir/screen_names.txt"
if [ ! -f "$screen_names_file" ]; then
    echo "screen_names.txt file not found in $script_dir."
    exit 1
fi

# Save the current directory
original_dir=$(pwd)

# Change directory to midas bin
cd "$MIDASSYS/bin"

# Read screen session names and associated process names from the file
while IFS='=' read -r process_name screen_name || [ -n "$process_name" ]; do
    # If the line doesn't contain '=', set process_name to the entire line
    if [ -z "$screen_name" ]; then
        process_name=$(echo "$process_name" | tr -d '[:space:]')
        screen_name="$process_name"
    else
        # Remove leading and trailing whitespace from the process name
        process_name=$(echo "$process_name" | tr -d '[:space:]')

        # Remove leading and trailing whitespace from the screen name
        screen_name=$(echo "$screen_name" | tr -d '[:space:]')
    fi

    # Remove "_name" from the process name
    process_name_no_suffix="${process_name%_name}"

    # Start the respective process in the background using screen
    screen -d -m -S "$screen_name" "./$process_name_no_suffix" -e "$MIDAS_EXPT_NAME"
    echo "$process_name_no_suffix running in the background as $screen_name (Experiment $MIDAS_EXPT_NAME)..."
done < "$screen_names_file"

# List the available screen sessions
screen -ls

# Change directory back to the original directory
cd "$original_dir"
