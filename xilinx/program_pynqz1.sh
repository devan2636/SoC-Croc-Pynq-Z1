#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Try to make Vivado available in PATH.
if ! command -v vivado >/dev/null 2>&1; then
    if [ -n "$VIVADO_SETTINGS" ] && [ -f "$VIVADO_SETTINGS" ]; then
        # shellcheck disable=SC1090
        source "$VIVADO_SETTINGS"
    elif ls "$HOME"/Xilinx/*/Vivado/settings64.sh >/dev/null 2>&1; then
        LATEST_SETTINGS="$(ls -1d "$HOME"/Xilinx/*/Vivado/settings64.sh | sort -V | tail -n 1)"
        # shellcheck disable=SC1090
        source "$LATEST_SETTINGS"
    elif ls /opt/Xilinx/Vivado/*/settings64.sh >/dev/null 2>&1; then
        LATEST_SETTINGS="$(ls -1d /opt/Xilinx/Vivado/*/settings64.sh | sort -V | tail -n 1)"
        # shellcheck disable=SC1090
        source "$LATEST_SETTINGS"
    fi
fi

if ! command -v vivado >/dev/null 2>&1; then
    echo "Error: vivado not found in PATH."
    echo "Set Vivado environment first, e.g.:"
    echo "  source /opt/Xilinx/Vivado/<version>/settings64.sh"
    echo "or run with:"
    echo "  VIVADO_SETTINGS=/opt/Xilinx/Vivado/<version>/settings64.sh ./program_pynqz1.sh"
    exit 1
fi

VIVADO_BIN="${VIVADO_BIN:-vivado}"
BITFILE="${1:-out/croc.pynqz1.bit}"
HW_URL="${HW_URL:-localhost:3121}"
HW_DEVICE="${HW_DEVICE:-xc7z020_1}"

if [ ! -f "$BITFILE" ]; then
    echo "Error: bitfile not found: $BITFILE"
    exit 1
fi

echo "Programming FPGA with: $BITFILE"
echo "HW server: $HW_URL"
echo "HW device: $HW_DEVICE"

"$VIVADO_BIN" -mode batch \
    -source scripts/program_pynqz1.tcl \
    -tclargs "$BITFILE" "$HW_URL" "$HW_DEVICE"

echo "Done programming FPGA."
