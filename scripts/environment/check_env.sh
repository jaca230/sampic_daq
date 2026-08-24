#!/bin/bash

set -e

SCRIPT_DIRECTORY="$(
    cd -P "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd
)"
# shellcheck disable=SC1091
source "$SCRIPT_DIRECTORY/environment_paths.sh"

failures=0

check_command() {
    local command_name="$1"
    if command -v "$command_name" >/dev/null 2>&1; then
        printf '[OK]   %-14s %s\n' \
            "$command_name" "$(command -v "$command_name")"
    else
        printf '[FAIL] %-14s not found\n' "$command_name"
        failures=$((failures + 1))
    fi
}

check_directory() {
    local label="$1"
    local directory="$2"
    if [ -d "$directory" ]; then
        printf '[OK]   %-14s %s\n' "$label" "$directory"
    else
        printf '[FAIL] %-14s missing: %s\n' "$label" "$directory"
        failures=$((failures + 1))
    fi
}

check_file() {
    local label="$1"
    local path="$2"
    if [ -f "$path" ]; then
        printf '[OK]   %-14s %s\n' "$label" "$path"
    else
        printf '[FAIL] %-14s missing: %s\n' "$label" "$path"
        failures=$((failures + 1))
    fi
}

if [ "${SAMPIC_SKIP_ACTIVE_ENV_CHECK:-0}" != "1" ] &&
   [ "${CONDA_PREFIX:-}" != "$SAMPIC_ENV_PREFIX" ]; then
    echo "[FAIL] active environment is not $SAMPIC_ENV_NAME"
    echo "       Run: source scripts/setup_env.sh"
    failures=$((failures + 1))
else
    echo "[OK]   environment    ${CONDA_PREFIX:-$SAMPIC_ENV_PREFIX}"
fi

for command_name in python jupyter cmake ninja make root root-config "${CC:-cc}" "${CXX:-c++}"; do
    check_command "$command_name"
done

if command -v root-config >/dev/null 2>&1; then
    echo "[OK]   ROOT version   $(root-config --version)"
fi

if [ -n "${CONDA_PREFIX:-}" ]; then
    root_cmake_config="$(
        find "$CONDA_PREFIX" -type f -name ROOTConfig.cmake -print -quit \
            2>/dev/null
    )"
    if [ -n "$root_cmake_config" ]; then
        printf '[OK]   %-14s %s\n' "ROOT CMake" "$root_cmake_config"
    else
        echo "[FAIL] ROOTConfig.cmake not found below $CONDA_PREFIX"
        failures=$((failures + 1))
    fi

    tbb_cmake_config="$(
        find "$CONDA_PREFIX" -type f -name TBBConfig.cmake -print -quit \
            2>/dev/null
    )"
    if [ -n "$tbb_cmake_config" ]; then
        printf '[OK]   %-14s %s\n' "TBB CMake" "$tbb_cmake_config"
    else
        echo "[FAIL] TBBConfig.cmake not found below $CONDA_PREFIX"
        failures=$((failures + 1))
    fi
fi

if command -v python >/dev/null 2>&1; then
    if python -c \
        'import ROOT; print(f"[OK]   PyROOT         {ROOT.__version__}")'
    then
        :
    else
        echo "[FAIL] PyROOT import failed"
        failures=$((failures + 1))
    fi
    if python -c \
        'import pandas, matplotlib; print("[OK]   notebooks      pandas + matplotlib")'
    then
        :
    else
        echo "[FAIL] notebook Python imports failed"
        failures=$((failures + 1))
    fi
fi

check_directory "MIDASSYS" "${MIDASSYS:-}"
check_file "libmidas" "${MIDASSYS:+$MIDASSYS/lib/libmidas.a}"
check_file "libmfe" "${MIDASSYS:+$MIDASSYS/lib/libmfe.a}"
check_file "MIDAS exptab" "${MIDAS_EXPTAB:-}"
check_directory "SAMPIC root" "${SAMPIC_ROOT:-}"
check_directory "SAMPIC libs" "${SAMPIC_ROOT:+$SAMPIC_ROOT/lib}"
check_directory \
    "lpdevc libs" "${SAMPIC_ROOT:+$SAMPIC_ROOT/lpdevc_install/lib}"

if [ "$failures" -ne 0 ]; then
    echo
    echo "Environment check failed with $failures problem(s)."
    exit 1
fi

echo
echo "Environment check passed."
