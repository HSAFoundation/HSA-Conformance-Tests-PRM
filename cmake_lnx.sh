#!/bin/bash

set -e

export HSAIL_TOOLS=/srv/git/HSAIL-Tools

export BASIC_OPTS="-DCMAKE_BUILD_TYPE=Debug -DHSAIL-Tools-PATH=$HSAIL_TOOLS"

mkdir -p build/lnx32
cd build/lnx32
cmake -G "Unix Makefiles" $BASIC_OPTS -DHSAIL-Tools-Subdir=lnx32 -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 -DCMAKE_SHARED_LINKER_FLAGS=-m32 -DCMAKE_INSTALL_PREFIX=dist "$@" ../..
cd ../..

mkdir -p build/lnx64
cd build/lnx64
cmake -G "Unix Makefiles" $BASIC_OPTS -DHSAIL-Tools-Subdir=lnx64 -DCMAKE_C_FLAGS=-m64 -DCMAKE_CXX_FLAGS=-m64 -DCMAKE_SHARED_LINKER_FLAGS=-m64 -DCMAKE_INSTALL_PREFIX=dist "$@" ../..
cd ../..
