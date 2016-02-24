// ag-note: this file includes features of
// 1) encrypting/decrypting function pointers; 
// 2) relacing push/pop with mov instructions;
// 3) replacing stack pointer with r15;
// 4) encrypting/decrypting vptr, etc.

#ifndef ASLR_GUARD_H
#define ASLR_GUARD_H

#include <stdbool.h>
#include <ctype.h>
#include <err.h>

#include "aslr-guard-macro.h"
#include "aslr-guard-config.h"


typedef char op_t[OP_SIZE];

// data structure for storing original assembly lines, 
// processed assembly lines, as well as some metadata
struct s_asm_data {
  char *filename;
  char *temp_filename;

  char *label_name;
  unsigned int label_id;

  char **lines;
  int lines_num;
  char **new_lines;
  int new_lines_num;
  char **func_syms;
  int func_syms_num;
  char **obj_syms;
  int obj_syms_num;
};

bool load_asm(struct s_asm_data *asm_data);
void load_syms(struct s_asm_data *asm_data);
void process_asm(struct s_asm_data *asm_data);
void flush_asm(struct s_asm_data *asm_data);
void free_asm_data(struct s_asm_data *asm_data);

// fp encoding
bool potential_func_addr_taken(char *line, struct s_asm_data *asm_data);
void encode_func_addr(char *line, struct s_asm_data *asm_data);

// fp decoding
void decode_func_addr_for_indirect_call(char *line, struct s_asm_data *asm_data);

// replace push/pop with mov
void replace_push_pop_with_mov(char *line, struct s_asm_data *asm_data);

// replace rsp/esp/sp with r15
void replace_sp_with_frame_ptr(struct s_asm_data *asm_data);

// vptr encoding
void encode_vptr(char *line, struct s_asm_data *meta);

// vptr decoding
void decode_vptr(char *line, struct s_asm_data *meta);



// utils
bool starts_with(const char *str, const char *pre);
bool opecode_with(const char *str, const char *op);
bool ends_with(const char *str, const char *pre);
bool is_effective_char(char c);
bool is_trim_char(char c);
char *trim_str(char *str);
void get_tail(char *raw, char *tail);
void get_type(char *type_raw, char *type_real);
int reg_bytes(char *reg);
void get_frame_ptr(int bytes, char *frame_ptr);
char *find_tab_or_space(char *str);
bool is_insn(char *line);
char *get_last_line(int i, struct s_asm_data *asm_data);
bool is_alphanumeric(char );
bool is_block_label(char *line);
char *get_low_half_reg(char *full);

static void _insert_new_line(struct s_asm_data *meta, char *line);
static void _insert_new_line_fmt(struct s_asm_data *meta, const char *fmt, ...);
static void _replace_cur_line(struct s_asm_data *meta, char *line);

#define _replace_cur_line_fmt(meta, ...) {        \
  meta->new_lines_num --;                         \
  free(meta->new_lines[meta->new_lines_num]);     \
  _insert_new_line_fmt(meta, ##__VA_ARGS__);      \
}

bool starts_with(const char *str, const char *pre)
{
  size_t lenpre = strlen(pre), lenstr = strlen(str);
  return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}

bool opecode_with(const char *str, const char *op)
{
  while ((str[0] == ' ' || str[0] == '\t') && str[0] != '\0')
    ++str;
  size_t lenop = strlen(op), lenstr = strlen(str);
  return lenstr < lenop ? false : strncmp(op, str, lenop) == 0;
}

bool ends_with(const char *str, const char *post)
{
  size_t lenpost = strlen(post), lenstr = strlen(str);
  return lenstr < lenpost ? false : strncmp(post,
      str + lenstr - lenpost, lenpost) == 0;
}

bool is_alphanumeric(char c) {
  if ((c >= '0' && c <= '9') ||
      (c >= 'a' && c <= 'z') ||
      (c >= 'A' && c <= 'Z'))
    return true;
  return false;
}

bool is_effective_char(char c) {
  if (is_alphanumeric(c) ||
      c == ')' || c == ':')
    return true;
  return false;
}

bool is_trim_char(char c) {
  if (c == ' ' || c == '\t' || c == '\n')
    return true;
  return false;
}

char *trim_str(char *str) {
  while(is_trim_char(str[0]))
    ++str;
  int i = strlen(str) - 1;
  while (i >= 0 && is_trim_char(str[i])) {
    str[i] = '\0';
    --i;
  }
  return str;
}

