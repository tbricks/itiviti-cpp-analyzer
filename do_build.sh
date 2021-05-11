#!/bin/bash
[ ! -d build ] && mkdir build
cd build

GCC_ROOT="/usr"
LLVM_ROOT="/usr/lib/llvm-10"

cmake \
    -DCMAKE_C_COMPILER="clang" \
    -DCMAKE_CXX_COMPILER="clang++" \
    -DLLVM_CONFIG_PATH="${LLVM_ROOT}/bin/llvm-config" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DBOOST_ROOT="${BOOST}" \
    -DGCC_TOOLCHAIN="${GCC_ROOT}" \
    -G "Unix Makefiles" \
    ../
