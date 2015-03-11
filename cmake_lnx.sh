#!/bin/bash

set -e

export HSAIL_TOOLS=/srv/git/HSAIL-Tools
export HSA_RUNTIME_INC=/srv/hsa_prm/drivers/hsa/runtime/inc
export HSA_RUNTIME_EXT_INC=/srv/hsa_prm/drivers/hsa/compiler/finalizer/Interface
export OCL_PATH=/srv/hsa_prm/drivers/opencl/dist/windows/debug
export ORCA_OPTS="-DOCL-Path=%OCL_PATH% -DENABLE_HEXL_ORCA=1"

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

mkdir -p build/lnx32_orca
cd build/lnx32_orca
export EXTRA_CFLAGS=-m32
cmake -G "Unix Makefiles" $BASIC_OPTS -DHSAIL-Tools-Subdir=lnx32 -DEXTRA_CFLAGS=$EXTRA_CFLAGS $ORCA_OPTS "$@" ../..
cd ../..

mkdir -p build/lnx64_orca
cd build/lnx64_orca
export EXTRA_CFLAGS=-m64
cmake -G "Unix Makefiles" $BASIC_OPTS -DHSAIL-Tools-Subdir=lnx64 -DEXTRA_CFLAGS=$EXTRA_CFLAGS $ORCA_OPTS "$@" ../..
cd ../..
