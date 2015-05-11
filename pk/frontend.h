// See LICENSE for license details.

#ifndef _RISCV_FRONTEND_H
#define _RISCV_FRONTEND_H

#include <stdint.h>

#ifdef __riscv64
# define TOHOST_CMD(dev, cmd, payload) \
  (((uint64_t)(dev) << 56) | ((uint64_t)(cmd) << 48) | (uint64_t)(payload))
#else
# define TOHOST_CMD(dev, cmd, payload) ({ \
  if ((dev) || (cmd)) __builtin_trap(); \
  (payload); })
#endif
#define FROMHOST_DEV(fromhost_value) ((uint64_t)(fromhost_value) >> 56)
#define FROMHOST_CMD(fromhost_value) ((uint64_t)(fromhost_value) << 8 >> 56)
#define FROMHOST_DATA(fromhost_value) ((uint64_t)(fromhost_value) << 16 >> 16)

void die(int) __attribute__((noreturn));
long frontend_syscall(long n, long a0, long a1, long a2, long a3, long a4, long a5, long a6);
uint64_t tohost_sync(unsigned dev, unsigned cmd, uint64_t payload);

#endif
