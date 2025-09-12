# Save the current directory
original_dir=$(pwd)
# Get the directory where the script is located
script_dir="$(cd "$(dirname "$0")" && pwd)"

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

# Kill screen sessions for the looped names
while IFS='=' read -r process_name screen_name || [ -n "$process_name" ]; do
    if [ -z "$screen_name" ]; then
        process_name=$(echo "$process_name" | tr -d '[:space:]')
        screen_name="$process_name"
    else
        process_name=$(echo "$process_name" | tr -d '[:space:]')
        screen_name=$(echo "$screen_name" | tr -d '[:space:]')
    fi

    screen -X -S "$screen_name" quit
    # Remove "_name" from the process name
    process_name_no_suffix="${process_name%_name}"
    echo "Killing $process_name_no_suffix ($screen_name)..."
done < "$screen_names_file"

# Change directory back to the original directory
cd "$original_dir"

