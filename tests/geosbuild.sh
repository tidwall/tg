#!/bin/bash

# ./geosbuild.sh [--rebuild]

GEOS_VERS=3.12.0

set -e
cd $(dirname "${BASH_SOURCE[0]}")

if [[ ! -f libgeos/build/install/lib/libgeos_c.a ]]; then
    if [[ ! -d libgeos ]]; then
        git clone https://github.com/tidwall/libgeos_static libgeos
    fi
    libgeos/download.sh "$GEOS_VERS"
    libgeos/build.sh
elif [[ "$GEOS_REBUILD" == "1" || "$1" == "--rebuild" ]]; then
    libgeos/build.sh
fi
