#!/usr/bin/env sh

autopep8 --aggressive --aggressive --in-place $(find . | grep "\.py$")
