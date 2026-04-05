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
    echo "  VIVADO_SETTINGS=/opt/Xilinx/Vivado/<version>/settings64.sh ./implement_pynqz1.sh"
    exit 1
fi

VIVADO_BIN="${VIVADO_BIN:-vivado}"

# bender script vivado -t fpga -t rtl -t pynqz1 > scripts/add_sources.pynqz1.tcl
mkdir -p build/pynqz1.clkwiz
cd build/pynqz1.clkwiz && \
    "$VIVADO_BIN" -mode batch -log ../pynqz1.clkwiz.log -jou ../pynqz1.clkwiz.jou \
    -source ../../scripts/impl_ip_pynqz1.tcl \
    -tclargs pynqz1 clkwiz \
    && cd ../..
mkdir -p build/pynqz1.vio
cd build/pynqz1.vio &&
    "$VIVADO_BIN" -mode batch -log ../pynqz1.vio.log -jou ../pynqz1.vio.jou \
    -source ../../scripts/impl_ip_pynqz1.tcl \
    -tclargs pynqz1 vio\
    && cd ../..
mkdir -p build/pynqz1.croc
cd build/pynqz1.croc && \
    "$VIVADO_BIN" -mode batch -log ../croc.pynqz1.log -jou ../croc.pynqz1.jou \
    -source ../../scripts/impl_sys.tcl \
    -tclargs pynqz1 croc \
    ../pynqz1.clkwiz/out.xci \
    ../pynqz1.vio/out.xci
