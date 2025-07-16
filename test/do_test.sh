#!/bin/bash

LLVM_ROOT="/opt/llvm-20"

CHECKS=$2
if [ -z "${CHECKS}" ]; then
    CHECKS="all"
fi

${LLVM_ROOT}/bin/clang++-20 \
    -Xclang -load -Xclang ../build/libica-plugin.so \
    -Xclang -add-plugin -Xclang ica-plugin \
    -Xclang -plugin-arg-ica-plugin -Xclang checks=${CHECKS} -Xclang -verify \
    -std=c++17 \
    -c \
    $1 -o $1.o
