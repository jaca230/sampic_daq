#!/bin/bash

set -e

SCRIPT_DIRECTORY="$(
    cd -P "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd
)"
# shellcheck disable=SC1091
source "$SCRIPT_DIRECTORY/environment_paths.sh"

show_help() {
    cat <<EOF
Usage: ./scripts/environment/create_env.sh [OPTIONS]

Create or update the project-local '$SAMPIC_ENV_NAME' micromamba environment.

Options:
  --recreate    Remove and recreate only the managed environment prefix.
  --no-check    Skip the post-install environment verification.
  --dry-run     Print the managed paths and exit without downloading.
  -h, --help    Show this help.

The micromamba root and package cache live under:
  $SAMPIC_MAMBA_ROOT_PREFIX

No system Conda installation or shell initialization is required.
EOF
}

RECREATE=false
RUN_CHECK=true
DRY_RUN=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --recreate)
            RECREATE=true
            shift
            ;;
        --no-check)
            RUN_CHECK=false
            shift
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo "ERROR: unknown option '$1'" >&2
            show_help >&2
            exit 2
            ;;
    esac
done

if [ "$DRY_RUN" = true ]; then
    cat <<EOF
Environment name:   $SAMPIC_ENV_NAME
Environment file:   $SAMPIC_ENV_FILE
Micromamba binary:  $SAMPIC_MICROMAMBA
Micromamba root:    $SAMPIC_MAMBA_ROOT_PREFIX
Environment prefix: $SAMPIC_ENV_PREFIX
Action:              $([ "$RECREATE" = true ] && echo recreate || echo create-or-update)
Post-install check:  $RUN_CHECK

No files were downloaded or changed.
EOF
    exit 0
fi

bootstrap_micromamba() {
    if [ -x "$SAMPIC_MICROMAMBA" ]; then
        return
    fi

    local platform
    case "$(uname -s)-$(uname -m)" in
        Linux-x86_64)
            platform="linux-64"
            ;;
        Linux-aarch64|Linux-arm64)
            platform="linux-aarch64"
            ;;
        Darwin-x86_64)
            platform="osx-64"
            ;;
        Darwin-arm64)
            platform="osx-arm64"
            ;;
        *)
            echo "ERROR: unsupported platform $(uname -s)-$(uname -m)" >&2
            exit 1
            ;;
    esac

    for command_name in curl tar; do
        if ! command -v "$command_name" >/dev/null 2>&1; then
            echo "ERROR: '$command_name' is required to install micromamba" >&2
            exit 1
        fi
    done

    local temporary_directory
    temporary_directory="$(mktemp -d)"
    trap 'rm -rf "$temporary_directory"' EXIT

    echo "Downloading micromamba for $platform..."
    curl --fail --location --silent --show-error \
        "https://micro.mamba.pm/api/micromamba/$platform/latest" \
        | tar -xj -C "$temporary_directory" bin/micromamba

    mkdir -p "$(dirname "$SAMPIC_MICROMAMBA")"
    install -m 0755 \
        "$temporary_directory/bin/micromamba" \
        "$SAMPIC_MICROMAMBA"
    rm -rf "$temporary_directory"
    trap - EXIT
    echo "Installed micromamba: $SAMPIC_MICROMAMBA"
}

bootstrap_micromamba
export MAMBA_ROOT_PREFIX="$SAMPIC_MAMBA_ROOT_PREFIX"

if [ "$RECREATE" = true ] && [ -d "$SAMPIC_ENV_PREFIX" ]; then
    echo "Removing managed environment: $SAMPIC_ENV_PREFIX"
    "$SAMPIC_MICROMAMBA" env remove \
        --yes \
        --name "$SAMPIC_ENV_NAME"
fi

if [ -d "$SAMPIC_ENV_PREFIX/conda-meta" ]; then
    echo "Updating environment '$SAMPIC_ENV_NAME' from $SAMPIC_ENV_FILE"
    "$SAMPIC_MICROMAMBA" env update \
        --yes \
        --name "$SAMPIC_ENV_NAME" \
        --file "$SAMPIC_ENV_FILE" \
        --prune
else
    echo "Creating environment '$SAMPIC_ENV_NAME' from $SAMPIC_ENV_FILE"
    "$SAMPIC_MICROMAMBA" create \
        --yes \
        --name "$SAMPIC_ENV_NAME" \
        --file "$SAMPIC_ENV_FILE"
fi

if [ "$RUN_CHECK" = true ]; then
    echo "Verifying the new environment..."
    SAMPIC_SKIP_ACTIVE_ENV_CHECK=1 \
        "$SAMPIC_MICROMAMBA" run \
        --name "$SAMPIC_ENV_NAME" \
        bash -c \
        "source '$SAMPIC_PROJECT_ROOT/scripts/setup_env.sh' &&
         '$SCRIPT_DIRECTORY/check_env.sh'"
fi

cat <<EOF

Environment ready. Activate it in the current shell with:

  source scripts/setup_env.sh

EOF
