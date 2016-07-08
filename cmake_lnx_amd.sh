#!/bin/bash
# Includes AMD specific experimental ORCA targets - no support in the main package

set -e

export OCL_PATH=/srv/hsa/drivers/opencl/dist/linux/debug
export ORCA_OPTS="-DOCL-Path=$OCL_PATH -DENABLE_HEXL_ORCA=1"

export BASIC_OPTS="-DCMAKE_BUILD_TYPE=Debug"

mkdir -p build/lnx32
cd build/lnx32
cmake -G "Unix Makefiles" $BASIC_OPTS -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 -DCMAKE_SHARED_LINKER_FLAGS=-m32 -DCMAKE_INSTALL_PREFIX=dist "$@" ../..
cd ../..

mkdir -p build/lnx64
cd build/lnx64
cmake -G "Unix Makefiles" $BASIC_OPTS -DCMAKE_C_FLAGS=-m64 -DCMAKE_CXX_FLAGS=-m64 -DCMAKE_SHARED_LINKER_FLAGS=-m64 -DCMAKE_INSTALL_PREFIX=dist "$@" ../..
cd ../..

mkdir -p build/lnx32_orca
cd build/lnx32_orca
cmake -G "Unix Makefiles" $BASIC_OPTS -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 -DCMAKE_SHARED_LINKER_FLAGS=-m32 -DCMAKE_INSTALL_PREFIX=dist $ORCA_OPTS "$@" ../..
cd ../..

mkdir -p build/lnx64_orca
cd build/lnx64_orca
cmake -G "Unix Makefiles" $BASIC_OPTS -DCMAKE_C_FLAGS=-m64 -DCMAKE_CXX_FLAGS=-m64 -DCMAKE_SHARED_LINKER_FLAGS=-m64 -DCMAKE_INSTALL_PREFIX=dist $ORCA_OPTS "$@" ../..
cd ../..
