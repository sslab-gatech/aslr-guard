/* setjmp for x86-64.
   Copyright (C) 2001-2014 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

#include <sysdep.h>
#include <jmpbuf-offsets.h>
#include <asm-syntax.h>
#include <stap-probe.h>

#include "../../aslr-guard-config.h"

ENTRY (__sigsetjmp)
	/* Save registers.  */
	movq %rbx, (JB_RBX*8)(%rdi)
#ifdef PTR_MANGLE
# ifdef __ILP32__
	/* Save the high bits of %rbp first, since PTR_MANGLE will
	   only handle the low bits but we cannot presume %rbp is
	   being used as a pointer and truncate it.  Here we write all
	   of %rbp, but the low bits will be overwritten below.  */
	movq %rbp, (JB_RBP*8)(%rdi)
# endif
	mov %RBP_LP, %RAX_LP
	PTR_MANGLE (%RAX_LP)
	mov %RAX_LP, (JB_RBP*8)(%rdi)
#else
	movq %rbp, (JB_RBP*8)(%rdi)
#endif
	movq %r12, (JB_R12*8)(%rdi)
	movq %r13, (JB_R13*8)(%rdi)
	movq %r14, (JB_R14*8)(%rdi)
	movq %r15, (JB_R15*8)(%rdi)
	lea 8(%rsp), %RDX_LP #_volatile_ /* Save SP as it will be after we return.  */
  
  // ag-note: encode RSP
#ifdef AG_ENCODE_CP
  push  %r15 #_volatile_
  mov %gs:0x100000, %r15 #get mapping table size
  mov %RDX_LP, %gs:0x100000(%r15) #save real fp
#ifdef USE_MAGIC_CODE
  push %rax #_volatile_
  push %rbx #_volatile_
  mov %r15, %rax
  movabs $AG_MAGIC_CODE, %rbx
  or %rbx, %r15
  mov %r15, %gs:0x100008(%rax) #save encrypted fp
  pop %rbx #_volatile_
  pop %rax #_volatile_
#elif USE_NONCE_DEVRAND
  push %rdi  #_volatile_
  push %rsi  #_volatile_
  push %rdx  #_volatile_
  push %rax  #_volatile_
  # syscall may destroy rcx and r11, so save them
  push %rcx  #_volatile_
  push %r11  #_volatile_
  # load file handler of /dev/urandom
  mov  %gs:0x100008, %rdi
  # location (%gs:0x10001c) for saving random value
  mov  %gs:0x100010, %rsi
  # read 4 bytes
  mov  $4, %rdx
  # syscall number for "read"
  mov  $0, %eax
  # do syscall
  syscall
  # load random value
  mov  -4(%rsi), %rax
  # save current location in safe memory
  mov  %r15, %rdi
  # save 4-byte nonce
  mov  %rax, %gs:0x100008(%rdi)
  or   %rax, %r15
  pop %r11 #_volatile_
  pop %rcx #_volatile_
  pop %rax #_volatile_
  pop %rdx #_volatile_
  pop %rsi #_volatile_
  pop %rdi #_volatile_
#elif USE_NONCE_RDRAND
  push %rdi #_volatile_
  push %rax #_volatile_
  rdrand %eax
  shl $32, %rax
  mov %r15, %rdi
  mov  %rax, %gs:0x100008(%rdi)
  or %rax, %r15
  pop %rax #_volatile_
  pop %rdi #_volatile_
#else
  mov %r15, %gs:0x100008(%r15) #save encrypted fp
#endif
  add $0x10, %gs:0x100000  #update mapping table size
  mov %r15, %RDX_LP  #encrypted fp
  pop %r15 #_volatile_
#endif
#ifdef PTR_MANGLE
	PTR_MANGLE (%RDX_LP)
#endif
	movq %rdx, (JB_RSP*8)(%rdi)
  /* Save PC we are returning to now.  */
	mov (%rsp), %RAX_LP	#_volatile_

  // ag-note: encode %RAX_LP
#ifdef AG_ENCODE_CP
  push  %r15 #_volatile_
  mov %gs:0x100000, %r15 #get mapping table size
  mov %RAX_LP, %gs:0x100000(%r15) #save real fp
#ifdef USE_MAGIC_CODE
  push %rax #_volatile_
  push %rbx #_volatile_
  mov %r15, %rax
  movabs $AG_MAGIC_CODE, %rbx
  or %rbx, %r15
  mov %r15, %gs:0x100008(%rax) #save encrypted fp
  pop %rbx #_volatile_
  pop %rax #_volatile_
#elif USE_NONCE_DEVRAND
  push %rdi  #_volatile_
  push %rsi  #_volatile_
  push %rdx  #_volatile_
  push %rax  #_volatile_
  # syscall may destroy rcx and r11, so save them
  push %rcx  #_volatile_
  push %r11  #_volatile_
  # load file handler of /dev/urandom
  mov  %gs:0x100008, %rdi
  # location (%gs:0x10001c) for saving random value
  mov  %gs:0x100010, %rsi
  # read 4 bytes
  mov  $4, %rdx
  # syscall number for "read"
  mov  $0, %eax
  # do syscall
  syscall
  # load random value
  mov  -4(%rsi), %rax
  # save current location in safe memory
  mov  %r15, %rdi
  # save 4-byte nonce
  mov  %rax, %gs:0x100008(%rdi)
  or   %rax, %r15
  pop %r11 #_volatile_
  pop %rcx #_volatile_
  pop %rax #_volatile_
  pop %rdx #_volatile_
  pop %rsi #_volatile_
  pop %rdi #_volatile_
#elif USE_NONCE_RDRAND
  push %rdi #_volatile_
  push %rax #_volatile_
  rdrand %eax
  shl $32, %rax
  mov %r15, %rdi
  mov  %rax, %gs:0x100008(%rdi)
  or %rax, %r15
  pop %rax #_volatile_
  pop %rdi #_volatile_
#else
  mov %r15, %gs:0x100008(%r15) #save encrypted fp
#endif
  add $0x10, %gs:0x100000  #update mapping table size
  mov %r15, %RAX_LP  #encrypted fp
  pop %r15 #_volatile_
#endif

	LIBC_PROBE (setjmp, 3, LP_SIZE@%RDI_LP, -4@%esi, LP_SIZE@%RAX_LP)
#ifdef PTR_MANGLE
	PTR_MANGLE (%RAX_LP)
#endif
	movq %rax, (JB_PC*8)(%rdi)

#if defined NOT_IN_libc && defined IS_IN_rtld
	/* In ld.so we never save the signal mask.  */
	xorl %eax, %eax
	retq
#else
	/* Make a tail call to __sigjmp_save; it takes the same args.  */
# ifdef	PIC
	jmp C_SYMBOL_NAME (__sigjmp_save)@PLT
# else
	jmp __sigjmp_save
# endif
#endif
END (__sigsetjmp)
hidden_def (__sigsetjmp)