void get_tail(char *raw, char *tail) {
  while (is_trim_char(raw[0]) && raw[0] != '\0')
    ++raw;
  size_t len_raw = strlen(raw);
  char *tail_end = raw + len_raw, *tail_end1;
  tail_end1 = strchr(raw, '#');
  if (tail_end1)
    tail_end = tail_end1;
  tail_end1 = strchr(raw, '/');
  if (tail_end1 && tail_end1 < tail_end)
    tail_end = tail_end1;
  tail_end1 = strchr(raw, ';');
  if (tail_end1 && tail_end1 < tail_end)
    tail_end = tail_end1;
  strncpy(tail, raw, tail_end - raw);
  tail[tail_end - raw] = '\0';
  int i = strlen(tail) - 1;
  while (!is_effective_char(tail[i]) && i >= 0) {
    tail[i] = '\0';
    --i;
  }
}

void get_type(char *type_raw, char *type_real) {
  size_t len = find_tab_or_space(type_raw) - type_raw;
  strncpy(type_real, type_raw, len);
  type_real[len] = '\0';
}

int reg_bytes(char *reg) {
  if (reg[0] == '%')
    ++reg;
  size_t len = strlen(reg);
  char a = reg[0];
  char b = reg[len - 1];
  if (b == 'l')
    return 1;
  else if (b == 'b')
    return 1;
  else if (b == 'w')
    return 2;
  else if (b == 'd')
    return 4;
  else if (a == 'r')
    return 8;
  else if (a == 'e')
    return 4;
  else if (len == 2)
    return 2;
  else
    return 8;
}

void get_frame_ptr(int bytes, char *fp) {
  switch (bytes) {
    case 1:
      sprintf(fp, "%sb", FRAME_PTR);
      break;
    case 2:
      sprintf(fp, "%sw", FRAME_PTR);
      break;
    case 4:
      sprintf(fp, "%sd", FRAME_PTR);
      break;
    default:
      sprintf(fp, "%s", FRAME_PTR);
      break;
  }
}

// NOTE. return readonly string
static
char const *_get_frame_ptr(int bytes) {
  switch (bytes) {
    case 1:
      return "%" FRAME_PTR "b";
    case 2:
      return "%" FRAME_PTR "w";
    case 4:
      return "%" FRAME_PTR "d";
    default:
      return "%" FRAME_PTR;
  }
}

char *find_tab_or_space(char *str) {
  char *idx = strchr(str, '\t');
  if (!idx)
    idx = strchr(str, ' ');
  return idx;
}

bool is_insn(char *line) {
  char *insn = malloc(strlen(line) + 1);
  get_tail(line, insn);
  //if ((line[0] == '\t' || line[0] == ' ') &&
  // #ifdef AG_MULTITHREAD
  if ((!strstr(insn, ":") || strstr(insn, "%fs:")) &&
      // #else
      //       if (!strstr(insn, ":") &&
      // #endif
    !opecode_with(line, ".") &&
      !opecode_with(line, "#") &&
      !opecode_with(line, "/") &&
      !ends_with(line, "#_volatile_")) {
      free(insn);
      return true;
      }
      free(insn);
      return false;
      }

      bool is_block_label(char *line) {
      int i = 0;
      while (is_alphanumeric(line[i])) {
      ++i;
      }
      if (i > 0 && line[i] == ':')
        return true;
      return false;
      }

char *get_last_line(int i, struct s_asm_data *asm_data) {
  while (i > 0) {
    if (is_insn(asm_data->lines[i - 1]))
      return asm_data->lines[i - 1];
    else
      --i;
  }
  return "";
}

static
bool _in_func_syms(char *needle, struct s_asm_data *meta) {
  int i;
  for (i = 0; i < meta->func_syms_num; i ++) {
    if (!strcmp(needle, meta->func_syms[i]))
      return true;
  }
  return false;
}

static
bool _in_obj_syms(char *needle, struct s_asm_data *meta) {
  int i;
  for (i = 0; i < meta->obj_syms_num; i ++) {
    if (!strcmp(needle, meta->obj_syms[i]))
      return true;
  }
  return false;
}

static inline
char *_skip_spaces(char *line) {
  return line + strspn(line, " \t\n");
}

