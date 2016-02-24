#!/bin/bash -e

# Tested on Ubuntu 14.04 and Ubuntu 15.04, "rdrand" support is required.
# ASLR-Guard mainly consists of binutils-2.24 (with the static linker), eglibc-2.19 (with
# dynamic loader and some shared libraies, e.g., libc), gcc-4.8.2 (with gcc and g++).
# Most modified parts can be found by searching for "ag-note" and "USE_ASLR_GUARD" in code.

cat <<EOF
------------------------------------------------------------------------------
aslr-guard contains 4 parts: 
1) compiler (in gcc-4.8.2)
2) assembler (in binutils-2.24)
3) linker (in binutils-2.24)
4) dynamic linker (in eglibc-2.19)

gcc-4.8.2 and binutils-2.24 should be compiled by original gcc first. 
Then we get the modified compiler, assembler, and linker.
After that, we use the modified compiler, assembler, and linker to compile 
eglibc-2.19 (e.g., dynamic linker)

Main modifications
1) compiler: simply setting "save_regs_using_mov" to be true
2) assembler: dynamic function pointer encoding/decoding, and safe stack. 
  We instrumented address-taking instrucitons, indirect call/jmp, push/pop 
  instruction, and replanced rsp with r15
3) linker: simply collecting some location information of data-access instructions, 
  as we need to adjust offset from data section to code section during module loading
4) dynamic linker: remapping code section to random address and adjust offsets.
  encoding/decoding static function pointers.
------------------------------------------------------------------------------
EOF

NJOB=`nproc`
ROOT=$(git rev-parse --show-toplevel)
LOGS=$ROOT/logs

# XXX. for arch dist
GCC_VERSION=${GCC_VERSION:-gcc-4.8}

mkdir -p $LOGS

log() {
  echo "[!] Run $1"
  ($@ 2>&1 > "$LOGS/$1.log" | tee "$LOGS/$1.err") || {
    cat "$LOGS/$1.log"
    echo "[!] Check $LOGS/$1.{log|err}"
    exit 1
  }
}

prerequisite() {
  if lsb_release -a | grep -q Ubuntu; then
    sudo apt-get install flex bison libmpfr-dev libgmp3-dev \
         libmpc-dev libc6-dev-i386 texinfo linux-libc-dev \
         gawk g++
  fi
}

build_binutils() {
  SRC=$ROOT/binutils-2.24

  cd $SRC
  rm -rf build
  rm -rf prefix

  mkdir build
  mkdir prefix

  cd $SRC/build

  ../configure \
    CC=$GCC_VERSION \
    CFLAGS='-g -O2 -Wno-error=unused-value' \
    --prefix=$SRC/prefix \
    --disable-nls \
    --with-lib-path=/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu \
    --with-sysroot=/usr/local

  make -j$NJOB
  make install
}

check_binutils() {
  SRC=$ROOT/binutils-2.24

  # check as since it randomly fails
  echo "[!] Testing as"
  for i in {1..10}; do
    $SRC/prefix/bin/as --version
  done
}

build_gcc() {
  SRC=$ROOT/gcc-4.8.2

  cd $SRC
  rm -rf build
  rm -rf prefix

  mkdir build
  mkdir prefix

  cd $SRC/build
  ../configure \
    --build=x86_64-linux-gnu \
    CC=$GCC_VERSION \
    CFLAGS='-g -O2 -Wno-error=unused-value' \
    --disable-bootstrap --enable-checking=none \
    --disable-multi-arch --disable-multilib \
    --enable-languages=c,c++ \
    --with-gmp=/usr/local/lib \
    --with-mpc=/usr/lib --with-mpfr=/usr/lib \
    --prefix=$SRC/prefix
  make -j$NPROC
  make install
}

check_gcc() {
  SRC=$ROOT/gcc-4.8.2
  echo "[!] Testing gcc"
  $SRC/prefix/bin/gcc --version
}

build_eglibc_orig() {
  SRC=$ROOT/eglibc-2.19-orig

  cd $SRC
  rm -rf build
  rm -rf prefix

  mkdir build
  mkdir prefix

  cd $SRC/build
  ../configure \
    CC=gcc \
    CFLAGS='-g -O2' \
    --prefix=$SRC/prefix
  make -j$NPROC
  make install
}

build_eglibc() {
  SRC=$ROOT/eglibc-2.19

  cd $SRC
  rm -rf build
  rm -rf prefix

  mkdir build
  mkdir prefix

  cd $SRC/build
  export PATH=$ROOT/binutils-2.24/prefix/bin:$ROOT/gcc-4.8.2/prefix/bin:$PATH
  ../configure \
    CC=gcc \
    CFLAGS='-fomit-frame-pointer -maccumulate-outgoing-args -mno-push-args -ffixed-r15 -g -O2' \
    --prefix=$SRC/prefix \
    --disable-multi-arch \
    --with-binutils=$ROOT/binutils-2.24/prefix/bin \
    --disable-profile
  make -j$NPROC
  make install
}

check_eglibc() {
  echo;
}

cat <<EOF 
build_libstdcpp() {
  SRC=$ROOT/gcc-4.8.2

  cd $SRC
  rm -rf build-libstdcpp

  mkdir build-libstdcpp

  cd $SRC/build-libstdcpp
  export PATH=$ROOT/binutils-2.24/prefix/bin:$ROOT/gcc-4.8.2/prefix/bin:$PATH
  ../configure \
    CC=gcc \
    CFLAGS='-fomit-frame-pointer -maccumulate-outgoing-args -mno-push-args -ffixed-r15 -g -O2' \
    --prefix=$ROOT/eglibc-2.19/prefix \
    --disable-multi-arch \
    --disable-multilib \
    --disable-profile
  make -j$NPROC
  make install

  #cp pre-built libgcc library
  cp $ROOT/pre-built/libgcc_s.so $ROOT/eglibc-2.19/prefix/lib/
  cp $ROOT/pre-built/libgcc_s.so.1 $ROOT/eglibc-2.19/prefix/lib/
}

check_libstdcpp() {
  echo;
}
EOF

add_pre_built_libs() {
   cp -r $ROOT/pre-built/. $ROOT/eglibc-2.19/prefix/lib/
}

prerequisite

log build_binutils
log check_binutils

log build_gcc
log check_gcc

log build_eglibc_orig
log build_eglibc
log check_eglibc

log add_pre_built_libs

