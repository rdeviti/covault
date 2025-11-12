#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT

# Install Redis 
sudo apt install -y redis-server
sudo apt install -y redis-tools

# Set up CoVault's Redis configuration with two instances (ports 6379 and 6380)
sudo cp ./third_party/redis/redis.conf /etc/redis/.
sudo cp ./third_party/redis/redis_6380.conf /etc/redis/.
sudo cp ~/covault/third_party/redis/redis-server_6380.service /lib/systemd/system/.

# Link second instance to systemd
cd /etc/systemd/system
sudo ln -s /lib/systemd/system/redis-server_6380.service redis_6380.service
ls -l

cd ~/covault

# Restart Redis
sudo systemctl restart redis
sudo systemctl start redis_6380

echo ""
echo "If systemctl start redis_6380 did not work, try running:"
echo "sudo /usr/bin/redis-server /etc/redis/redis_6380.conf"
echo "To check if you can connect, run:"
echo "redis-cli -h 127.0.0.1 -p 6380"
echo "redis-cli -h 127.0.0.1"
echo "Check status with:"
echo "systemctl status redis"
