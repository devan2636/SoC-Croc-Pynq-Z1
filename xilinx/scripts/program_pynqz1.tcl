# Program a PYNQ-Z1 bitstream to hardware via Vivado hardware manager.
# Usage:
#   vivado -mode batch -source scripts/program_pynqz1.tcl -tclargs <bitfile> [hw_server_url] [hw_device]
# Example:
#   vivado -mode batch -source scripts/program_pynqz1.tcl -tclargs out/croc.pynqz1.bit localhost:3121 xc7z020_1

set bitfile [lindex $argv 0]
if { $bitfile eq "" } {
    puts "ERROR: Missing bitfile argument."
    puts "Usage: -tclargs <bitfile> [hw_server_url] [hw_device]"
    exit 2
}

set hw_url [lindex $argv 1]
if { $hw_url eq "" } {
    set hw_url "localhost:3121"
}

set req_dev [lindex $argv 2]
if { $req_dev eq "" } {
    set req_dev "xc7z020_1"
}

if { ![file exists $bitfile] } {
    puts "ERROR: Bitfile does not exist: $bitfile"
    exit 3
}

set abs_bitfile [file normalize $bitfile]
puts "INFO: Bitfile = $abs_bitfile"
puts "INFO: HW server = $hw_url"
puts "INFO: Requested device = $req_dev"

open_hw_manager
connect_hw_server -url $hw_url
open_hw_target

set devs [get_hw_devices]
if { [llength $devs] == 0 } {
    puts "ERROR: No hardware devices found."
    close_hw_target
    disconnect_hw_server
    close_hw_manager
    exit 4
}

set target_dev [get_hw_devices $req_dev]
if { [llength $target_dev] == 0 } {
    puts "WARNING: Requested device '$req_dev' not found. Using first detected device."
    set target_dev [lindex $devs 0]
}

current_hw_device $target_dev
refresh_hw_device $target_dev
set_property PROGRAM.FILE $abs_bitfile $target_dev
program_hw_devices $target_dev
refresh_hw_device $target_dev

puts "INFO: Programming complete on [get_property NAME $target_dev]"

close_hw_target
disconnect_hw_server
close_hw_manager
exit 0