static inline
char *_skip_seps(char *line, char *seps) {
  return line + strspn(line, seps);
}

static
void get_opcode(char *iter, op_t op) {
  gas_assert(iter);

  // move to opcode
  iter = _skip_spaces(iter);

  // copy opcode
  size_t i = 0;
  while (isalpha(*iter)) {
    op[i] = *iter;
    iter ++;
    i ++;
  }
  op[i] = '\0';
}

static
char get_optype(char *line, size_t type_chr_idx) {
  op_t op;
  get_opcode(line, op);
  if (strlen(op) == type_chr_idx)
    return ' ';
  return op[type_chr_idx];
}

static
char get_optype_bytes(char *line, size_t type_chr_idx) {
  switch (get_optype(line, type_chr_idx)) {
    case 'l': return 4;
    case 'w': return 2;
    case 'b': return 1;
    default: return 8;
  }
}

static
void get_oparand(char *iter, int num, op_t op) {
  gas_assert(iter && (num == 1 || num == 2));

  // move to opcode
  iter = _skip_spaces(iter);

  // skip opcode
  while (isalpha(iter[0]))
    iter ++;

  while ((--num) >= 0) {
    // move to oprand
    iter = _skip_seps(iter, " ,\t\n");

    // copy to op
    unsigned int depth = 0;
    size_t i;
    for (i = 0; i < OP_SIZE; i ++) {
      op[i] = *iter;
      if (*iter == '(')
        depth ++;
      if (*iter == ')')
        depth --;
      if (depth == 0 && (*iter == '\0' || strchr(" ,\t\n#;/", *iter))) {
        op[i] = '\0';
        break;
      }
      iter ++;
    }
    gas_assert(i < OP_SIZE);
  }
}

// check if the insturction is taking function address
bool potential_func_addr_taken(char *line, struct s_asm_data *asm_data) {
  // interested in mov & lea
  if (!opecode_with(line, "mov")
      && !opecode_with(line, "lea")) {
    return false;
  }

  // only if rip-relatively addressed
  if (strstr(line, "( %rip ),") || !strstr(line, "(%rip),"))
    return false;

  op_t op1;
  get_oparand(line, 1, op1);

  // randomized func ptr
  if (starts_with(op1, "_dl_runtime_resolve"))
    return false;

  // drop '(%rip)'
  op1[strlen(op1) - strlen("(%rip)")] = '\0';

  char *at_idx = strstr(op1, "@");
  if (at_idx) {
    // gcc: if it is for global function, there will be a space inserted 
    // between the function name and "@GOTPCREL"
    if (*(char *)(at_idx - 1) == ' ') {
      return true;
    }
    else
      at_idx[0] = '\0';
  }
  // care only func ptr
  if (_in_func_syms(op1, asm_data))
    return true;
  if (_in_obj_syms(op1, asm_data))
    return false;

  if (starts_with(op1, ".")
      || strchr(op1, '+'))
    return false;
  // NOTE. unable to differenciate global function and global object 
  // of handwritten assembly (relatively rare)
  // return true as long as it is global verify it in linker
  if (at_idx)
    if (starts_with(at_idx + 1, "GOTPCREL"))
      return true;

  // more heuristics
  if (starts_with(op1, "__GI__")
      || starts_with(op1, "_dl_"))
    return true;

  // FIXME: it is supposed to return true by default, to be concervative
  return false;
}

char *get_low_half_reg(char *full) {
  if (strstr(full, "rax")) 
    return "%eax";
  else if (strstr(full, "rbx")) 
    return "%ebx";
  else if (strstr(full, "rcx")) 
    return "%ecx";
  else if (strstr(full, "rdx")) 
    return "%edx";
  else if (strstr(full, "rsi")) 
    return "%esi";
  else if (strstr(full, "rdi")) 
    return "%edi";
  else if (strstr(full, "rbp")) 
    return "%ebp";
  else if (strstr(full, "rsp")) 
    return "%esp";
  else if (strstr(full, "r8")) 
    return "%r8d";
  else if (strstr(full, "r9")) 
    return "%r9d";
  else if (strstr(full, "r10")) 
    return "%r10d";
  else if (strstr(full, "r11")) 
    return "%r11d";
  else if (strstr(full, "r12")) 
    return "%r12d";
  else if (strstr(full, "r13")) 
    return "%r13d";
  else if (strstr(full, "r14")) 
    return "%r14d";
  else if (strstr(full, "r15")) 
    return "%r15d";
  else {
    aslr_not_reachable();
    return NULL;
  }
}


