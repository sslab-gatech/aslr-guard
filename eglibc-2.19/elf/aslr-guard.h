#ifndef ASLR_GUARD_H
#define ASLR_GUARD_H


#include <time.h>
#include "../aslr-guard-config.h"

#define AG_Addr unsigned long

// address of safe memory, will be destroyed after use
AG_Addr safeMemReg; 

// code remapping records
struct remapInfo {
  AG_Addr l_addr;
  AG_Addr oldCodeBase;
  AG_Addr newCodeBase;
  AG_Addr oldGpBase;   //.got.plt
  AG_Addr newGpBase;
  size_t codeSize;        //size of all code
  size_t gotpltSize;
  // rodata section info
  AG_Addr oldRelRoBase;
  AG_Addr newRelRoBase;
  size_t relRoSize;
};
off_t current_remapInfo_off;

AG_Addr *stat_ptr;

/*********************************/
/* ag-note: encrypt function pointer and save fp mapping entry
 * in isolated memory*/
  static inline AG_Addr __attribute__ ((unused))
encrypt_function_pointer (AG_Addr fp)
{
#ifndef AG_ENCODE_CP
  return fp;
#endif
  if (fp < LEAST_ADDRESS || fp > AG_MAGIC_CODE || 
      fp > LARGEST_ADDRESS)
    return fp;
  AG_Addr enc_fp;
  // TODO: check if the pointer is already encrypted (see NOTE.todo)
  __asm__ __volatile__(
      "push %%r15 #_volatile_\n\t"
      "mov %%gs:0x100000, %%r15 \n\t"     //get mapping table size
      "mov %1, %%gs:0x100000(%%r15) \n\t" //save real fp
#ifdef USE_MAGIC_CODE
      "push %%rbx #_volatile_\n\t"
      "mov %%r15, %%rbx  \n\t" 
      "or %2, %%r15  \n\t"
      "mov %%r15, %%gs:0x100008(%%rbx) \n\t" //save encrypted fp
      "pop %%rbx  #_volatile_\n\t"
#elif USE_NONCE_DEVRAND
      "push %%rdi  #_volatile_\n\t"
      "push %%rsi  #_volatile_\n\t"
      "push %%rdx  #_volatile_\n\t"
      "push %%rax  #_volatile_\n\t"
      "push %%rcx  #_volatile_\n\t"       // syscall may destroy rcx and r11, so save them
      "push %%r11  #_volatile_\n\t"
      "mov  %%gs:0x100008, %%rdi \n\t"    // load file handler of /dev/urandom
      "mov  %%gs:0x100010, %%rsi \n\t"    // location (%gs:0x10001c) for saving random value
      "mov  $4, %%rdx \n\t"               // read 4 bytes
      "mov  $0, %%eax \n\t"               // syscall number for "read"
      "syscall \n\t"                      // do syscall
      "mov  -4(%%rsi), %%rax \n\t"        // load random value
      "mov  %%r15, %%rdi \n\t"            // save current location in safe memory
      "mov  %%rax, %%gs:0x100008(%%rdi) \n\t" //save 4-byte nonce
      "or   %%rax, %%r15 \n\t"
      "pop %%r11 #_volatile_\n\t"
      "pop %%rcx #_volatile_\n\t"
      "pop %%rax #_volatile_\n\t"
      "pop %%rdx #_volatile_\n\t"
      "pop %%rsi #_volatile_\n\t"
      "pop %%rdi #_volatile_\n\t"
#elif USE_NONCE_RDRAND
      "push %%rdi #_volatile_\n\t"
      "push %%rax #_volatile_\n\t"
      "rdrand %%eax \n\t"                 // generate random nonce
      "shl $32, %%rax \n\t"               // move to high 32 bits
      "mov %%r15, %%rdi \n\t"             // save location
      "mov  %%rax, %%gs:0x100008(%%rdi) \n\t" // save nonce
      "or %%rax, %%r15 \n\t"              // encryption
      "pop %%rax #_volatile_\n\t"
      "pop %%rdi #_volatile_\n\t"
#else
      "mov %%r15, %%gs:0x100008(%%r15) \n\t" //save encrypted fp
#endif
      "add $0x10, %%gs:0x100000  \n\t"    //update mapping table size
      "mov %%r15, %0  \n\t"               //encrypted fp
      "pop %%r15  #_volatile_\n\t"
      : "=r" (enc_fp)
#ifdef USE_MAGIC_CODE
      : "r" (fp), "r"(AG_MAGIC_CODE)
#else
      : "r" (fp)
#endif
      );
  return enc_fp;
}


/* ag-note: check if an address is pointing to gotplt */
  static inline bool __attribute__ ((unused))
