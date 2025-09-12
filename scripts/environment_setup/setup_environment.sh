#!/bin/bash

# Function to display usage message
usage() {
    echo "Usage: $0 [-a|--add]"
    echo "Flags:"
    echo "  -a, --add   Add MIDASSYS/bin to PATH if MIDASSYS is defined."
    # Add more flags and descriptions here as needed.
}

# Flags
add_midassys_to_path=false
run_script=true  # Set to true by default
quit=false  # Initialize quit to false

# Parse command line flags
while [[ $# -gt 0 && $quit == false ]]; do
    case "$1" in
        -a|--add)
            add_midassys_to_path=true
            shift
            ;;
        -h|--help)
            usage
            quit=true  # Set quit to true when invalid option is encountered
            run_script=false  # Set to false when invalid option is encountered
            ;;
        *)
            echo "Invalid option: $1" >&2
            usage
            quit=true  # Set quit to true when invalid option is encountered
            run_script=false  # Set to false when invalid option is encountered
            ;;
    esac
done

# Check if run_script is true before executing the main functionality
if [ "$run_script" = true ]; then

    # Read environment variables from the file
    while IFS= read -r line; do
        # Split the line into variable name and value
        name=$(echo "$line" | cut -d'=' -f1)
        value=$(echo "$line" | cut -d'=' -f2-)

        # Check if the value is empty, and if not, export the variable
        if [ -n "$value" ]; then
            export "$name"="$value"
        else
            echo "Warning: $name is empty."
        fi
    done < environment_variables.txt


    # Check if the flag for adding MIDASSYS to PATH is present
    if [ "$add_midassys_to_path" = true ]; then
        if [ -n "$MIDASSYS" ]; then
            export PATH="$MIDASSYS/bin:$PATH"
            echo "Added MIDASSYS to path."
        else
            echo "Warning: MIDASSYS is not set. Cannot add to PATH."
        fi
    fi


    # Echo the environment variables
    echo "MIDAS_EXPT_NAME environment variable set to: $MIDAS_EXPT_NAME"
    echo "MIDASSYS environment variable set to: $MIDASSYS"
    echo "MIDAS_EXPTAB environment variable set to: $MIDAS_EXPTAB"
fi
