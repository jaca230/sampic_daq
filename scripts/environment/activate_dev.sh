#!/bin/bash

# Activate the reproducible developer toolchain, creating it on first use.
if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    echo "ERROR: source this script instead of executing it:" >&2
    echo "  source scripts/environment/activate_dev.sh" >&2
    exit 1
fi

SAMPIC_DEV_SCRIPT_DIRECTORY="$(
    cd -P "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd
)"
# shellcheck disable=SC1091
source "$SAMPIC_DEV_SCRIPT_DIRECTORY/environment_paths.sh"

if [ ! -x "$SAMPIC_MICROMAMBA" ] ||
   [ ! -d "$SAMPIC_ENV_PREFIX/conda-meta" ]; then
    echo "Developer environment '$SAMPIC_ENV_NAME' is not installed; creating it."
    if ! "$SAMPIC_DEV_SCRIPT_DIRECTORY/create_env.sh" --no-check; then
        echo "ERROR: failed to create developer environment '$SAMPIC_ENV_NAME'." >&2
        return 1
    fi
fi

if [ -n "${VIRTUAL_ENV:-}" ] && declare -F deactivate >/dev/null 2>&1; then
    deactivate
fi

export MAMBA_ROOT_PREFIX="$SAMPIC_MAMBA_ROOT_PREFIX"
eval "$("$SAMPIC_MICROMAMBA" shell hook --shell bash)"
micromamba activate "$SAMPIC_ENV_NAME"

# MIDAS is built against the host libc, so frontend builds use host compilers.
export CC="${SAMPIC_HOST_CC:-/usr/bin/cc}"
export CXX="${SAMPIC_HOST_CXX:-/usr/bin/c++}"

# shellcheck disable=SC1091
source "$SAMPIC_DEV_SCRIPT_DIRECTORY/activate_runtime.sh"
prepend_sampic_path CMAKE_PREFIX_PATH "$CONDA_PREFIX"

echo "Activated SAMPIC developer environment '$SAMPIC_ENV_NAME'"
echo "  CONDA_PREFIX=$CONDA_PREFIX"
echo "  ROOT=$(command -v root-config 2>/dev/null || echo missing)"
echo "  CC=$CC"
echo "  CXX=$CXX"
