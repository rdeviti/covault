#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# This script sets up the Debian VMs for the project (relevant for the MPI-SWS machines).

# Setup direct connection to Snp machines
ifconfig enp2s0 10.3.32.3 netmask 255.255.255.0 mtu 9100
ifconfig enp3s0 10.3.33.3 netmask 255.255.255.0 mtu 9100

# Setup default (virbr0) connection to get Internet connectivity
dhclient -r enp1s0
dhclient enp1s0

# Check output
ifconfig
