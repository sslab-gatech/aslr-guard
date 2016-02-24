#ifndef LIB_RELOC_H
#define LIB_RELOC_H

#include "elf/internal.h"

/* ag-note: data structure for saving relocation information
 * of data-access instructions */

// relocation entry for data-access instructions
struct s_rela_code_ent {
  Elf_Internal_Rela *reloc;
  struct s_rela_code_ent *next;
};

// relocatin infomation for data-access instructions
struct s_rela_code_sec {
  struct s_rela_code_ent *head;
  struct s_rela_code_ent *cur;
  size_t size;
} rela_code_sec;

bfd_vma code_start;
bfd_vma code_end;

bfd_vma rodata_start;
bfd_vma rodata_end;

// release memory
void rela_code_sec_free (void);

#endif

