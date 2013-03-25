// See LICENSE for license details.

#ifndef _RISCV_FRONTEND_H
#define _RISCV_FRONTEND_H

#include <machine/syscall.h>

sysret_t frontend_syscall(long n, long a0, long a1, long a2, long a3);

#endif
