#!/bin/bash

# modified run_spec_clang_asan.sh to support cast_sanitizer options

# Simple script to run CPU2006 with AddressSanitizer.
# Make sure to use spec version 1.2 (SPEC_CPU2006v1.2).
# Run this script like this:
# $./run_spec_clang_asan.sh TAG [test|train|ref] benchmarks
# TAG is any word. If you use different TAGS you can runs several builds in
# parallel.
# test is a small data set, train is medium, ref is large.
# To run all C use all_c, for C++ use all_cpp

# TODO : point to the installed speccpu dir

CUR_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
SPEC_DIR=$HOME/cpu2006

cd $SPEC_DIR

inst=$1
shift
name=$inst"_"$1
shift
size=$1
shift

ulimit -s 8092  # stack

BINUTILS_BIN="$CUR_DIR/../../binutils-2.24/prefix/bin"
GCC_BIN="$CUR_DIR/../../gcc-4.8.2/prefix/bin"
EGLIBC_LIB="$CUR_DIR/../../eglibc-2.19/prefix/lib"

SPEC_J=${SPEC_J:-8}
NUM_RUNS=${NUM_RUNS:-1}
BUILD_FLAGS="-pie -fPIC -g"
AG_FLAGS="-fomit-frame-pointer -maccumulate-outgoing-args -mno-push-args"
if [ "$inst" == "aslrguard" ]; then
  AG_FLAGS="-Wl,-dynamic-linker=${EGLIBC_LIB}/ld-2.19.so -B${EGLIBC_LIB} -fomit-frame-pointer -maccumulate-outgoing-args -mno-push-args -ffixed-r15"
  export PATH="$BINUTILS_BIN:$GCC_BIN:$PATH"
fi

BIT=${BIT:-64}
OPT_LEVEL=${OPT_LEVEL:-"-O0"}
COMMON_FLAGS="$BUILD_FLAGS -m$BIT $AG_FLAGS"

CC="gcc -std=gnu89 $COMMON_FLAGS"
CXX="g++ $COMMON_FLAGS"

echo "inst:" $inst
echo "name:" $name
echo "BUILD_FLAGS:" $BUILD_FLAGS
echo "AG_FLAGS:" $AG_FLAGS
echo "CC:" $CC
echo "CXX:" $CXX
echo "PATH:" $PATH


rm -rf config/$name.*

cat << EOF > config/$name.cfg
monitor_wrapper = $SPEC_WRAPPER  \$command
ignore_errors = yes
tune          = base
ext           = $name
output_format = asc, Screen
reportable    = 1
teeout        = yes
teerunout     = yes
strict_rundir_verify = 0
makeflags = -j$SPEC_J

default=default=default=default:
CC  = $CC
CXX = $CXX
EXTRA_LIBS = $EXTRA_LIBS
FC         = echo

default=base=default=default:
COPTIMIZE     = $OPT_LEVEL
CXXOPTIMIZE  =  $OPT_LEVEL

default=base=default=default:
PORTABILITY = -DSPEC_CPU_LP64

400.perlbench=default=default=default:
CPORTABILITY= -DSPEC_CPU_LINUX_X64

462.libquantum=default=default=default:
CPORTABILITY= -DSPEC_CPU_LINUX

483.xalancbmk=default=default=default:
CXXPORTABILITY= -DSPEC_CPU_LINUX -include string.h

447.dealII=default=default=default:
CXXPORTABILITY= -include string.h -include stdlib.h -include cstddef
EOF

# Don't report alloc-dealloc-mismatch bugs (there is on in 471.omnetpp)
. shrc
runspec -c $name -a run -I -l --size $size -n $NUM_RUNS $@
