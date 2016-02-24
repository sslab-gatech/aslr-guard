#!/bin/bash

CUR_DIR=$(pwd)

BINUTILS_BIN=$CUR_DIR"/../../binutils-2.24/prefix/bin"
GCC_BIN=$CUR_DIR"/../../gcc-4.8.2/prefix/bin"
EGLIBC_LIB=$CUR_DIR"/../../eglibc-2.19/prefix/lib"

AG_FLAGS=-B$EGLIBC_LIB' -fomit-frame-pointer -maccumulate-outgoing-args -mno-push-args -ffixed-r15 -g -O2 -pie -fPIC -Wl,-dynamic-linker='$EGLIBC_LIB'/ld-2.19.so'
echo $AG_FLAGS

rm -rf build prefix
mkdir build prefix && cd build

export PATH=$BINUTILS_BIN:$PATH
../configure CC=$GCC_BIN/gcc CFLAGS="${AG_FLAGS}" --prefix=$CUR_DIR/../prefix

make -j8 && make install

