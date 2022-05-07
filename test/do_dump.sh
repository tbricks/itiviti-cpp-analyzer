#!/bin/bash

LLVM_ROOT="/opt/llvm-10"
GCC_ROOT="/opt/gcc-8.2.0"
FILTER="-ast-dump-filter=${2:-main}"

clang++ -MJ tmp.o.json --std=c++1z \
    -o tmp.o -c $1

sed -e '1s/^/[\n/' -e '$s/,$/\n]/' *.o.json > compile_commands.json

clang-check -extra-arg=-std=c++1z -ast-dump $FILTER $1

rm compile_commands.json tmp.o.json

#${LLVM_ROOT}/bin/clang++ -Xclang -ast-dump -fsyntax-only --std=c++1z $1
