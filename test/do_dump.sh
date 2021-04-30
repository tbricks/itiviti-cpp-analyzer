#!/bin/bash

LLVM_ROOT="/opt/llvm-10"
GCC_ROOT="/opt/gcc-8.2.0"
FILTER="-ast-dump-filter=${2:-main}"

${LLVM_ROOT}/bin/clang++ -MJ tmp.o.json --std=c++1z \
    --gcc-toolchain=/opt/gcc-8.2.0 \
    -isystem/opt/gcc-8.2.0/lib/gcc/x86_64-linux-gnu/8.2.0/include \
    -o tmp.o -c $1

sed -e '1s/^/[\n/' -e '$s/,$/\n]/' *.o.json > compile_commands.json

${LLVM_ROOT}/bin/clang-check -extra-arg=-std=c++1z -ast-dump $FILTER $1

#${LLVM_ROOT}/bin/clang++ -Xclang -ast-dump -fsyntax-only --std=c++1z $1
