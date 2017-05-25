#ifndef _RISCV_HTIF_H
#define _RISCV_HTIF_H

#include <stdint.h>
#include <stdlib.h>

#if __riscv_xlen == 64
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

void htif_console_putchar(uint8_t);
int htif_console_getchar();
void htif_poweroff() __attribute__((noreturn));
void htif_syscall(uintptr_t);
void htif_disk_read(uintptr_t addr, uintptr_t offset, size_t size);
void htif_disk_write(uintptr_t addr, uintptr_t offset, size_t size);
unsigned long htif_disk_size(void);

#endif
