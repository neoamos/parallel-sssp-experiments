#!/bin/bash

if [ $# -ne 3 ] && [ $# -ne 4 ]; then
    echo Usage: $0 graph.txt reference-output.txt [binary]
    exit 1;
fi;
BINARY=${3:-./sssp_serial}
t=`mktemp`
echo Temporary output is $t
$BINARY $1 $t $4 && diff -qs $t $2
rm -f $t
