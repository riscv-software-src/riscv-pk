#include "uart.h"
#include "fdt.h"

volatile uint32_t* uart;

void uart_putchar(uint8_t ch)
{
#ifdef __riscv_atomic
    int32_t r;
    do {
      __asm__ __volatile__ (
        "amoor.w %0, %2, %1\n"
        : "=r" (r), "+A" (uart[UART_REG_TXFIFO])
        : "r" (ch));
    } while (r < 0);
#else
    volatile uint32_t *tx = uart + UART_REG_TXFIFO;
    while ((int32_t)(*tx) < 0);
    *tx = ch;
#endif
}

int uart_getchar()
{
  int32_t ch = uart[UART_REG_RXFIFO];
  if (ch < 0) return -1;
  return ch;
}

void query_uart(uintptr_t dtb)
{
  uart = 0; // (void*)fdt_get_reg(dtb, "sifive,uart0");
  if (!uart) return;

  // Enable Rx/Tx channels
  uart[UART_REG_TXCTRL] = UART_TXEN;
  uart[UART_REG_RXCTRL] = UART_RXEN;
}
