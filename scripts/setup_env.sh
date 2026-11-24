#!/bin/bash
# Source this script to configure the DAQ runtime environment.
if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    echo "Source this script: source scripts/setup_env.sh" >&2
    exit 1
fi
source "$(dirname "${BASH_SOURCE[0]}")/environment/activate_runtime.sh"
