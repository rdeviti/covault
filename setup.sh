#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT

# Launch from CoVault's main folder
cd ..

# Get libhiredis from APT 
sudo apt install -y libhiredis-dev

# Install dependencies to run formatter
sudo apt install -y clang-format
sudo apt install -y python3-pip
pip3 install autopep8

# Install Boost dependencies (for amd vms)
sudo apt install -y libboost-all-dev

# Install dependencies for the plotting scripts
pip3 install pandas
pip3 install matplotlib
pip3 install numpy

# Install emp-toolkit
wget https://raw.githubusercontent.com/emp-toolkit/emp-readme/master/scripts/install.py
python3 install.py --install --tool --ot --sh2pc --ag2pc

# Copy custom emp files to emp toolkit
cp ./covault/third_party/emp_custom/halfgate_gen.h ./emp-tool/emp-tool/gc/.
cp ./covault/third_party/emp_custom/semihonest.h ./emp-sh2pc/emp-sh2pc/.
cp ./covault/third_party/emp_custom/sh_party.h ./emp-sh2pc/emp-sh2pc/.

# Install emp-tool
cd emp-tool
# Install version tested on this codebase
git checkout 6e75f6d03e622ca6a2a23ba0c1c82fdd93f2c733
# echo "set(CRYPTO_IN_CIRCUIT 1)" >> cmake/emp-base.cmake
# cmake .
cmake -DCRYPTO_IN_CIRCUIT=on .
make
sudo make install
cd ..

# Install emp-ag2pc
cd emp-ag2pc
# Install version tested on this codebase
git checkout a1cbca82d87a73c118ef861304726cb575c3b78d
cmake .
make
sudo make install
cd ..

# Install emp-sh2pc
cd emp-sh2pc
# Install version tested on this codebase
git checkout 61589f52111a26015b2bb8ab359dc457f8a246eb
cmake .
make
sudo make install
cd ..

# Install emp-ot
cd emp-ot
# Install version tested on this codebase
git checkout eb0daf2a7a88c44b419f6d1276dc19e66f80776f
cmake .
make
sudo make install
cd ..

# Install emp-agmpc
python3 install.py --deps --agmpc
cd emp-agmpc
# Install version tested on this codebase
git checkout 0add81ed517ac5b83d3a6576572b8daa0d236303
cmake .
make
sudo make install
cd ..

cd covault
# Disable frequency scaling (not relevant on GCE VMs)
# ./sbin/set_cpugov.sh

# Compile CoVault
mkdir build
cd build
sudo cp ../cmake/common.cmake /usr/local/cmake/.
cmake ..
make
echo "Install complete. Setting up Redis."

cd ..
# Setup Redis
./redis_setup.sh

