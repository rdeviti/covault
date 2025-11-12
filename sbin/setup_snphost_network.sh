#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# This script sets up the AMD machines for the project (relevant for the MPI-SWS machines).

# Put the interfaces up
ip link set dev eno12399np0 up
ip link set dev eno12409np1 up

# Use jumbo frames on both interfaces
ifconfig eno12399np0 mtu 9100
ifconfig eno12409np1 mtu 9100

# Create a bridge
ip link add br0 type bridge
# Flush IP on the interface to bridge
ip addr flush dev eno12399np0
# Add the interface to the bridge
ip link set eno12399np0 master br0
# Create a new TAP interface
ip tuntap add dev tap0 mode tap user $(whoami)
# Make sure everything is up
ip link set tap0 master br0
ip link set dev br0 up
ip link set dev tap0 up

# Repeat the procedure for the second interface
ip link add br1 type bridge
ip addr flush dev eno12409np1
ip link set eno12409np1 master br1
ip tuntap add dev tap1 mode tap user $(whoami)
ip link set tap1 master br1
ip link set dev br1 up
ip link set dev tap1 up

# Setup IP address and MTU size on the bridges
ifconfig br0 10.3.32.1 netmask 255.255.255.0 broadcast 10.3.32.255 mtu 9100
ifconfig br1 10.3.33.1 netmask 255.255.255.0 broadcast 10.3.33.255 mtu 9100

# Setup jumbo frames on TAP interfaces
ifconfig tap0 mtu 9100
ifconfig tap1 mtu 9100
