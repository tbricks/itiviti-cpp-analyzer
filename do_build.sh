#!/bin/bash

GCC_ROOT="/opt/gcc-15"
LLVM_ROOT="/opt/llvm-20"

[ -z ${CMAKE} ] && CMAKE="cmake"

${CMAKE} -S . -B ./build/ \
    -DCMAKE_C_COMPILER="${LLVM_ROOT}/bin/clang" \
    -DCMAKE_CXX_COMPILER="${LLVM_ROOT}/bin/clang++" \
    -DCMAKE_BUILD_TYPE=Release \
    -DGCC_TOOLCHAIN="${GCC_ROOT}" \
    -DBOOST_FROM_INTERNET=ON \
    -G "Unix Makefiles"
