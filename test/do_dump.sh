#!/bin/bash

LLVM_ROOT="/opt/llvm-20"
FILTER="-ast-dump-filter=${2:-main}"

${LLVM_ROOT}/bin/clang++ -MJ tmp.o.json --std=c++1z \
    -o tmp.o -c $1

sed -e '1s/^/[\n/' -e '$s/,$/\n]/' *.o.json > compile_commands.json

${LLVM_ROOT}/bin/clang-check -extra-arg=-std=c++1z -ast-dump $FILTER $1

rm compile_commands.json tmp.o.json
