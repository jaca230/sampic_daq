#!/bin/bash

# ---------------------------------------------------------------
# Script Name: delete_data.sh
# Description: Deletes all MIDAS data files (.mid and related
#              extensions) from the directory specified in
#              experiment_dir.txt. If experiment_dir.txt is
#              missing or unreadable, it calls find_data_dir.sh
#              to generate it. The script lists the first 10
#              files that will be deleted, calculates the total
#              space to be freed, and asks for user
#              confirmation before proceeding. After deletion,
#              it reports the space freed and the current
#              drive usage percentage.
# ---------------------------------------------------------------

# Exit immediately if a command exits with a non-zero status
set -e

# ---------------------------------------------------------------
# Color Definitions
# ---------------------------------------------------------------

# Check if the script is running in a terminal
if [ -t 1 ]; then
    # Define color codes
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    CYAN='\033[0;36m'  # Changed from BLUE to CYAN
    NC='\033[0m'        # No Color
else
    # If not a terminal, define empty variables
    RED=''
    GREEN=''
    YELLOW=''
    CYAN=''
    NC=''
fi

# ---------------------------------------------------------------
# Function Definitions
# ---------------------------------------------------------------

# Function to display error messages and exit
error_exit() {
    echo -e "${RED}Error: $1${NC}" >&2
    exit 1
}

# Function to display usage information
usage() {
    echo -e "${YELLOW}Usage: $0 [--dry-run]${NC}"
    echo -e "${YELLOW}Deletes all MIDAS .mid files from the specified data directory.${NC}"
    echo -e "${YELLOW}Ensure that MIDAS_EXPTAB and MIDAS_EXPT_NAME environment variables are set.${NC}"
    echo -e "${YELLOW}Options:${NC}"
    echo -e "${YELLOW}  --dry-run    Perform a trial run with no changes made.${NC}"
    exit 1
}

# Function to convert bytes to human-readable format
convert_bytes() {
    local bytes=$1
    if command -v numfmt >/dev/null 2>&1; then
        numfmt --to=iec --suffix=B <<< "$bytes"
    else
        # Fallback if numfmt is not available
        local units=("B" "KB" "MB" "GB" "TB")
        local unit_index=0
        while (( bytes >= 1024 && unit_index < 4 )); do
            bytes=$((bytes / 1024))
            unit_index=$((unit_index + 1))
        done
        echo "${bytes}${units[$unit_index]}"
    fi
}

# ---------------------------------------------------------------
# Handle Help and Dry Run Flags
# ---------------------------------------------------------------

DRY_RUN=false
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        *)
            echo -e "${YELLOW}Unknown option: $1${NC}"
            usage
            ;;
    esac
done

# ---------------------------------------------------------------
# Determine the Directory Where the Script Resides
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
# Define Paths Relative to the Script Directory
# ---------------------------------------------------------------

# Path to find_data_dir.sh
FIND_DATA_DIR_SCRIPT="$script_directory/find_data_dir.sh"

# Path to experiment_dir.txt
OUTPUT_FILE="$script_directory/experiment_dir.txt"

# Path to log file (optional)
LOG_FILE="$script_directory/delete_data.log"

# ---------------------------------------------------------------
# Function to Get the Data Directory
# ---------------------------------------------------------------
get_data_directory() {
    # Check if experiment_dir.txt exists and is readable
    if [[ -r "$OUTPUT_FILE" ]]; then
        directory=$(cat "$OUTPUT_FILE")
    else
        echo -e "${YELLOW}experiment_dir.txt not found or unreadable. Attempting to generate it using find_data_dir.sh...${NC}"
        # Check if find_data_dir.sh exists and is executable
        if [[ ! -x "$FIND_DATA_DIR_SCRIPT" ]]; then
            error_exit "find_data_dir.sh not found or not executable in '$script_directory'."
        fi
        # Call find_data_dir.sh to generate experiment_dir.txt
        "$FIND_DATA_DIR_SCRIPT"
        # Verify that experiment_dir.txt was created
        if [[ -r "$OUTPUT_FILE" ]]; then
            directory=$(cat "$OUTPUT_FILE")
        else
            error_exit "Failed to generate '$OUTPUT_FILE' using find_data_dir.sh."
        fi
    fi

    # Trim any leading/trailing whitespace
    directory=$(echo "$directory" | xargs)

    # Verify that the directory exists and is a directory
    if [[ ! -d "$directory" ]]; then
        error_exit "The directory '$directory' does not exist or is not a directory."
    fi

    echo "$directory"
}