#define _insert_hdr(meta) \
  _insert_new_line_fmt(meta, "#> ================ by %s():%d", __FUNCTION__, __LINE__)
#define _insert_ftr(meta) \
  _insert_new_line_fmt(meta, "#< ================ by %s():%d", __FUNCTION__, __LINE__)

#define _EMITI(fmt, ...) _insert_new_line_fmt(meta, "\t"fmt, ##__VA_ARGS__)
#define _EMITL(fmt, ...) _insert_new_line_fmt(meta, fmt, ##__VA_ARGS__)
#define _EMITV(fmt, ...) _insert_new_line_fmt(meta, "\t"fmt" #_volatile_", ##__VA_ARGS__)

// insert function pointer encryption routine right after
// address-taking instruction
void encode_func_addr(char *line, struct s_asm_data *meta) {
  op_t op2;
  get_oparand(line, 2, op2);

  char frame_ptr[10] = {0};
  get_frame_ptr(reg_bytes(op2), frame_ptr);

  _insert_hdr(meta);
  // TODO: check if the pointer is already encrypted (see NOTE.todo)
  _EMITV("pushq %%" FRAME_PTR);
  _EMITI("movq  %%gs:0x100000, %%" FRAME_PTR);
  _EMITI("mov   %s, %%gs:0x100000(%%" FRAME_PTR ")", op2); //save real fp
#ifdef USE_MAGIC_CODE
  _EMITV("pushq %%rax");
  _EMITV("pushq %%rbx");
  _EMITI("movq  %%" FRAME_PTR ", %%rax");
  _EMITI("movabs $" AG_MAGIC_CODE ", %%rbx");
  _EMITI("or    %%rbx, %%" FRAME_PTR );
  _EMITI("movq  %%" FRAME_PTR ", %%gs:0x100008(%%rax)"); //save encoded fp
  _EMITV("popq %%rbx");
  _EMITV("popq %%rax");
#elif USE_NONCE_DEVRAND
  _EMITV("pushq %%rdi");
  _EMITV("pushq %%rsi");
  _EMITV("pushq %%rdx");
  _EMITV("pushq %%rax");
  _EMITV("pushq %%rcx");                  // syscall may destropy rcx and r11
  _EMITV("pushq %%r11");
  _EMITI("movq  %%gs:0x100008, %%rdi");   // load file handler of /dev/urandom
  _EMITI("movq  %%gs:0x100010, %%rsi");   // location (%gs:0x10001c) for saving random value
  _EMITI("movq  $4, %%rdx");              // read 4 bytes
  _EMITI("mov  $0, %%eax");               // syscall number for "read"
  _EMITI("syscall");                      // do syscall
  _EMITI("movq  -4(%%rsi), %%rax");       // load random value
  _EMITI("movq  %%" FRAME_PTR ", %%rdi"); // save current location
  _EMITI("movq  %%rax, %%gs:0x100008(%%rdi)"); //save 4-byte nonce
  _EMITI("or    %%rax, %%" FRAME_PTR );
  _EMITV("popq %%r11");
  _EMITV("popq %%rcx");
  _EMITV("popq %%rax");
  _EMITV("popq %%rdx");
  _EMITV("popq %%rsi");
  _EMITV("popq %%rdi");
#elif USE_NONCE_RDRAND
  _EMITV("pushq %%rdi");
  _EMITV("pushq %%rax");
  _EMITI("rdrand %%eax");                 // load random value
  _EMITI("shl $32, %%rax");              // shift random value to high 32 bits
  _EMITI("movq  %%" FRAME_PTR ", %%rdi"); // save current location
  _EMITI("movq  %%rax, %%gs:0x100008(%%rdi)"); //save 4-byte nonce
  _EMITI("or    %%rax, %%" FRAME_PTR );   // generate encrypted fp
  _EMITV("popq %%rax");
  _EMITV("popq %%rdi");
#else
  _EMITI("movq  %%" FRAME_PTR ", %%gs:0x100008(%%" FRAME_PTR ")"); //save encoded fp
#endif
  _EMITI("addl  $0x10, %%gs:0x100000");
  _EMITI("mov   %%%s, %s", frame_ptr, op2);
  _EMITV("popq  %%" FRAME_PTR);
  _insert_ftr(meta);
}

