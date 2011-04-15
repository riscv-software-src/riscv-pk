#ifndef _PK_H
#define _PK_H

#define USER_MEM_SIZE 0x70000000
#define USER_MAINVARS_SIZE 0x1000
#define USER_START 0x10000

#ifndef __ASSEMBLER__

#include <stdint.h>
#include <machine/syscall.h>

typedef struct
{
  long gpr[32];
  long sr;
  long epc;
  long badvaddr;
  long cause;
  long insn;
} trapframe_t;

#define panic(s,...) do { printk(s"\n", ##__VA_ARGS__); sys_exit(-1); } while(0)
#define kassert(cond) do { if(!(cond)) panic("assertion failed: "#cond); } while(0)

#ifdef __cplusplus
extern "C" {
#endif

extern int have_fp;
extern int have_vector;
int emulate_fp(trapframe_t*);
void init_fp(trapframe_t* tf);

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

void sys_exit(int code) __attribute__((noreturn));
sysret_t syscall(long a0, long a1, long a2, long a3, long n);

long load_elf(const char* fn, int* user64);

static inline void advance_pc(trapframe_t* tf)
{
  int rvc = (tf->insn & 0x3) < 0x3;
  tf->epc += rvc ? 2 : 4;
}

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

#ifdef __cplusplus
}
#endif

#endif

#endif
