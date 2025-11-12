# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# This script generates the benchmark files for the mapper and reducer.

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
        default="10.3.32.3",
        dest='peer_ip_1')
parser.add_argument('-i2', '--peer_ip_2',
        type=str,
        required=False,
        default="10.3.33.3",
        dest='peer_ip_2')
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
parser.add_argument('-mo', '--mapper_outfile',
        type=str,
        required=False,
        default="./mapper_",
        dest='base_mapper_outfile')
parser.add_argument('-ro', '--reducer_outfile',
        type=str,
        required=False,
        default="./reducer_",
        dest='base_reducer_outfile')

args = parser.parse_args()
party = args.party
peer_ip_1 = args.peer_ip_1
peer_ip_2 = args.peer_ip_2
tile_size = args.tile_size
tile_end = args.tile_end
n_reps = args.n_reps
base_mapper_outfile = args.base_mapper_outfile
base_reducer_outfile = args.base_reducer_outfile

peer_ip = [peer_ip_1, peer_ip_2]
localhost = "127.0.0.1"
mapper_port = [50000, 55000]
reducer_port = [50500, 55500]
mapreduce_port = [60000, 65000]
redis_port = [6379, 6380]
tile_start = 0
party_nr = {"a":1, "b":2}

# for each dualex pipeline
for i in [1, 2]:
    for party in ["a", "b"]:

        # mapper
        val = {"description": "Mapper benchfile",
                "options": {
                "party": party_nr[party],
                "peer_ip": peer_ip[i-1],
                "port": mapper_port[i-1],
	        "reducer_port": mapreduce_port[i-1],
	        "redis_ip": localhost,
	        "redis_port": redis_port[i-1],
    	        "tile_start": tile_start,
	    	"tile_end": tile_end,
	    	"tile_size": tile_size,
	    	"n_reps": n_reps,
	    	"reducer_ip": localhost,
	    	"outfile": base_mapper_outfile+party+str(i)+".csv",
                }
            }
    
        with open('mapper_'+party+str(i)+'.json', 'w') as out:
            out.write(json.dumps(val, indent=4))

        # reducer
        val = {"description": "Reducer benchfile",
               "options": {
                "party": party_nr[party],
                "peer_ip": peer_ip[i-1],
                "port": reducer_port[i-1],
	        "reducer_port": mapreduce_port[i-1],
		"tile_start": tile_start,
		"tile_end": tile_end,
		"tile_size": tile_size,
	    	"n_reps": n_reps,
		"mapper_ip": localhost,
		"outfile": base_reducer_outfile+party+str(i)+".csv"
                }
            }
    
        with open('reducer_'+party+str(i)+'.json', 'w') as out:
            out.write(json.dumps(val, indent=4))

