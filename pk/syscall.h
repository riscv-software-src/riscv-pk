// See LICENSE for license details.

#include <machine/syscall.h>

void sys_exit(int code) __attribute__((noreturn));
sysret_t syscall(long a0, long a1, long a2, long a3, long a4, long a5, long n);