void decode_func_addr_for_indirect_call(char *line, struct s_asm_data *meta) {
  op_t op1;
  get_oparand(line, 1, op1);

  // drop '*'
  gas_assert(op1[0] == '*');
  char *reg = op1 + 1;
  // drop cur line
  _replace_cur_line_fmt(meta, "#-%s", line);

  // fp in register
  if (starts_with(op1, "*%") && !strstr(op1, "%fs")) {
#if defined(USE_NONCE_DEVRAND) || defined(USE_NONCE_RDRAND)
    char *halfReg = get_low_half_reg(reg);
#endif
    if (opecode_with(line, "call")) {
      _insert_hdr(meta);

#ifdef USE_MAGIC_CODE
      _EMITV("pushq %%" FRAME_PTR); //assume FRAME_PTR will never used in indirect call
      _EMITI("movabs $" AG_MAGIC_CODE ", %%" FRAME_PTR);
      _EMITI("xor  %%" FRAME_PTR ", %s", reg);
      _EMITV("popq %%" FRAME_PTR);
#elif defined(USE_NONCE_DEVRAND) || defined(USE_NONCE_RDRAND)
      _EMITI("xor  %%gs:0x100008(%s), %s", halfReg, reg);
#endif
      _EMITI("call  *%%gs:0x100000(%s)", reg);
      _insert_ftr(meta);

    }
    else if (opecode_with(line, "jmp")) {
      _insert_hdr(meta);
#ifdef USE_MAGIC_CODE
      _EMITV("pushq %%" FRAME_PTR); //assume FRAME_PTR will never used in indirect call
      _EMITI("movabs $" AG_MAGIC_CODE ", %%" FRAME_PTR);
      _EMITI("xor  %%" FRAME_PTR ", %s", reg);
      _EMITV("popq %%" FRAME_PTR);
#elif defined(USE_NONCE_DEVRAND) || defined(USE_NONCE_RDRAND)
      _EMITI("xor  %%gs:0x100008(%s), %s", halfReg, reg);
#endif
      _EMITI("jmp   *%%gs:0x100000(%s)", reg);
      _insert_ftr(meta);
    } else {
      aslr_not_reachable();
    }
  }
  // fp in memory
  else if (starts_with(op1, "*") && (ends_with(op1, ")") || strstr(op1, "%fs"))) {
    //reg can be r10, as its value is saved
    //gas_assert(!strstr(reg, "r10"));
    if (opecode_with(line, "call")) {
      _insert_hdr(meta);
      _EMITV("pushq %%r10");
      _EMITI("mov%c %s, %%r10", get_optype(line, strlen("call")), reg);
#ifdef USE_MAGIC_CODE
      _EMITV("pushq %%rax");
      _EMITI("movabs $" AG_MAGIC_CODE ", %%rax");
      _EMITI("xor  %%rax, %%r10");
      _EMITV("popq %%rax");
#elif defined(USE_NONCE_DEVRAND) || defined(USE_NONCE_RDRAND)
      _EMITI("xor  %%gs:0x100008(%r10d), %r10");
#endif
      _EMITI("movq  %%gs:0x100000(%%r10), %%r10");
      _EMITV("movq  %%r10, -0x8(%%rsp)");
      _EMITV("popq  %%r10");
      _EMITV("call  *-0x10(%%rsp)");
      _insert_ftr(meta);
    }
    else if(opecode_with(line, "jmp")) {
      _insert_hdr(meta);
      _EMITV("pushq %%r10");
      _EMITI("mov%s %s, %%r10", get_optype(line, strlen("jmp")), reg);
#ifdef USE_MAGIC_CODE
      _EMITV("pushq %%rax");
      _EMITI("movabs $" AG_MAGIC_CODE ", %%rax");
      _EMITI("xor  %%rax, %%r10");
      _EMITV("popq %%rax");
#elif defined(USE_NONCE_DEVRAND) || defined(USE_NONCE_RDRAND)
      _EMITI("xor  %%gs:0x100008(%r10d), %r10");
#endif
      _EMITI("movq  %%gs:0x100000(%%r10), %%r10");
      _EMITV("movq  %%r10, -0x8(%%rsp)");
      _EMITV("popq  %%r10");
      _EMITV("jmp   *-0x10(%%rsp)");
      _insert_ftr(meta);
    }
    else {
      aslr_not_reachable();
    }
  }
  else {
    err(1, "Cannot recognize %s\n", line);
    aslr_not_reachable();
  }
}

