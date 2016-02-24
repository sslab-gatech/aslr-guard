/* Machine-dependent ELF indirect relocation inline functions.
   x86-64 version.
   Copyright (C) 2009-2014 Free Software Foundation, Inc.
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

#ifndef _DL_IREL_H
#define _DL_IREL_H

#include <stdio.h>
#include <unistd.h>

#include "../../elf/aslr-guard.h"

#define ELF_MACHINE_IRELA	1

static ElfW(Addr)
elf_ifunc_invoke (ElfW(Addr) addr)
{
  //ag-note: update address
  //addr = adjust_encode_code_addr (addr, MAY_ENCODE);
 
  ElfW(Addr) ret = addr;
  __asm__ __volatile__  (
      "call\t%1 #_volatile_\n\t"
      "mov\t%%rax, %0\n\t"
      :"=r" (ret)
      :"r" (addr)
      );
  // ret is either a plain pointer or encrypted pointer
  // if it is plain, return it directly; 
  // otherwise, decrypt it and return it
#ifdef USE_MAGIC_CODE
  if (ret > LEAST_ADDRESS && ret < AG_MAGIC_CODE)
#elif defined(USE_NONCE_DEVRAND) || defined(USE_NONCE_RDRAND)
  if (ret > LEAST_ADDRESS && ret < LARGEST_ADDRESS)
#else
  if (ret > LEAST_ADDRESS)
#endif
    return ret;
  else {
#ifdef USE_MAGIC_CODE
    if (ret > AG_MAGIC_CODE)
      ret = ret ^ AG_MAGIC_CODE;
#endif
    ElfW(Addr) offset = ret&0xffffffff;
    __asm__ __volatile__  (
#if defined(USE_NONCE_DEVRAND) || defined(USE_NONCE_RDRAND)
        "xor  %%gs:0x100008(%2), %1 \n\t"
#endif
        "mov %%gs:0x100000(%1), %%rax\n\t"
        "mov\t%%rax, %0\n\t"
        :"=r" (addr)
        :"r" (ret), "r" (offset)
        );
    return addr;
  }
  //return ((ElfW(Addr) (*) (void)) (addr)) ();
}

static inline void
__attribute ((always_inline))
elf_irela (const ElfW(Rela) *reloc)
{
  ElfW(Addr) *const reloc_addr = (void *) reloc->r_offset;
  const unsigned long int r_type = ELFW(R_TYPE) (reloc->r_info);

  if (__builtin_expect (r_type == R_X86_64_IRELATIVE, 1))
    {
      ElfW(Addr) value = elf_ifunc_invoke(reloc->r_addend);
      *reloc_addr = value;
    }
  else
    __libc_fatal ("unexpected reloc type in static binary");
}

#endif /* dl-irel.h */
