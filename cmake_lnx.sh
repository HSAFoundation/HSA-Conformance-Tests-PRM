#!/bin/bash

set -e

export BASIC_OPTS="-DCMAKE_BUILD_TYPE=Debug"

mkdir -p build/lnx32
cd build/lnx32
cmake -G "Unix Makefiles" $BASIC_OPTS -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 -DCMAKE_SHARED_LINKER_FLAGS=-m32 -DCMAKE_INSTALL_PREFIX=dist "$@" ../..
cd ../..

mkdir -p build/lnx64
cd build/lnx64
cmake -G "Unix Makefiles" $BASIC_OPTS -DCMAKE_C_FLAGS=-m64 -DCMAKE_CXX_FLAGS=-m64 -DCMAKE_SHARED_LINKER_FLAGS=-m64 -DCMAKE_INSTALL_PREFIX=dist "$@" ../..
cd ../..
