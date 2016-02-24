#!/bin/bash -e

NJOB=$(nproc)
ROOT=$(git rev-parse --show-toplevel)
MAKE="make --no-print-directory -j$NJOB"
LOGS=$ROOT/logs

mkdir -p $LOGS

log() {
  echo "[!] Run $1"
  ($@ 2>&1 > "$LOGS/$1.log" | tee "$LOGS/$1.err") || {
    cat "$LOGS/$1.log"
    echo "[!] Check $LOGS/$1.{log|err}"
    exit 1
  }
}

test_binutil() {
  $ROOT/binutils-2.24/prefix/bin/as -u
}

build_binutil() {
  set -e
  cd $ROOT/binutils-2.24/build
  $MAKE
  $MAKE install

  test_binutil
}

build_gcc() {
  set -e
  cd $ROOT/gcc-4.8.2/build
  $MAKE
  $MAKE install
}

build_eglibc_orig() {
  set -e
  cd $ROOT/eglibc-2.19-orig/build
  $MAKE
  $MAKE install
}

build_eglibc() {
  set -e
  export PATH=$ROOT/binutils-2.24/prefix/bin:$ROOT/gcc-4.8.2/prefix/bin:$PATH
  cd $ROOT/eglibc-2.19/build
  $MAKE
  $MAKE install
}

cat <<EOF
build_libstdcpp() {
  set -e
  export PATH=$ROOT/binutils-2.24/prefix/bin:$ROOT/gcc-4.8.2/prefix/bin:$PATH
  cd $ROOT/gcc-4.8.2/build-libstdcpp
  $MAKE
  $MAKE install
}
EOF

add_pre_built_libs() {
   cp -r $ROOT/pre-built/. $ROOT/eglibc-2.19/prefix/lib/
}

test_all() {
  test_binutil
}

case "$1" in
  t*) log test_all          ;;
  b*) log build_binutil     ;;
  g*) log build_gcc         ;;
#  o*) log build_eglibc_orig ;;
  e*) log build_eglibc      ;;
  p*) log add_pre_built_libc ;;
  *)
    log build_binutil
    log build_gcc
    log build_eglibc_orig
    log build_eglibc
    log add_pre_built_libs
    ;;
esac