// encode vtable pointer
void encode_vptr(char *line, struct s_asm_data *meta) {
  // drop cur line
  _replace_cur_line_fmt(meta, "#-%s", line);

  op_t op1, op2;
  get_oparand(line, 1, op1);
  get_oparand(line, 2, op2);

  char frame_ptr[10] = {0};
  get_frame_ptr(reg_bytes(op1), frame_ptr);

  _insert_hdr(meta);
  _EMITV("pushq %%" FRAME_PTR);
  _EMITI("movq  %%gs:0x100000, %%" FRAME_PTR);
  _EMITI("mov   %s, %%gs:0x100000(%%" FRAME_PTR ")", op1);
  _EMITI("movq  %%" FRAME_PTR ", %%gs:0x100008(%%" FRAME_PTR ")");
  _EMITI("addl  $0x10, %%gs:0x100000");
  _EMITI("mov   %%%s, %s", frame_ptr, op2);
  _EMITV("popq  %%" FRAME_PTR);
  _insert_ftr(meta);
}

// decode vtable pointer
void decode_vptr(char *line, struct s_asm_data *meta) {
  op_t op2;
  get_oparand(line, 2, op2);

  _insert_hdr(meta);
  _EMITI("movq  %%gs:0x100000(%s), %s", op2, op2);
  _insert_ftr(meta);

}

void replace_push_pop_with_mov(char *line, struct s_asm_data *meta)
{
  // drop cur line
  _replace_cur_line_fmt(meta, "#-%s", line);

  op_t op1;
  get_oparand(line, 1, op1);

  bool is_mem = (op1[0] == '(');

  _insert_hdr(meta);
  if (opecode_with(line, "push")) {
    char op_type = get_optype(line, strlen("push"));
    char op_type_bytes = get_optype_bytes(line, strlen("push"));
    if (is_mem) {
      _EMITV("pushq %r10");
      _EMITI("xor   %r10, %r10");
      _EMITI("mov%c %s, %%r10", op_type, op1);
      strcpy(op1, "%r10");
    }
    _EMITI("sub   $%d, %%rsp", op_type_bytes);
    _EMITI("mov%c %s, (%%rsp)", op_type, op1);
    if (is_mem) {
      _EMITV("popq  %r10");
    }
  } else if (opecode_with(line, "pop")) {
    char op_type = get_optype(line, strlen("pop"));
    char op_type_bytes = get_optype_bytes(line, strlen("pop"));
    if (is_mem) {
      _EMITV("pushq %r10");
      _EMITI("xor   %r10, %r10");
      _EMITI("mov%c  (%%rsp), %r10", op_type);
      _EMITI("mov%s  %%r10, %s", op_type_bytes, op1);
      _EMITV("popq   %r10");
    } else {
      _EMITI("mov%c  (%%rsp), %s", op_type, op1);
      _EMITI("add    $%d, %rsp", op_type_bytes);
    }
  } else {
    aslr_not_reachable();
  }
  _insert_ftr(meta);
}

static
void _str_replace(char **in, char const *from, char const *to) {
  char *strp = *in;
  char *iter = strstr(strp, from);
  if (!iter)
    return;

  char *out = malloc(strlen(strp) - strlen(from) + strlen(to) + 1);
  memcpy(out, strp, (iter - strp));
  sprintf(out + (iter - strp), "%s%s", to, iter + strlen(from));

  free(strp);
  *in = out;

  _str_replace(in, from, to);
}

void replace_sp_with_frame_ptr(struct s_asm_data *meta) {
  int i;
  for (i = 0; i < meta->new_lines_num; i++) {
    if (!is_insn(meta->new_lines[i]))
      continue;
    _str_replace(&meta->new_lines[i], "%rsp", _get_frame_ptr(8));
    _str_replace(&meta->new_lines[i], "%esp", _get_frame_ptr(4));
    _str_replace(&meta->new_lines[i], "%sp" , _get_frame_ptr(2));
    _str_replace(&meta->new_lines[i], "%spl", _get_frame_ptr(1));
  }
}

