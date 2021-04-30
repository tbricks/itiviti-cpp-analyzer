#!/bin/bash

[ ! -d build ] && mkdir build
cd build

GCC_ROOT="/opt/gcc-8.2.0"
LLVM_ROOT="/opt/llvm-10"

cmake \
    -DCMAKE_C_COMPILER="${LLVM_ROOT}/bin/clang" \
    -DCMAKE_CXX_COMPILER="${LLVM_ROOT}/bin/clang++" \
    -DLLVM_CONFIG_PATH="${LLVM_ROOT}/bin/llvm-config" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DBOOST_ROOT="${BOOST}" \
    -DGCC_TOOLCHAIN="${GCC_ROOT}" \
    -G "Unix Makefiles" \
    ../
