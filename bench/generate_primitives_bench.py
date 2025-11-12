# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
# 
# This script generates the benchmark files for the primitives benchmarks.

import json
import argparse

parser = argparse.ArgumentParser(
        description='Generate benchmark files')

parser.add_argument('-p', '--party',
        type=str,
        required=False,
        default="a",
        dest='party')
parser.add_argument('-i1', '--peer_ip_1',
        type=str,
        required=False,
        default="10.3.32.4",
        dest='peer_ip_1')
parser.add_argument('-i2', '--peer_ip_2',
        type=str,
        required=False,
        default="10.3.33.4",
        dest='peer_ip_2')
parser.add_argument('-n', '--n_reps',
        type=int,
        required=False,
        default=1,
        dest='n_reps')
parser.add_argument('-o', '--outfile',
        type=str,
        required=False,
        default="./primitives_",
        dest='outfile')

args = parser.parse_args()
party = args.party
peer_ip_1 = args.peer_ip_1
peer_ip_2 = args.peer_ip_2
n_reps = args.n_reps
outfile = args.outfile

# for each dualex pipeline
peer_ip = [peer_ip_1, peer_ip_2]
port = [50000, 55000]
party_nr = {"a":1, "b":2}
for i in [1, 2]:
    for party in ["a", "b"]:
        val = {"description": "Primitives benchfile",
                "options": {
                "party": party_nr[party],
                "port": port[i-1],
                "peer_ip": peer_ip[i-1],
	    	    "outfile": outfile+party+str(i)+".csv",
	    	    "n_reps": n_reps
                }
             }
    
        with open('primitives_'+party+str(i)+'.json', 'w') as out:
            out.write(json.dumps(val, indent=4))
