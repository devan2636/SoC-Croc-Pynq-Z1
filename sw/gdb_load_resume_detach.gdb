set confirm off
set pagination off
set remotetimeout 120
target extended-remote localhost:3335
monitor halt
load
set {unsigned int}0x03000000 = 0x10000000
set {unsigned int}0x03000004 = 0x00000001
set $pc = 0x10000000
monitor resume
detach
quit
