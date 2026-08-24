#!/bin/bash

# This script must be sourced so activation and exported variables remain in
# the caller's shell.
if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    echo "ERROR: source this script instead of executing it:" >&2
    echo "  source scripts/setup_env.sh" >&2
    exit 1
fi

SAMPIC_SETUP_SCRIPT_DIRECTORY="$(
    cd -P "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd
)"
# shellcheck disable=SC1091
source "$SAMPIC_SETUP_SCRIPT_DIRECTORY/environment/environment_paths.sh"

if [ -f "$SAMPIC_PROJECT_ROOT/.env" ]; then
    # shellcheck disable=SC1091
    source "$SAMPIC_PROJECT_ROOT/.env"
fi

if [ ! -x "$SAMPIC_MICROMAMBA" ] ||
   [ ! -d "$SAMPIC_ENV_PREFIX/conda-meta" ]; then
    echo "Environment '$SAMPIC_ENV_NAME' is not installed; creating it now."
    if ! "$SAMPIC_SETUP_SCRIPT_DIRECTORY/environment/create_env.sh" \
        --no-check
    then
        echo "ERROR: failed to create environment '$SAMPIC_ENV_NAME'." >&2
        return 1
    fi
fi

# Deactivate the old plain Python venv if it is active.
if [ -n "${VIRTUAL_ENV:-}" ] && declare -F deactivate >/dev/null 2>&1; then
    deactivate
fi

export MAMBA_ROOT_PREFIX="$SAMPIC_MAMBA_ROOT_PREFIX"
eval "$("$SAMPIC_MICROMAMBA" shell hook --shell bash)"
micromamba activate "$SAMPIC_ENV_NAME"

# The installed MIDAS libraries were built against the host libc. Keep the
# frontend on the host toolchain instead of Conda's compatibility sysroot.
# ROOT-only tools select root-config's compiler in their own build script.
SAMPIC_DEFAULT_HOST_CC="/usr/bin/cc"
SAMPIC_DEFAULT_HOST_CXX="/usr/bin/c++"
if [ ! -x "$SAMPIC_DEFAULT_HOST_CC" ]; then
    SAMPIC_DEFAULT_HOST_CC="$(command -v cc)"
fi
if [ ! -x "$SAMPIC_DEFAULT_HOST_CXX" ]; then
    SAMPIC_DEFAULT_HOST_CXX="$(command -v c++)"
fi
export CC="${SAMPIC_HOST_CC:-$SAMPIC_DEFAULT_HOST_CC}"
export CXX="${SAMPIC_HOST_CXX:-$SAMPIC_DEFAULT_HOST_CXX}"

prepend_environment_path() {
    local variable_name="$1"
    local directory="$2"
    local current_value="${!variable_name:-}"

    if [ ! -d "$directory" ]; then
        return
    fi
    case ":$current_value:" in
        *":$directory:"*)
            return
            ;;
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

prepend_environment_path PATH "$MIDASSYS/bin"
prepend_environment_path PYTHONPATH "$MIDASSYS/python"
prepend_environment_path CMAKE_PREFIX_PATH "$CONDA_PREFIX"
prepend_environment_path CMAKE_PREFIX_PATH "$MIDASSYS"
prepend_environment_path LIBRARY_PATH "$MIDASSYS/lib"
prepend_environment_path LD_LIBRARY_PATH "$MIDASSYS/lib"
prepend_environment_path PKG_CONFIG_PATH "$MIDASSYS/lib/pkgconfig"

prepend_environment_path LIBRARY_PATH "$SAMPIC_ROOT/lib"
prepend_environment_path LD_LIBRARY_PATH "$SAMPIC_ROOT/lib"
prepend_environment_path \
    LIBRARY_PATH "$SAMPIC_ROOT/lpdevc_install/lib"
prepend_environment_path \
    LD_LIBRARY_PATH "$SAMPIC_ROOT/lpdevc_install/lib"

echo "Activated $SAMPIC_ENV_NAME"
echo "  CONDA_PREFIX=$CONDA_PREFIX"
echo "  ROOT=$(command -v root-config 2>/dev/null || echo missing)"
echo "  CC=${CC:-missing}"
echo "  CXX=${CXX:-missing}"
echo "  MIDASSYS=$MIDASSYS"
echo "  MIDAS_EXPTAB=$MIDAS_EXPTAB"
echo "  SAMPIC_DAQ_DIR=$SAMPIC_DAQ_DIR"