is_gotplt (AG_Addr addr) 
{
  if (addr < LEAST_ADDRESS || addr > AG_MAGIC_CODE)
    return false;
  if (safeMemReg != 0) {
    int idx = 0;
    size_t gotpltSize = 0;
    AG_Addr oldGotpltBase = 0;
    while (idx < *(int *)safeMemReg) {
      oldGotpltBase = *(AG_Addr*)(safeMemReg + sizeof(int) +
          idx * sizeof(struct remapInfo) + 3 * sizeof(AG_Addr));
      gotpltSize = *(AG_Addr*)(safeMemReg + sizeof(int) + 
          idx * sizeof(struct remapInfo) + 6 * sizeof(AG_Addr));
      if (oldGotpltBase <= addr && addr < oldGotpltBase + gotpltSize) {
        return true;
      }
      ++idx;
    }
  }
  return false;
}

/* ag-note: check if an address is pointing to .rodata section */
  static inline bool __attribute__ ((unused))
is_rodata (AG_Addr addr) 
{
  if (addr < LEAST_ADDRESS || addr > AG_MAGIC_CODE)
    return false;
  if (safeMemReg != 0) {
    int idx = 0;
    size_t relRoSize = 0;
    AG_Addr oldRelRoBase = 0;
    while (idx < *(int *)safeMemReg) {
      oldRelRoBase = *(AG_Addr*)(safeMemReg + sizeof(int) +
          idx * sizeof(struct remapInfo) + 7 * sizeof(AG_Addr));
      relRoSize = *(AG_Addr*)(safeMemReg + sizeof(int) + 
          idx * sizeof(struct remapInfo) + 9 * sizeof(AG_Addr));
      if (oldRelRoBase <= addr && addr < oldRelRoBase + relRoSize) {
        return true;
      }
      ++idx;
    }
  }
  return false;
}

/* ag-note: adjust address according to 
 * the offset introduced by remapping 
 * mode: 
 * 1: don't encode; 
 * 2: encode when found remap info; 
 * 4: always encode*/
#define NOT_ENCODE 1
#define MAY_ENCODE 2
#define ALWAYS_ENCODE 4
  static  AG_Addr __attribute__ ((unused))
adjust_encode_code_addr (AG_Addr addr, unsigned int mode, AG_Addr loc) 
{
#if defined(REMAP_CODE_TO_RANDOM) && defined(AG_ENCODE_CP)
  if (addr < LEAST_ADDRESS || addr > AG_MAGIC_CODE)
    return addr;
  if (safeMemReg != 0) {
    int idx = 0;
    size_t codeSize;
    AG_Addr oldCodeBase = 0, newCodeBase = 0;

    size_t gotpltSize = 0;
    AG_Addr oldGotpltBase = 0;
    size_t relRoSize = 0;
    AG_Addr oldRelRoBase = 0;

    while (idx < *(int *)safeMemReg) {
      oldCodeBase = *(AG_Addr*)(safeMemReg + sizeof(int) +
          idx * sizeof(struct remapInfo) + sizeof(AG_Addr));
      codeSize = *(AG_Addr*)(safeMemReg + sizeof(int) + 
          idx * sizeof(struct remapInfo) + 5 * sizeof(AG_Addr));
      if (oldCodeBase <= addr && addr < oldCodeBase + codeSize) {
        if (loc > 0) {
          // avoid encoding gotplt entries
          oldGotpltBase = *(AG_Addr*)(safeMemReg + sizeof(int) +
              idx * sizeof(struct remapInfo) + 3 * sizeof(AG_Addr));
          gotpltSize = *(AG_Addr*)(safeMemReg + sizeof(int) + 
              idx * sizeof(struct remapInfo) + 6 * sizeof(AG_Addr));
          if (oldGotpltBase <= loc && loc < oldGotpltBase + gotpltSize)
            return addr;
        }

#ifdef DO_STATISTICS
        ++(*stat_ptr);
#endif
        newCodeBase = *(AG_Addr*)(safeMemReg + sizeof(int) +
            idx * sizeof(struct remapInfo) + 2 * sizeof(AG_Addr));
        if (mode & MAY_ENCODE)
          // do encryption
          addr = encrypt_function_pointer(addr + newCodeBase - oldCodeBase);
        else if(mode & NOT_ENCODE)
          addr += newCodeBase - oldCodeBase;
        return addr;
      }
      ++idx;
    }
  }
  if (mode & ALWAYS_ENCODE)
    return encrypt_function_pointer(addr);
#endif
  return addr;
}

/* fast address adjustration. 
 * remapInfo is provided from current_remapInfo_off */
  static inline AG_Addr __attribute__ ((unused))
adjust_encode_code_addr_fast (AG_Addr addr,
    off_t current_remapInfo_off, AG_Addr loc)
{
#if defined(REMAP_CODE_TO_RANDOM) && defined(AG_ENCODE_CP)
  if (addr < LEAST_ADDRESS || addr > AG_MAGIC_CODE)
    return addr;
#ifdef DO_STATISTICS
  ++(*stat_ptr);
  ++(*(stat_ptr+1));
#endif
  if (safeMemReg != 0 && current_remapInfo_off > 0) {
    AG_Addr oldCodeBase = *(AG_Addr*)(safeMemReg + 
        current_remapInfo_off +
        sizeof(AG_Addr));
    AG_Addr newCodeBase = *(AG_Addr*)(safeMemReg + 
        current_remapInfo_off +
        2 * sizeof(AG_Addr));

    addr += (newCodeBase - oldCodeBase);
    return addr;
  }
#endif
  return addr;
}
/* ag-note: get the offset between remapped code and
 * original code based on load address */
  static inline off_t __attribute__ ((unused))
