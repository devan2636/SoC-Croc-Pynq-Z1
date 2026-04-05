# Copyright 2024 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# OpenOCD script for Cheshire on PYNQ-Z1 (legacy J-Link profile).

adapter speed 100
adapter driver jlink
set irlen 5

transport select jtag
reset_config none
gdb_port 3335
telnet_port 4446
tcl_port 6667

set _CHIPNAME riscv
jtag newtap $_CHIPNAME cpu -irlen ${irlen} -expected-id 0x0c0c5db3

set _TARGETNAME $_CHIPNAME.cpu
target create $_TARGETNAME riscv -chain-position $_TARGETNAME -coreid 0

gdb_report_data_abort enable
gdb_report_register_access_error enable

riscv set_reset_timeout_sec 120
riscv set_command_timeout_sec 120

riscv set_mem_access abstract sysbus progbuf

# Try enabling address translation (only works for newer versions)
if { [catch { riscv set_enable_virtual on } ] } {
    echo "Warning: This version of OpenOCD does not support address translation.\
        To debug on virtual addresses, please update to the latest version."
}

init
if { [catch { halt } ] } {
    echo "Warning: initial halt failed; target may still be in reset/ungated clock"
}
echo "Ready for Remote Connections on Port 3335"
