#ifndef _PK_H
#define _PK_H

typedef struct
{
  long gpr[32];
  long sr;
  long epc;
  long badvaddr;
  long cr29;
} trapframe_t;

#define USER_MEM_SIZE 0x70000000
#define USER_MAINVARS_SIZE 0x1000
#define USER_START 0x10000

#define panic(s,...) do { printk(s"\n", ##__VA_ARGS__); sys_exit(-1); } while(0)
#define kassert(cond) do { if(!(cond)) panic("assertion failed: "#cond); } while(0)

#ifdef __cplusplus
extern "C" {
#endif

void printk(const char* s, ...);
void init_tf(trapframe_t*, long pc, long sp);
void pop_tf(trapframe_t*);
void dump_tf(trapframe_t*);

void unhandled_trap(trapframe_t*);
void handle_syscall(trapframe_t*);
void handle_breakpoint(trapframe_t*);
void boot();

void sys_exit(int code) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif
