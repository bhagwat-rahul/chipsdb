#!/usr/bin/env bash
mkdir -p build
clang -std=c99 -O2 -g src/*.c -o build/chipsdb
./build/chipsdb
