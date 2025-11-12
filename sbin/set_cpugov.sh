#!/usr/bin/env zsh
#!/usr/bin/env bash
#!/bin/env zsh
#!/bin/env bash

# needs root access
# sets the specified gov to all the cores
# (gov = performance disables frequency scaling)

apt install cpufrequtils
apt install linux-cpupower
echo "Before (frequency scaling may be enabled): "
cpufreq-info | grep "current CPU"
cpufreq-set -g performance
echo "After (frequency scaling disabled): "
cpufreq-info | grep "current CPU"