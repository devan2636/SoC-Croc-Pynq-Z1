#!/bin/bash

# bender script vivado -t fpga -t rtl -t zcu104 > scripts/add_sources.zcu104.tcl
mkdir -p build/zcu104.clkwiz
cd build/zcu104.clkwiz && \
    vivado -mode batch -log ../zcu104.clkwiz.log -jou ../zcu104.clkwiz.jou \
    -source ../../scripts/impl_ip_zcu104.tcl \
    -tclargs zcu104 clkwiz \
    && cd ../..
mkdir -p build/zcu104.vio
cd build/zcu104.vio &&
    vivado -mode batch -log ../zcu104.vio.log -jou ../zcu104.vio.jou \
    -source ../../scripts/impl_ip_zcu104.tcl \
    -tclargs zcu104 vio\
    && cd ../..
mkdir -p build/zcu104.croc
cd build/zcu104.croc && \
    vivado -mode batch -log ../croc.zcu104.log -jou ../croc.zcu104.jou \
    -source ../../scripts/impl_sys.tcl \
    -tclargs zcu104 croc \
    ../zcu104.clkwiz/out.xci \
    ../zcu104.vio/out.xci
