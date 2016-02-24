#!/bin/bash

cp test.s x.s
$(dirname "$0")/../../binutils-2.24/prefix/bin/as -aslr-only-asm-preprocess x.s
cat x.s
