#!/usr/bin/env zsh
#!/usr/bin/env bash
#!/bin/env bash

clang-format -i -style=file include/*
clang-format -i -style=file include/utils/*
clang-format -i -style=file include/macs/*
clang-format -i -style=file src/*
clang-format -i -style=file test/*.cpp

