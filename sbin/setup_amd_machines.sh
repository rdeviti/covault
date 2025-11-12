#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# This script sets up the AMD machines for the project (relevant for the MPI-SWS machines).

# Install requirements
apt install -y iasl libelf-dev libslirp-dev

# Install qemu (then ignore qemu error in AMDSEV install script)
apt install -y qemu-utils qemu-system-x86 qemu-system-gui

# Get AMDSEV 
git clone https://github.com/AMDESE/AMDSEV.git
cd AMDSEV
git checkout snp-latest

# Build host and guest kernels
./build.sh --package
cp kvm.conf /etc/modprobe.d/

# Install host kernel
cd snp-release-$(date -I)
./install.sh

# Update grub 
# (disable.cfg relevant for mpi-sws machines)
mv /etc/default/grub.d/disable.cfg  /etc/default/grub.d/disable.cfg_BAK
update-grub
