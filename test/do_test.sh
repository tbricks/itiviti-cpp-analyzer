#!/bin/bash

GCC_ROOT="/usr"
GCC_VERSION="9"

CHECKS=$2
if [ -z "${CHECKS}" ]; then
    CHECKS="all"
fi

#    -fsyntax-only \
${LLVM_ROOT}/bin/clang++ \
    -Xclang -load -Xclang ../build/libica-plugin.so \
    -Xclang -add-plugin -Xclang ica-plugin \
    -Xclang -plugin-arg-ica-plugin -Xclang checks=${CHECKS} -Xclang -verify \
    -std=c++17 \
    --gcc-toolchain=${GCC_ROOT} \
    -isystem${GCC_ROOT}/lib/gcc/x86_64-linux-gnu/${GCC_VERSION}/include \
    -c \
    $1 -o $1.o

#${LLVM_ROOT}/bin/clang++ \
#    -Xclang -load -Xclang ../build/libclang-toy-plugin.so -Xclang -plugin -Xclang clang-toy-plugin \
#    -std=c++1z \
#    --gcc-toolchain=/opt/gcc-8.2.0 \
#    -isystem/opt/gcc-8.2.0/lib/gcc/x86_64-linux-gnu/8.2.0/include \
#    $1


#    -Xclang -load -Xclang ../cmake-build-debug/libclang-toy-plugin.so \
