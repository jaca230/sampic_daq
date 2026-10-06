#!/bin/bash

# Source this script with no options for the minimal runtime environment, or
# with --dev for the reproducible micromamba developer toolchain.
if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    echo "ERROR: source this script instead of executing it:" >&2
    echo "  source scripts/setup_env.sh [--dev]" >&2
    exit 1
fi

SAMPIC_SETUP_SCRIPT_DIRECTORY="$(
    cd -P "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd
)"

case "${1:-}" in
    "")
        # shellcheck disable=SC1091
        source "$SAMPIC_SETUP_SCRIPT_DIRECTORY/environment/activate_runtime.sh"
        ;;
    --dev)
        # shellcheck disable=SC1091
        source "$SAMPIC_SETUP_SCRIPT_DIRECTORY/environment/activate_dev.sh"
        ;;
    -h|--help)
        cat <<'EOF'
Usage: source scripts/setup_env.sh [--dev]

  no option  Configure the minimal environment needed to run the built DAQ.
             This mode never downloads or installs packages.
  --dev      Activate the developer environment. On first use, bootstrap
             micromamba and create the project-local environment.
EOF
        ;;
    *)
        echo "ERROR: unknown setup option '$1'" >&2
        echo "Usage: source scripts/setup_env.sh [--dev]" >&2
        return 2
        ;;
esac
