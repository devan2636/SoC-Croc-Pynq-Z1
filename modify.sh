#!/bin/bash

# Move PYNQ-Z1 related implementation and other changes neeed
# to the croc directories

rsync -av \
    --exclude='modify.sh' \
    --exclude='README.md' \
    --exclude='bitstreams/' \
    --exclude='.git/' \
    --include='sw/' \
    --include='xilinx/' \
    ./ ../croc/