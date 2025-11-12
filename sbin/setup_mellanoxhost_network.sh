#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# This script sets up the Intel machines for the project (relevant for the MPI-SWS machines).

# Put the interfaces up
ip link set dev ens1f0np0 up
ip link set dev ens1f1np1 up

# Use jumbo frames on both interfaces
ifconfig ens1f0np0 mtu 9100
ifconfig ens1f1np1 mtu 9100

# Create a bridge
ip link add br0 type bridge
# Flush IP on the interface to bridge
ip addr flush dev ens1f0np0
# Add the interface to the bridge
ip link set ens1f0np0 master br0
ip link set dev br0 up

# Repeat the procedure for the second interface
ip link add br1 type bridge
ip addr flush dev ens1f1np1
ip link set ens1f1np1 master br1
ip link set dev br1 up

# Setup IP address and MTU size on the bridges
ifconfig br0 10.3.32.2 netmask 255.255.255.0 broadcast 10.3.32.255 mtu 9100
ifconfig br1 10.3.33.2 netmask 255.255.255.0 broadcast 10.3.33.255 mtu 9100
