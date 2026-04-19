# Copyright 2024 ETH Zurich and University of Bologna.
# OpenOCD script for Cheshire on PYNQ-Z1 (V0.12.0 Compatible)

adapter speed 50
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
# Create target without deferred examination to keep GDB attach/load stable.
target create $_TARGETNAME riscv -chain-position $_TARGETNAME -coreid 0

gdb_report_data_abort enable
gdb_report_register_access_error enable

riscv set_reset_timeout_sec 120
riscv set_command_timeout_sec 120
riscv set_mem_access sysbus progbuf abstract

init

echo "Attempting to manually examine and halt the hart..."
set retry 0
while { $retry < 10 } {
    if { [catch { $_TARGETNAME arp_examine } ] } {
        echo "Examination failed, retrying ($retry)..."
    } elseif { [catch { halt } ] } {
        echo "Halt failed, retrying ($retry)..."
    } else {
        set retry 10
    }
    if { $retry < 10 } {
        sleep 1000
    }
    incr retry
}

echo "-------------------------------------------"
echo "Ready for Remote Connections on Port 3335"
echo "-------------------------------------------"