#!/bin/bash

set -e
cd $(dirname "${BASH_SOURCE[0]}")
./geosbuild.sh
cc -O3 -Ilibgeos/build/install/include -o relate relate.c \
     libgeos/build/install/lib/libgeos_c.a \
     libgeos/build/install/lib/libgeos.a -lm -pthread -lstdc++
./relate "$1" "$2"
