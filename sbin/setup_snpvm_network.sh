#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# This script sets up the SNP VMs machines for the project (relevant for the MPI-SWS machines).

# Setup direct connection to Mellanox machines
ifconfig enp0s2 10.3.32.4 netmask 255.255.255.0 mtu 9100
ifconfig enp0s3 10.3.33.4 netmask 255.255.255.0 mtu 9100

# Check config
ifconfig
