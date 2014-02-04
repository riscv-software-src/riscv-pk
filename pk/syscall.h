// See LICENSE for license details.

#ifndef _PK_SYSCALL_H
#define _PK_SYSCALL_H

#include <machine/syscall.h>

#define IS_ERR_VALUE(x) ((unsigned long)(x) >= (unsigned long)-4096)
#define ERR_PTR(x) ((void*)(long)(x))
#define PTR_ERR(x) ((long)(x))

void sys_exit(int code) __attribute__((noreturn));
long syscall(long a0, long a1, long a2, long a3, long a4, long a5, long n);

#endif
