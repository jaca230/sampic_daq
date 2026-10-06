#!/bin/bash

# Configure only what is needed to run the built DAQ and MIDAS tools. This
# script intentionally performs no downloads, package installation, or build.
if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    echo "ERROR: source this script instead of executing it:" >&2
    echo "  source scripts/environment/activate_runtime.sh" >&2
    exit 1
fi

SAMPIC_RUNTIME_SCRIPT_DIRECTORY="$(
    cd -P "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd
)"
# shellcheck disable=SC1091
source "$SAMPIC_RUNTIME_SCRIPT_DIRECTORY/environment_paths.sh"

if [ -f "$SAMPIC_PROJECT_ROOT/.env" ]; then
    # shellcheck disable=SC1091
    source "$SAMPIC_PROJECT_ROOT/.env"
fi

prepend_sampic_path() {
    local variable_name="$1"
    local directory="$2"
    local current_value="${!variable_name:-}"

    if [ ! -d "$directory" ]; then
        return
    fi
    case ":$current_value:" in
        *":$directory:"*) return ;;
    esac
    export "$variable_name=$directory${current_value:+:$current_value}"
}

SAMPIC_OWNER_ROOT="$(
    cd "$SAMPIC_PROJECT_ROOT/../../../.." >/dev/null 2>&1 && pwd
)"

export SAMPIC_DAQ_DIR="$SAMPIC_PROJECT_ROOT"
export SAMPIC_ROOT="$SAMPIC_PROJECT_ROOT/external/sampic_256ch_lib"
export MIDASSYS="${MIDASSYS:-$SAMPIC_OWNER_ROOT/software/midas}"
export MIDAS_EXPTAB="${MIDAS_EXPTAB:-$SAMPIC_PROJECT_ROOT/../../midas_data/online/exptab}"
export MIDAS_EXPT_NAME="${MIDAS_EXPT_NAME:-SAMPIC}"

prepend_sampic_path PATH "$MIDASSYS/bin"
prepend_sampic_path PYTHONPATH "$MIDASSYS/python"
prepend_sampic_path CMAKE_PREFIX_PATH "$MIDASSYS"
prepend_sampic_path LIBRARY_PATH "$MIDASSYS/lib"
prepend_sampic_path LD_LIBRARY_PATH "$MIDASSYS/lib"
prepend_sampic_path PKG_CONFIG_PATH "$MIDASSYS/lib/pkgconfig"
prepend_sampic_path LD_LIBRARY_PATH "$SAMPIC_PROJECT_ROOT/build/lib"
prepend_sampic_path LIBRARY_PATH "$SAMPIC_ROOT/lib"
prepend_sampic_path LD_LIBRARY_PATH "$SAMPIC_ROOT/lib"
prepend_sampic_path LIBRARY_PATH "$SAMPIC_ROOT/lpdevc_install/lib"
prepend_sampic_path LD_LIBRARY_PATH "$SAMPIC_ROOT/lpdevc_install/lib"

echo "Activated SAMPIC runtime environment"
echo "  MIDASSYS=$MIDASSYS"
echo "  MIDAS_EXPTAB=$MIDAS_EXPTAB"
echo "  SAMPIC_DAQ_DIR=$SAMPIC_DAQ_DIR"