get_offset_remap_code_l_addr (AG_Addr l_addr)
{
#if defined(REMAP_CODE_TO_RANDOM) && defined(AG_ENCODE_CP)
  if (safeMemReg != 0) {
    int idx = 0;
    AG_Addr oldCodeBase = 0, newCodeBase = 0, old_l_addr = 0;
    while (idx < *(int *)safeMemReg) {
      old_l_addr = *(AG_Addr*)(safeMemReg + sizeof(int) +
          idx * sizeof(struct remapInfo));
      if (old_l_addr == l_addr) {
        oldCodeBase = *(AG_Addr*)(safeMemReg + sizeof(int) +
            idx * sizeof(struct remapInfo) + sizeof(AG_Addr));
        newCodeBase = *(AG_Addr*)(safeMemReg + sizeof(int) +
            idx * sizeof(struct remapInfo) + 2 * sizeof(AG_Addr));
        return newCodeBase - oldCodeBase;
      }
      ++idx;
    }
  }
#endif
  return 0;
}
/* ag-note: get the offset between remapped code and
 * original code, given code address */
  static inline off_t __attribute__ ((unused))
get_offset_remap_code_addr (AG_Addr addr)
{
#if defined(REMAP_CODE_TO_RANDOM) && defined(AG_ENCODE_CP)
  if (safeMemReg != 0) {
    int idx = 0;
    AG_Addr oldCodeBase = 0, newCodeBase = 0;
    size_t codeSize;
    while (idx < *(int *)safeMemReg) {
      oldCodeBase = *(AG_Addr*)(safeMemReg + sizeof(int) +
          idx * sizeof(struct remapInfo) + sizeof(AG_Addr));
      codeSize = *(AG_Addr*)(safeMemReg + sizeof(int) + 
          idx * sizeof(struct remapInfo) + 5 * sizeof(AG_Addr));
      if (oldCodeBase <= addr && addr < oldCodeBase + codeSize) {
        newCodeBase = *(AG_Addr*)(safeMemReg + sizeof(int) +
            idx * sizeof(struct remapInfo) + 2 * sizeof(AG_Addr));
        return newCodeBase - oldCodeBase;
      }
      ++idx;
    }
  }
#endif
  return 0;
}

/* ag-note: get the offset between remapped and
 * original .got.plt, given load address */
  static inline off_t __attribute__ ((unused))
get_offset_remap_gotplt_l_addr (AG_Addr l_addr)
{
#if defined(REMAP_CODE_TO_RANDOM) && defined(AG_ENCODE_CP)
  if (safeMemReg != 0) {
    int idx = 0;
    AG_Addr oldGotpltBase = 0, newGotpltBase = 0, old_l_addr = 0;
    while (idx < *(int *)safeMemReg) {
      old_l_addr = *(AG_Addr*)(safeMemReg + sizeof(int) +
          idx * sizeof(struct remapInfo));
      if (l_addr == old_l_addr) {
        oldGotpltBase = *(AG_Addr*)(safeMemReg + sizeof(int) +
            idx * sizeof(struct remapInfo) + 3 * sizeof(AG_Addr));
        newGotpltBase = *(AG_Addr*)(safeMemReg + sizeof(int) +
            idx * sizeof(struct remapInfo) + 4 * sizeof(AG_Addr));
        return newGotpltBase - oldGotpltBase;
      }
      ++idx;
    }
  }
#endif
  return 0;
}

/* wapper for mmap: mmapping to a random address */
static void *rand_mmap(size_t length, int prot, int flags,
    int fd, off_t offset) {
  void *rand_base = 0;
  do{
    __asm__ __volatile__ (
        "push %%rax \n\t"
        "push %%rbx \n\t"
        "rdrand %%eax \n\t"
        "mov $0x7f00000000, %%rbx \n\t"
        "add %%rbx, %%rax \n\t"
        "shl $12, %%rax \n\t"
        "mov $0x7fffffffff000, %%rbx \n\t"
        "and %%rbx, %%rax \n\t"
        "mov %%rax, %0 \n\t"
        "pop %%rbx \n\t"
        "pop %%rax \n\t"
        :"=r" (rand_base)
        :
        : "%rax", "%rbx"
        );
  } while (rand_base != __mmap(rand_base,
        length,
        prot,
        flags,
        fd,
        offset));
  return rand_base;
}

/* print clock cycles*/
static void print_clock_cycle(hp_timing_t time) {
  char buf[30];
  HP_TIMING_PRINT (buf, sizeof (buf), time);
  _dl_debug_printf ("@ time consumed: %s \n", buf);
}
/* print time*/
static void print_time_now(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  _dl_debug_printf ("time in microseconds: %lu\n", 1000000 * tv.tv_sec + tv.tv_usec);
}
/*********************************/
#endif
