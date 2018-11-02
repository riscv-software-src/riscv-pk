// See LICENSE for license details.

#ifndef _RISCV_UARTLR_H
#define _RISCV_UARTLR_H

#include <stdint.h>

extern volatile unsigned int *uart_litex;

void uart_litex_putchar(uint8_t ch);
int uart_litex_getchar();
void query_uart_litex(uintptr_t dtb);

#endif
