#ifndef _RISCV_MIMPL_H
#define _RISCV_MIMPL_H

#include <stdint.h>

struct mimpl_ops {
  void (*console_putchar)(uint8_t ch);
  uintptr_t (*console_getchar)(void);
  void (*power_off)(void) __attribute((noreturn));
  int (*swint_pending)(void);
  void (*timer_callback)(void);
};

void mimpl_init(void) __attribute__((weak));
struct mimpl_ops* mimpl_ops(void);

#endif
