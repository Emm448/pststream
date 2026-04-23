#!/bin/bash

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <file.c> <output>"
    exit 1
fi

MAIN_FILE=$1
OUTPUT=$2

# Compile
gcc -I ./include ./src/*.c "$MAIN_FILE" -o "$OUTPUT"

# Results
if [ $? -eq 0 ]; then
    echo "Out file: $OUTPUT"
else
    echo "Compile error"
fi