# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
# 
# This script generates the benchmark files for the query benchmarks.

import json
import argparse
import socket

parser = argparse.ArgumentParser(
        description='Generate benchmark files: node')

parser.add_argument('-p', '--party',
        type=str,
        required=False,
        default="a",
        dest='input_party')
parser.add_argument('-i1', '--peer_ip_1',
        type=str,
        required=False,
        default="10.3.32.3",
        dest='peer_ip_1')
parser.add_argument('-i2', '--peer_ip_2',
        type=str,
        required=False,
        default="10.3.33.3",
        dest='peer_ip_2')
parser.add_argument('-r2', '--reducer_ip_2',
        type=str,
        required=False,
        default="10.3.32.3",
        dest='reducer_ip_2')
parser.add_argument('-r3', '--reducer_ip_3',
        type=str,
        required=False,
        default="10.3.33.3",
        dest='reducer_ip_3')
parser.add_argument('-np', '--n_processes',
        type=int,
        required=False,
        default=2,
        dest='n_processes')
parser.add_argument('-s', '--tile_size',
        type=int,
        required=False,
        default=100,
        dest='tile_size')
parser.add_argument('-e', '--tile_end',
        type=int,
        required=False,
        default=1,
        dest='tile_end')
parser.add_argument('-n', '--n_reps',
        type=int,
        required=False,
        default=1,
        dest='n_reps')
parser.add_argument('-o', '--node_outfile',
        type=str,
        required=False,
        default="./node_",
        dest='base_node_outfile')

args = parser.parse_args()
input_party = args.input_party
peer_ip_1 = args.peer_ip_1
peer_ip_2 = args.peer_ip_2
reducer_ip_2 = args.reducer_ip_2
reducer_ip_3 = args.reducer_ip_3
n_processes = args.n_processes
tile_size = args.tile_size
tile_end = args.tile_end
n_reps = args.n_reps
base_node_outfile = args.base_node_outfile

this_ip = socket.gethostbyname(socket.gethostname())
tile_start = 0
localhost = "127.0.0.1"
redis_port = [6379, 6380]
party_nr = {"a":1, "b":2}

# for each dualex pipeline
for i in range(1, n_processes+1):
    for party in ["a", "b"]:
        
        if i % 2 == 0:
             peer_ip = peer_ip_2
        else:
             peer_ip = peer_ip_1

        # main pipeline
        if input_party == party:
            base_port = 50000
            base_reducer_port = 50100
        # dualex pipeline
        else:
            base_port = 55000    
            base_reducer_port = 55100
    
        val = {"description": "Node benchfile",
                "options": {
                    "party": party_nr[party],
                    "peer_ip": peer_ip,
                    "port": base_port + (i-1),
	                "reducer_port": base_reducer_port + (i-1),
	                "redis_ip": localhost,
	                "redis_port": redis_port[party_nr[party]-1],
	 	            "tile_start": tile_start,
		            "tile_end": tile_end,
		            "tile_size": tile_size,
		            "n_reps": n_reps,
		            "reducer_ip_1": this_ip,
		            "reducer_ip_2": reducer_ip_2,
	  	            "reducer_ip_3": reducer_ip_3,
		            "outfile": base_node_outfile+party+str(i)+".csv",
                    "id": i
                    }
            }

        with open('node_'+party+str(i)+'.json', 'w') as out:
            out.write(json.dumps(val, indent=4))