// load data from assembly file to lines in asm_data
bool load_asm(struct s_asm_data *meta) {
  FILE *fp = fopen(meta->filename, "rb");
  if (!fp)
    err(EXIT_FAILURE, "failed to open %s", meta->filename);

  // initial buf size
  size_t buf_len = 10000;
  meta->lines = malloc(sizeof(char *) * buf_len);

  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  size_t count = 0;
  while ((read = getline(&line, &len, fp)) != -1) {
    gas_assert(read < MAX_ASM_LINE);
    if (count >= buf_len) {
      buf_len *= 2;
      meta->lines = realloc(meta->lines, sizeof(char *) * buf_len);
      gas_assert(meta->lines);
    }
    // drop '\n'
    gas_assert(line[read - 1] == '\n');
    line[read - 1] = '\0';

    char *line_tmp = line;
    if (is_block_label(line)) {
      // check if it is followed by an insn
      char *possible_insn = strchr(line, ':') + 1;
      if (is_insn(possible_insn)) {
        char *label = strdup(line);
        label[possible_insn - line] = '\0';
        meta->lines[count] = label;
        count ++;
        line_tmp = possible_insn;
      }
    }
    meta->lines[count] = strdup(line_tmp);
    count ++;
  }
  meta->lines_num = count;

  free(line);
  fclose(fp);

  // init meta info
  meta->label_name = strdup(meta->filename);
  meta->label_id = 0;

  _str_replace(&meta->label_name, "/", "_");
  _str_replace(&meta->label_name, ".", "_");

  return true;
}

// collect all function/object symbols
//  - objects sybmols to opt out unecessary referencing
void load_syms(struct s_asm_data *asm_data) {
  // not initialized
  if (asm_data == NULL
      || asm_data->lines == NULL
      || asm_data->lines_num == 0)
    return;

  asm_data->func_syms = malloc(asm_data->lines_num * sizeof(char *));
  asm_data->func_syms_num = 0;
  asm_data->obj_syms = malloc(asm_data->lines_num * sizeof(char *));
  asm_data->obj_syms_num = 0;

  int i;
  for (i = 0; i < asm_data->lines_num; ++i) {
    char *line = asm_data->lines[i];
    if (opecode_with(line, ".type")) {
      char *start_idx = strstr(line, ".type") + 6;
      while (is_trim_char(start_idx[0]))
        ++start_idx;
      char *end_idx = strchr(line, ',');
      if (!end_idx)
        continue;
      // get sym
      char *sym = malloc(end_idx - start_idx + 1);
      strncpy(sym, start_idx, end_idx - start_idx);
      sym[end_idx - start_idx] = '\0';
      sym = trim_str(sym);
      if (ends_with(line, "function")) {
        asm_data->func_syms[asm_data->func_syms_num] = sym;
        ++asm_data->func_syms_num;
        continue;
      }
      else if (ends_with(line, "object")) {
        asm_data->obj_syms[asm_data->obj_syms_num] = sym;
        ++asm_data->obj_syms_num;
        continue;
      }

      // NOTE. many variants to specify type description, # , STT_*
      gas_assert(!strcasestr(line, "function")
          && !strcasestr(line, "object"));
    }
  }
}

static inline
void _insert_new_line(struct s_asm_data *meta, char *line) {
  // NOTE. allow a bit of inline replacement
  char *buf = malloc(MAX_ASM_LINE);
  strncpy(buf, line, MAX_ASM_LINE);

  meta->new_lines[meta->new_lines_num] = buf;
  meta->new_lines_num ++;
}

static inline
void _replace_cur_line(struct s_asm_data *meta, char *line) {
  free(meta->new_lines[meta->new_lines_num]);
  meta->new_lines_num --;
  _insert_new_line(meta, line);
}

  static
void _insert_new_line_fmt(struct s_asm_data *meta, const char *fmt, ...)
{
  va_list args;
  char buf[MAX_ASM_LINE];

  va_start(args, fmt);
  (void) vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  _insert_new_line(meta, buf);
}

