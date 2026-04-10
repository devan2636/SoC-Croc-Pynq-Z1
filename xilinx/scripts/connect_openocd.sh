#!/bin/bash

# Resolve this script's directory so OpenOCD can be started from anywhere.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Default configuration
# Prefer the OpenOCD 0.12-compatible profile for better attach stability.
CONFIG="openocd2.jlink.tcl"
GDB_PORT="3335"

# Check user input
case "$1" in
    ""|jlink|jlink2|jlink-compat)
        CONFIG="openocd2.jlink.tcl"
        ;;
    jlink-legacy)
        CONFIG="openocd.jlink.tcl"
        ;;
    genesys2)
        CONFIG="openocd.genesys2.tcl"
        ;;
    *)
        echo "Unknown option: $1"
        echo "Usage: $0 [jlink|jlink-legacy|genesys2]"
        echo "  jlink        : use J-Link JTAG with OpenOCD 0.12-compatible profile (default)"
        echo "  jlink-legacy : use legacy J-Link profile"
        echo "  genesys2 : use Digilent HS-2 (requires openocd.genesys2.tcl)"
        exit 1
        ;;
esac

if [ ! -f "$SCRIPT_DIR/$CONFIG" ]; then
    echo "Error: OpenOCD config not found: $SCRIPT_DIR/$CONFIG"
    exit 1
fi

# Run OpenOCD with the chosen configuration
echo "Running OpenOCD with $CONFIG"
if [ -n "$GDB_PORT" ]; then
    echo "GDB port: $GDB_PORT"
fi

# Avoid port conflicts from stale OpenOCD sessions.
DEBUG_PORTS='(3333|3335|4444|4446|6666|6667)'
PORT_PIDS="$(ss -ltnp | sed -n 's/.*pid=\([0-9][0-9]*\).*/\1/p' | sort -u)"
if [ -n "$PORT_PIDS" ]; then
    for pid in $PORT_PIDS; do
        if ps -p "$pid" -o comm= 2>/dev/null | grep -q '^openocd$'; then
            if ss -ltnp | grep -E "(127\.0\.0\.1|0\.0\.0\.0):$DEBUG_PORTS" | grep -q "pid=$pid"; then
                echo "Stopping stale OpenOCD on debug ports: $pid"
                kill "$pid" 2>/dev/null || true
            fi
        fi
    done

    # Wait briefly for sockets to close; force-kill if still alive.
    for _ in 1 2 3 4 5; do
        if ! ss -ltnp | grep -E "(127\.0\.0\.1|0\.0\.0\.0):$DEBUG_PORTS" | grep -q openocd; then
            break
        fi
        sleep 1
    done

    REMAINING_PIDS="$(ss -ltnp | sed -n 's/.*pid=\([0-9][0-9]*\).*/\1/p' | sort -u)"
    if [ -n "$REMAINING_PIDS" ]; then
        for pid in $REMAINING_PIDS; do
            if ps -p "$pid" -o comm= 2>/dev/null | grep -q '^openocd$'; then
                if ss -ltnp | grep -E "(127\.0\.0\.1|0\.0\.0\.0):$DEBUG_PORTS" | grep -q "pid=$pid"; then
                    echo "Force-stopping stuck OpenOCD: $pid"
                    kill -9 "$pid" 2>/dev/null || true
                fi
            fi
        done
    fi
fi

openocd -f "$SCRIPT_DIR/$CONFIG"