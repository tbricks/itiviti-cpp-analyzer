#!/bin/bash

GCC_ROOT="/usr"
GCC_VERSION="9"

FILTER="-ast-dump-filter=${2:-main}"

${LLVM_ROOT}/bin/clang++ -MJ tmp.o.json --std=c++1z \
    --gcc-toolchain=${GCC_ROOT} \
    -isystem${GCC_ROOT}/lib/gcc/x86_64-linux-gnu/${GCC_VERSION}/include \
    -o tmp.o -c $1

sed -e '1s/^/[\n/' -e '$s/,$/\n]/' *.o.json > compile_commands.json

clang-check -extra-arg=-std=c++1z -ast-dump $FILTER $1

#${LLVM_ROOT}/bin/clang++ -Xclang -ast-dump -fsyntax-only --std=c++1z $1
