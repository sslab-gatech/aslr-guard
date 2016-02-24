CUR_DIR = $(realpath $(LEVEL))

BINUTILS_BIN := $(CUR_DIR)/../binutils-2.24/prefix/bin
GCC_BIN      := $(CUR_DIR)/../gcc-4.8.2/prefix/bin
EGLIBC_LIB   := $(CUR_DIR)/../eglibc-2.19/prefix/lib

BEFORE_FLAG =

ASLR_GUARD_FLAG =-Wl,-dynamic-linker=$(EGLIBC_LIB)/ld-2.19.so \
  -pie -fPIC -save-temps -fverbose-asm\
  -B$(EGLIBC_LIB) \
  -fomit-frame-pointer \
  -maccumulate-outgoing-args \
  -mno-push-args \
  -ffixed-r15 \
  -g -O0

all: before.out guard.out before.s

before.out: $(SRCS)
	g++ $(BEFORE_FLAG) $^ -o $@ -ldl

before.s: $(SRCS)
	g++ $(BEFORE_FLAG) -S $^ -o $@

guard.out: $(SRCS)
	PATH=$(BINUTILS_BIN):$(PATH) $(GCC_BIN)/g++ $(ASLR_GUARD_FLAG) $^ -o $@ -ldl

gdb-gcc:
	PATH=$(BINUTILS_BIN):$(PATH) gdb $(GCC_BIN)/g++ --args $(GCC_BIN)/g++ $(ASLR_GUARD_FLAG) $(SRCS) -o guard.out

gdb-guard: guard.out
	gdb guard.out

diff: before.s
	cp -f before.s after.s
	$(BINUTILS_BIN)/as after.s
	-diff -urN before.s after.s

diff-bin: before.out guard.out
	-bash -c "diff -urN <(objdump -D before.out) <(objdump -D guard.out)"

run: guard.out
	LD_LIBRARY_PATH=$(EGLIBC_LIB):$(LD_LIBRARY_PATH) ./guard.out

clean:
	@rm -rf *.out *.s

.PHONY: all run clean test diff
