#!/bin/bash

# Arguments
GDB_BIN_PATH="$1"
TARGET="$2"

# Check arguments
if [ -z "$TARGET" ] || [ -z "$GDB_BIN_PATH" ]; then
    echo "Usage: $0 <gdb_bin_path> <program>"
    exit 1
fi

# Full path to GDB executable
GDB_EXEC="$GDB_BIN_PATH/riscv32-unknown-elf-gdb"
OCD_GDB_PORT="${OCD_GDB_PORT:-3335}"

# Validate GDB exists
if [ ! -x "$GDB_EXEC" ]; then
    echo "Error: GDB not found or not executable at $GDB_EXEC"
    exit 1
fi

# Run GDB
"$GDB_EXEC" \
    --ex "target remote localhost:$OCD_GDB_PORT" \
    --ex "monitor halt" \
    --ex "load" \
    --ex "set $pc = _start" \
    --ex "continue" \
    "$TARGET"