// process asm: safe stack, code pointer encode/decode
void process_asm(struct s_asm_data *asm_data) {
  gas_assert(asm_data);
  asm_data->new_lines = malloc((asm_data->lines_num) * 2 * sizeof(char *));
  gas_assert(asm_data->new_lines);
  asm_data->new_lines_num = 0;

  //bool isVCall = false;
  // for each inst
  int i;
  for (i = 0; i < asm_data->lines_num; ++i) {
    char *line = asm_data->lines[i];
    _insert_new_line(asm_data, line);
#ifdef AG_SAFE_STACK
    // preparing for temprary unsafe stack
    if (starts_with(line, "_dl_start:")) {
      _insert_new_line_fmt(asm_data,
          "\tleaq\t-0x10000(%%rsp), %%%s #_volatile_",
          FRAME_PTR);
    }
#endif

    // nothing to rewrite
    if (!is_insn(line))
      continue;

#ifdef AG_ENCODE_CP
    // anything inferring based on %rip
    if (potential_func_addr_taken(line, asm_data)) {
      // replace address-taking insts with a call to __aslr_encptr
      encode_func_addr(line, asm_data);
    }
    // indirect call/jmp
    else if (opecode_with(line, "call")
        || (opecode_with(line, "jmp") && ends_with(line, "_tail_"))) {

      op_t op1;
      get_oparand(line, 1, op1);

      // skip gs relative accesses
      if (starts_with(op1, "%gs"))
        continue;
      // if not indirect, so direct
      if (starts_with(op1, "*")) {
        //skip virtual call
        /*
           if (isVCall) {
           _replace_cur_line_fmt(asm_data, "%s #_volatile_", line);
           isVCall = false;
           continue;
           }
           */
        decode_func_addr_for_indirect_call(line, asm_data);
      }
      // remove marks for vptr def-use inserted by gcc
      else if (strstr(op1, "AG_VPTR_DEF")) {
        _replace_cur_line_fmt(asm_data, "#-%s", line);
      }
      else if (strstr(op1, "AG_VPTR_USE")) {
        _replace_cur_line_fmt(asm_data, "#-%s", line);
      }
      else
        continue;
    }
    // vptr encoding/decoding
    else if (opecode_with(line, "mov") && 
        (strstr(line, "->_vptr.") || strstr(line, "._vptr."))) {
      if (!strstr(line, "), %") && !strstr(line, "),%") &&
          strstr(line, ", this")) {//def vptr
        encode_vptr(line, asm_data);
      }
      else if (strstr(line, "), %") || strstr(line, "),%")){ //use vptr
        decode_vptr(line, asm_data);
        //isVCall = true;
      }
    }
#endif

#ifdef AG_SAFE_STACK
    // anything that might change %rsp
    if (opecode_with(line, "push") || opecode_with(line, "pop")) {
      // skip incorrect mapping
      if (opecode_with(line, "popcnt"))
        continue;
      replace_push_pop_with_mov(line, asm_data);
    }
#endif
  }
#ifdef AG_SAFE_STACK
  replace_sp_with_frame_ptr(asm_data);
#endif
}

static
void _dump_output(char *pn, struct s_asm_data *asm_data) {
  FILE *fp = fopen(pn, "w");
  if (!fp)
    err(1, "failed to open %s", pn);

  int i;
  for (i = 0; i < asm_data->new_lines_num; ++i) {
    fprintf(fp, "%s\n", asm_data->new_lines[i]);
  }
  fclose(fp);
}

// output transformed assembly code to the same file
void flush_asm(struct s_asm_data *asm_data) {
  _dump_output(asm_data->filename, asm_data);
  if (asm_data->temp_filename) {
    _dump_output(asm_data->temp_filename, asm_data);
  }
}

// release resource
void free_asm_data(struct s_asm_data *asm_data) {
  int i;
  for (i = 0; i < asm_data->lines_num; ++i) {
    if (asm_data->lines[i])
      free(asm_data->lines[i]);
    asm_data->lines[i] = NULL;
  }
  for (i = 0; i < asm_data->new_lines_num; ++i) {
    if (asm_data->new_lines[i]) {
      free(asm_data->new_lines[i]);
      asm_data->new_lines[i] = NULL;
    }
  }
  for (i = 0; i < asm_data->func_syms_num; ++i) {
    if (asm_data->func_syms[i]) {
      free(asm_data->func_syms[i]);
      asm_data->func_syms[i] = NULL;
    }
  }
  for (i = 0; i < asm_data->obj_syms_num; ++i) {
    if (asm_data->obj_syms[i]) {
      free(asm_data->obj_syms[i]);
      asm_data->obj_syms[i] = NULL;
    }
  }

  free(asm_data->filename);
  free(asm_data->label_name);
  free(asm_data->temp_filename);
}

#undef _EMITI
#undef _EMITL

#endif
