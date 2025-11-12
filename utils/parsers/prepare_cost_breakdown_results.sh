#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# This script prepares the cost breakdown results for the paper.
# This is relevant for the GCE setup. for other setups, the paths must be changed.

out=~/covault/results/cost_breakdown
mkdir -p $out

# dualex with tees
cp ~/covault/results/primitives/primitives_a1.csv $out/dualex_tees_1.csv
cp ~/covault/results/primitives/primitives_a2.csv $out/dualex_tees_2.csv
python3 ~/covault/utils/parsers/sh2pc_parser.py $out/dualex_tees_1.csv $out/dualex_tees_1_
python3 ~/covault/utils/parsers/sh2pc_parser.py $out/dualex_tees_2.csv $out/dualex_tees_2_

# semi-honest with tees
cp ~/covault/results/primitives/primitives_nodualex_a1.csv $out/sh_snp_gen.csv
python3 ~/covault/utils/parsers/sh2pc_parser.py $out/sh_snp_gen.csv $out/sh_snp_gen_

# semi-honest gcs
scp intel01:~/covault/results/primitives/primitives_nodualex_a1.csv $out/sh_intel_gen.csv
scp amd01:~/covault/results/primitives/primitives_nodualex_a1.csv $out/sh_amd_gen.csv
python3 ~/covault/utils/parsers/sh2pc_parser.py $out/sh_intel_gen.csv $out/sh_intel_gen_
python3 ~/covault/utils/parsers/sh2pc_parser.py $out/sh_amd_gen.csv $out/sh_amd_gen_

# ag2pc
python3 ~/covault/utils/parsers/ag2pc_parser.py ~/covault/results/ag2pc_logs/ag2pc_benchmark_1.txt
cp ~/covault/results/ag2pc_logs/ag2pc_benchmark_1_time.csv $out/ag2pc_sort10000_time.csv
