#!/bin/bash

set -e

export HSAIL_TOOLS=/srv/git/HSAIL-Tools
export HSA_RUNTIME_INC=/srv/hsa/drivers/hsa/runtime/opensrc/hsa-runtime/inc
export HSA_RUNTIME_EXT_INC=/srv/hsa/drivers/hsa/compiler/finalizer/Interface

export BASIC_OPTS="-DCMAKE_BUILD_TYPE=Debug -DHSAIL-Tools-PATH=$HSAIL_TOOLS -DHSA-Runtime-Inc-PATH=$HSA_RUNTIME_INC -DHSA-Runtime-Ext-Inc-PATH=$HSA_RUNTIME_EXT_INC"

mkdir -p build/lnx32
cd build/lnx32
export EXTRA_CFLAGS=-m32
cmake -G "Unix Makefiles" $BASIC_OPTS -DHSAIL-Tools-Subdir=lnx32 -DEXTRA_CFLAGS=$EXTRA_CFLAGS "$@" ../..
cd ../..

mkdir -p build/lnx64
cd build/lnx64
export EXTRA_CFLAGS=-m64
cmake -G "Unix Makefiles" $BASIC_OPTS -DHSAIL-Tools-Subdir=lnx64 -DEXTRA_CFLAGS=$EXTRA_CFLAGS "$@" ../..
cd ../..
