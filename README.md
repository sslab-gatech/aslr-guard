# ASLR-Guard

ASLR-Guard is a security mechanism aims to prevent code pointer leaks or render their leak harmless, 
so that code reuse attacks that need to first leak the randomized address can be prevented.
ASLR-Guard toolchain is built based on the GNU toolchain. 
ASLR-Guard paper was published at ACM CCS'15.

### Documentation
CCS'15 paper: https://sslab.gtisc.gatech.edu/assets/papers/2015/lu:aslrguard.pdf
Webpage: https://sslab.gtisc.gatech.edu/pages/memrand.html

### Build ASLR-Guard
It is easy to build:
```
cd <dir of ASLR-Guard>
$ ./setup.sh
```
Detailed building steps and codebase structure of ASLR-Guard can be found in setup.sh.
NOTE that your processor is supposed to support "rdrand" instruction; otherwise, define 
USE_MAGIC_CODE (a simulation of nonce, no guarantee in preventing function pointer replay attacks) 
and undefine USE_NONCE_RDRAND in eglibc-2.19/aslr-guard-config.h and 
binutils-2.24/gas/aslr-guard-config.h. More configurations are also available in these two files.

### Use ASLR-Guard to protect programs
Run the test cases:
```
$ cd <dir of ASLR-Guard>/test
$ make
```
Please have a look at test/Makefile.inc to see individual steps to use ASLR-Guard.

### Run SPEC Benchmarks with ASLR-Guard
Have SPEC CPU2006 Benchmarks installed
Then:
```
$ cd <dir of ASLR-Guard>/scripts/spec
$ ./quick_speccpu_run.sh <aslrguard|orig> all
```
