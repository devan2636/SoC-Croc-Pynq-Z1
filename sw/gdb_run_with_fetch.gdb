set confirm off
set pagination off

target extended-remote localhost:3335
monitor reset halt
load

# Ensure boot address points to SRAM program area.
set {unsigned int}0x03000000 = 0x10000000
# Force fetch enable.
set {unsigned int}0x03000004 = 0x00000001

x/wx 0x03000000
x/wx 0x03000004

set $pc = 0x10000000
continue
