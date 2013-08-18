// See LICENSE for license details.

#ifndef _PK_H
#define _PK_H

#ifndef __ASSEMBLER__

#include <stdint.h>
#include <string.h>

typedef struct
{
  long gpr[32];
  long sr;
  long epc;
  long badvaddr;
  long cause;
  long insn;
} trapframe_t;

#define panic(s,...) do { do_panic(s"\n", ##__VA_ARGS__); } while(0)
#define kassert(cond) do { if(!(cond)) kassert_fail(""#cond); } while(0)
void do_panic(const char* s, ...) __attribute__((noreturn));
void kassert_fail(const char* s) __attribute__((noreturn));
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define CLAMP(a, lo, hi) MIN(MAX(a, lo), hi)
#define ROUNDUP(a, b) ((((a)-1)/(b)+1)*(b))
#define ROUNDDOWN(a, b) ((a)/(b)*(b))

#ifdef __cplusplus
extern "C" {
#endif

extern int have_fp;
extern int have_vector;
extern uint32_t mem_mb;
int emulate_fp(trapframe_t*);
void init_fp(trapframe_t* tf);

int emulate_int(trapframe_t*);

void printk(const char* s, ...);
void init_tf(trapframe_t*, long pc, long sp, int user64);
void pop_tf(trapframe_t*);
void dump_tf(trapframe_t*);

void unhandled_trap(trapframe_t*);
void handle_misaligned_load(trapframe_t*);
void handle_misaligned_store(trapframe_t*);
void handle_fault_load(trapframe_t*);
void handle_fault_store(trapframe_t*);
void boot();

typedef struct {
  int elf64;
  int phent;
  int phnum;
  size_t user_min;
  size_t entry;
  size_t brk_min;
  size_t brk;
  size_t brk_max;
  size_t mmap_max;
  size_t stack_bottom;
  size_t phdr;
  size_t phdr_top;
  size_t stack_top;
} elf_info;

extern elf_info current;

void load_elf(const char* fn, elf_info* info);

static inline int insn_len(long insn)
{
  return (insn & 0x3) < 0x3 ? 2 : 4;
}

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

#ifdef __cplusplus
}
#endif

#endif

#endif
