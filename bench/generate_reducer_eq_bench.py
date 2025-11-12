# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
# 
# This script generates the benchmark files for the reducer equality check.

import json
import argparse

parser = argparse.ArgumentParser(
        description='Generate benchmark files')

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
parser.add_argument('-ro', '--reducer_outfile',
        type=str,
        required=False,
        default="./reducer_eq_",
        dest='base_reducer_outfile')

args = parser.parse_args()
peer_ip_1 = args.peer_ip_1
peer_ip_2 = args.peer_ip_2
tile_size = args.tile_size
base_reducer_outfile = args.base_reducer_outfile

party_nr = {"a":1, "b":2}

for party in ["a", "b"]:
	val = {"description": "Reducer Equality Check benchfile",
		"options": {
		"party": party_nr[party],
		"peer_ip_1": peer_ip_1,
		"peer_ip_2": peer_ip_2,
		"port": 50000,
		"tile_size": tile_size,
                "outfile": base_reducer_outfile+party+".csv"
		}
    	}

	with open('reducer_eq_'+party+'.json', 'w') as out:
    		out.write(json.dumps(val, indent=4))
