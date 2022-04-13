#!/bin/bash
[ ! -d build ] && mkdir build
cd build

GCC_ROOT="/opt/gcc-8.2.0"
LLVM_ROOT="/opt/llvm-10"

cmake \
    -DCMAKE_C_COMPILER="${LLVM_ROOT}/bin/clang" \
    -DCMAKE_CXX_COMPILER="${LLVM_ROOT}/bin/clang++" \
    -DCMAKE_BUILD_TYPE=Release \
    -DGCC_TOOLCHAIN="${GCC_ROOT}" \
    -DBOOST_FROM_INTERNET=ON \
    -G "Unix Makefiles" \
    ../
