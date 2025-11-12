#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# This script sets up the Intel machines for the project (relevant for the MPI-SWS machines).


# Install dependencies
apt -y install libguestfs-tools libosinfo-bin qemu-system virt-manager tigervnc-viewer qemu-kvm libvirt-daemon  bridge-utils virtinst libvirt-daemon-system

# Install emp-tool dependency
apt -y install xxd

# Start libvirt
systemctl status libvirtd.service
virsh net-start default

# Setup tap and check
modprobe vhost_net
lsmod | grep vhost

#Execute network script 
./setup_mellanoxhost_network.sh

# Copy VM files
cd /var/lib/libvirt/images
scp rdeviti@mellanox03:/DS/covault/nobackup/mellanox-debian-vms/jammy* . 
scp -r rdeviti@mellanox03:/DS/covault/nobackup/mellanox-debian-vms/ubuntu* .
cp ubuntu2204.xml /etc/libvirt/qemu/.

# Create VM
virsh define ubuntu2204.xml

# Start VM
virsh start ubuntu2204

echo "Done."