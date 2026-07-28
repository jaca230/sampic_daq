#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
REPO_ROOT=$(realpath "$SCRIPT_DIR/../..")
VENDOR_ROOT="$REPO_ROOT/external/sampic_256ch_lib"
EXAMPLE_DIR="$VENDOR_ROOT/example"
FTDI_DIR="$VENDOR_ROOT/lpdevc_install"
LPDEVC_LIB_DIR="$FTDI_DIR/lib"
SAMPIC_LIB_DIR="$VENDOR_ROOT/lib"

if [[ ! -d "$EXAMPLE_DIR" ]]; then
  echo "[build_vendor_sampic_test.sh, ERROR] Missing vendor example directory: $EXAMPLE_DIR" >&2
  exit 1
fi

if [[ ! -e "$FTDI_DIR/libftd2xx.so" ]]; then
  newest_ftdi=$(ls "$FTDI_DIR"/libftd2xx.so.* 2>/dev/null | sort -V | tail -n 1 || true)
  if [[ -z "$newest_ftdi" ]]; then
    echo "[build_vendor_sampic_test.sh, ERROR] Missing libftd2xx.so under: $FTDI_DIR" >&2
    exit 1
  fi
  ln -s "$(basename "$newest_ftdi")" "$FTDI_DIR/libftd2xx.so"
  echo "[build_vendor_sampic_test.sh] Created libftd2xx.so -> $(basename "$newest_ftdi")"
fi

make -C "$EXAMPLE_DIR" clean all \
  CFLAGS="-I$VENDOR_ROOT/include_lib -I$FTDI_DIR/include -m64 -fPIC -Wall" \
  LFLAGS="-L$SAMPIC_LIB_DIR -L$LPDEVC_LIB_DIR -L$FTDI_DIR -lsampic256ch -llpdevC -llpdev -lftd2xx -lrt -lpthread -Wl,--disable-new-dtags -Wl,-rpath,$SAMPIC_LIB_DIR -Wl,-rpath,$LPDEVC_LIB_DIR -Wl,-rpath,$FTDI_DIR"

echo
echo "[build_vendor_sampic_test.sh] Built: $EXAMPLE_DIR/sampic_test"
echo "[build_vendor_sampic_test.sh] Run:"
echo "  $EXAMPLE_DIR/sampic_test"