# ---------------------------------------------------------------
# Function to Calculate Total Size of Files
# ---------------------------------------------------------------
calculate_total_size() {
    local files="$1"
    if [ -z "$files" ]; then
        echo "0"
        return
    fi

    # Calculate total size in bytes
    total_size=$(find "$DATA_DIR" -type f \( -name "*.mid" -o -name "*.mid.*" \) -printf "%s\n" | awk '{sum += $1} END {print sum}')

    # Handle case where no files are found
    if [ -z "$total_size" ]; then
        total_size=0
    fi

    echo "$total_size"
}

# ---------------------------------------------------------------
# Function to Get Drive Usage Percentage
# ---------------------------------------------------------------
get_drive_usage_percentage() {
    local dir="$1"
    # Get the usage percentage from df
    usage_percent=$(df -h "$dir" | awk 'NR==2 {print $5}' | tr -d '%')
    echo "$usage_percent"
}

# ---------------------------------------------------------------
# Main Execution Flow
# ---------------------------------------------------------------

# Prevent deletion from root, home, or other critical directories
if [[ "$DATA_DIR" == "/" || "$DATA_DIR" == "/home" || "$DATA_DIR" == "/root" ]]; then
    error_exit "Refusing to delete files from critical system directories."
fi

# Get the data directory
DATA_DIR=$(get_data_directory)

# Inform the user about the directory being cleaned
echo -e "${CYAN}Data Directory to Clean: $DATA_DIR${NC}"

# Find all .mid files (including variations) in the directory
# Patterns include:
#   *.mid
#   *.mid.*
mid_files=$(find "$DATA_DIR" -type f \( -name "*.mid" -o -name "*.mid.*" \))

# Count the number of files found
file_count=$(echo "$mid_files" | grep -c .)

# Check if there are any files to delete
if [[ "$file_count" -eq 0 ]]; then
    echo -e "${GREEN}No .mid files found in '$DATA_DIR'. Nothing to delete.${NC}"
    exit 0
fi

# Calculate the total size of files to be deleted
total_size_bytes=$(calculate_total_size "$mid_files")
human_readable_size=$(convert_bytes "$total_size_bytes")

# List the first 10 files that will be deleted
echo -e "${YELLOW}The following $file_count .mid files will be deleted:${NC}"
echo "$mid_files" | head -n 10

# If there are more than 10 files, indicate that not all are listed
if [[ "$file_count" -gt 10 ]]; then
    echo -e "${YELLOW}...and $(($file_count - 10)) more files.${NC}"
fi

# Display the total size to be freed
echo -e "${CYAN}Total space to be freed: $human_readable_size${NC}"

# Handle Dry Run Mode
if $DRY_RUN; then
    echo -e "${YELLOW}Dry run mode enabled. No files will be deleted.${NC}"
    exit 0
fi

# Prompt the user for confirmation
read -p "$(echo -e "${YELLOW}Are you sure you want to delete these files? (y/N): ${NC}")" confirmation

# Convert the response to lowercase
confirmation=${confirmation,,}

# Check if the user confirmed
if [[ "$confirmation" == "y" || "$confirmation" == "yes" ]]; then
    echo -e "${CYAN}Deleting .mid files from '$DATA_DIR'...${NC}"
    
    # Calculate space before deletion (for accurate measurement)
    space_before=$(df -B1 --output=avail "$DATA_DIR" | tail -1 | tr -d ' ')

    # Delete the files and capture deleted file paths
    deleted_files=$(find "$DATA_DIR" -type f \( -name "*.mid" -o -name "*.mid.*" \) -print -delete)

    # Calculate space after deletion
    space_after=$(df -B1 --output=avail "$DATA_DIR" | tail -1 | tr -d ' ')

    # Calculate space freed in bytes
    space_freed=$((space_after - space_before))

    # Handle negative or zero space_freed
    if [[ "$space_freed" -lt 0 ]]; then
        space_freed=0
    fi

    # Convert bytes to human-readable format
    human_readable_freed=$(convert_bytes "$space_freed")

    # Get the current drive usage percentage
    usage_percent=$(get_drive_usage_percentage "$DATA_DIR")

    # Log deletion (optional)
    if [[ "$file_count" -gt 0 ]]; then
        echo "$deleted_files" >> "$LOG_FILE"
        echo -e "${GREEN}Deletion logged in '$LOG_FILE'.${NC}"
    fi

    echo -e "${GREEN}Deletion completed. $file_count files have been removed from '$DATA_DIR'.${NC}"
    echo -e "${GREEN}Space freed: $human_readable_freed.${NC}"
    echo -e "${GREEN}Current drive usage: $usage_percent% full.${NC}"
else
    echo -e "${YELLOW}Deletion aborted by the user. No files were deleted.${NC}"
    exit 0
fi
