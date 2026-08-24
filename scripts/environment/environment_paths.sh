#!/bin/bash

# Shared path definitions for the project-local development environment.
# This file is sourced by the environment scripts.

SAMPIC_ENV_SCRIPT_DIRECTORY="$(
    cd -P "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd
)"
SAMPIC_PROJECT_ROOT="$(
    cd "$SAMPIC_ENV_SCRIPT_DIRECTORY/../.." >/dev/null 2>&1 && pwd
)"

export SAMPIC_PROJECT_ROOT
export SAMPIC_ENV_NAME="sampic_dev"
export SAMPIC_MAMBA_ROOT_PREFIX="$SAMPIC_PROJECT_ROOT/.venv"
export SAMPIC_ENV_PREFIX="$SAMPIC_MAMBA_ROOT_PREFIX/envs/$SAMPIC_ENV_NAME"
export SAMPIC_MICROMAMBA="$SAMPIC_PROJECT_ROOT/.tools/micromamba"
export SAMPIC_ENV_FILE="$SAMPIC_PROJECT_ROOT/environment.yml"
