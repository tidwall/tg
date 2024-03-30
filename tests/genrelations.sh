#!/bin/bash

set -e
cd $(dirname "${BASH_SOURCE[0]}")

compfile() {
    hexdump -v -e '16/1 "_x%02X" "\n"' $1 | \
        sed 's/_/\\/g; s/\\x  //g; s/.*/    "&"/'
    echo ";"
}

compheader() {
    echo "// Auto generated file, DO NOT EDIT"
    for file in relations/*.jsonc
    do
        filename=$(basename $file)
        filename="${filename%.*}_jsonc"
        echo "static const char $filename[] ="
        compfile $file
    done
    echo "    
struct relation {
    const char *name;
    const char *data;
};

static struct relation relations[] = {"
    for file in relations/*.jsonc
    do
        filename=$(basename $file)
        filename="${filename%.*}_jsonc"
        printf "    { \"%s\", %s },\n" $(basename $file) $filename
    done

    echo "};"
}

compheader > relations.h

