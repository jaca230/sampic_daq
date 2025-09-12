#!/bin/bash

# Get the directory of the script
SOURCE=${BASH_SOURCE[0]}
while [ -L "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
  SOURCE=$(readlink "$SOURCE")
  [[ $SOURCE != /* ]] && SOURCE=$DIR/$SOURCE # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
script_directory=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )

# Declare a global variable to hold the result
dir_choice=""

# Function to handle multiple options
handle_multiple_options() {
    local name="$1"
    shift
    local options=("$@")

    # Split each option by newline and treat each line as a separate option
    declare -A seen_options
    unique_options=()
    for option in "${options[@]}"; do
        IFS=$'\n' read -ra lines <<< "$option"
        for line in "${lines[@]}"; do
            cleaned_option=$(realpath -s "$line")
            if [ ! -n "${seen_options["$cleaned_option"]}" ]; then
                unique_options+=("$cleaned_option")
                seen_options["$cleaned_option"]=1
            fi
        done
    done
    eval "$name=${unique_options[0]}"
    if [ "${#unique_options[@]}" -gt 1 ]; then
        echo "$name has multiple options:"
        for option in "${unique_options[@]}"; do
            echo "  - $option"
        done
        echo "Using the first option: $name=${unique_options[0]}"
        echo
    fi

    dir_choice="${unique_options[0]}"
}

# Function to search for directories or files
# Arguments:
# 1. Timeout duration in seconds
# 2. Name of the directory to search for
# 3. Environment variable name to set
# 4. "d" for directory or "f" for file
search_and_set_directory_or_file() {
    local timeout_duration="$1"
    local search_name="$2"
    local var_name="$3"
    local dir_type="$4"
    local file_type="$5"

    current_dir=($script_directory)
    result_dirs=()
    start_time=$(date +%s)  # Record the start time

    while [ $(( $(date +%s) - start_time )) -lt $timeout_duration ]; do
        if [ "$dir_type" == "d" ]; then
            result=$(find "$current_dir" -maxdepth 3 -type d -name "$search_name" 2>/dev/null)
        elif [ "$file_type" == "f" ]; then
            result=$(find "$current_dir" -maxdepth 3 -type f -name "$search_name" 2>/dev/null)
        fi
        if [ -n "$result" ]; then
            result_dirs+=("$result")
        fi

        if [ "$current_dir" == "/" ]; then
            break  # Stop when the root directory is reached
        fi
        current_dir=$(dirname "$current_dir")
    done

    if [ "${#result_dirs[@]}" -gt 0 ]; then
        handle_multiple_options "$var_name" "${result_dirs[@]}"
        eval "$var_name=\"$dir_choice\""
    elif [ -z "${!var_name}" ]; then
        echo "$var_name not found within $timeout_duration seconds."
    fi
}

# Search for "midas" directory backward with a timeout of 5 seconds
search_and_set_directory_or_file 5 "midas" "MIDASSYS" "d" ""

# Search for "exptab" file backward with a timeout of 5 seconds
search_and_set_directory_or_file 5 "exptab" "MIDAS_EXPTAB" "" "f"


MIDAS_EXPT_NAME=$(timeout 5 sed -n '1{p;q}' "$MIDAS_EXPTAB" | awk '{print $1}')
if [ -z "$MIDAS_EXPT_NAME" ]; then
    echo "Warning: The first line in $MIDAS_EXPTAB is empty or doesn't exist."
fi


# Write the environment variables to "environment_variables.txt"
echo "MIDASSYS=$MIDASSYS" > environment_variables.txt
echo "MIDAS_EXPTAB=$MIDAS_EXPTAB" >> environment_variables.txt
echo "MIDAS_EXPT_NAME=$MIDAS_EXPT_NAME" >> environment_variables.txt
echo
echo "Directory detection completed and environment variables saved to environment_variables.txt."
