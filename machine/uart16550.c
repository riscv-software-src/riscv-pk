// See LICENSE for license details.

#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include "uart16550.h"
#include "fdt.h"

volatile uint8_t* uart16550;
// some devices require a shifted register index
// (e.g. 32 bit registers instead of 8 bit registers)
static uint32_t uart16550_reg_shift;
static uint32_t uart16550_clock = 1843200;   // a "common" base clock

#define UART_REG_QUEUE     0    // rx/tx fifo data
#define UART_REG_DLL       0    // divisor latch (LSB)
#define UART_REG_IER       1    // interrupt enable register
#define UART_REG_DLM       1    // divisor latch (MSB) 
#define UART_REG_FCR       2    // fifo control register
#define UART_REG_LCR       3    // line control register
#define UART_REG_MCR       4    // modem control register
#define UART_REG_LSR       5    // line status register
#define UART_REG_MSR       6    // modem status register
#define UART_REG_SCR       7    // scratch register
#define UART_REG_STATUS_RX 0x01
#define UART_REG_STATUS_TX 0x20

// We cannot use the word DEFAULT for a parameter that cannot be overridden due to -Werror
#ifndef UART_DEFAULT_BAUD
#define UART_DEFAULT_BAUD  38400
#endif

void uart16550_putchar(uint8_t ch)
{
  while ((uart16550[UART_REG_LSR << uart16550_reg_shift] & UART_REG_STATUS_TX) == 0);
  uart16550[UART_REG_QUEUE << uart16550_reg_shift] = ch;
}

int uart16550_getchar()
{
  if (uart16550[UART_REG_LSR << uart16550_reg_shift] & UART_REG_STATUS_RX)
    return uart16550[UART_REG_QUEUE << uart16550_reg_shift];
  return -1;
}

struct uart16550_scan
{
  int compat;
  uint64_t reg;
  uint32_t reg_offset;
  uint32_t reg_shift;
  uint32_t clock_freq;
  uint32_t baud;
};

static void uart16550_open(const struct fdt_scan_node *node, void *extra)
{
  struct uart16550_scan *scan = (struct uart16550_scan *)extra;
  memset(scan, 0, sizeof(*scan));
  scan->baud = UART_DEFAULT_BAUD;
}

static void uart16550_prop(const struct fdt_scan_prop *prop, void *extra)
{
  struct uart16550_scan *scan = (struct uart16550_scan *)extra;
  // For the purposes of the boot loader, the 16750 is a superset of what 16550a provides
  if (!strcmp(prop->name, "compatible") && ((fdt_string_list_index(prop, "ns16550a") != -1) || (fdt_string_list_index(prop, "ns16750") != -1))) {
    scan->compat = 1;
  } else if (!strcmp(prop->name, "reg")) {
    fdt_get_address(prop->node->parent, prop->value, &scan->reg);
  } else if (!strcmp(prop->name, "reg-shift")) {
    scan->reg_shift = fdt_get_value(prop, 0);
  } else if (!strcmp(prop->name, "reg-offset")) {
    scan->reg_offset = fdt_get_value(prop, 0);
  } else if (!strcmp(prop->name, "current-speed")) {
    // This is the property that Linux uses
    scan->baud = fdt_get_value(prop, 0);
  } else if (!strcmp(prop->name, "clock-frequency")) {
    scan->clock_freq = fdt_get_value(prop, 0);
  }
}

static void uart16550_done(const struct fdt_scan_node *node, void *extra)
{
  uint32_t clock_freq;
  struct uart16550_scan *scan = (struct uart16550_scan *)extra;
  if (!scan->compat || !scan->reg || uart16550) return;

  if (scan->clock_freq != 0)
    uart16550_clock = scan->clock_freq;
  // if device tree doesn't supply a clock, fallback to default clock of 1843200

  // Check for divide by zero
  uint32_t divisor = uart16550_clock / (16 * (scan->baud ? scan->baud : UART_DEFAULT_BAUD));
  // If the divisor is out of range, don't assert, set the rate back to the default
  if (divisor >= 0x10000u)
    divisor = uart16550_clock / (16 * UART_DEFAULT_BAUD);

  uart16550 = (void*)((uintptr_t)scan->reg + scan->reg_offset);
  uart16550_reg_shift = scan->reg_shift;
  // http://wiki.osdev.org/Serial_Ports
  uart16550[UART_REG_IER << uart16550_reg_shift] = 0x00;                // Disable all interrupts
  uart16550[UART_REG_LCR << uart16550_reg_shift] = 0x80;                // Enable DLAB (set baud rate divisor)
  uart16550[UART_REG_DLL << uart16550_reg_shift] = (uint8_t)divisor;    // Set divisor (lo byte)
  uart16550[UART_REG_DLM << uart16550_reg_shift] = (uint8_t)(divisor >> 8);     //     (hi byte)
  uart16550[UART_REG_LCR << uart16550_reg_shift] = 0x03;                // 8 bits, no parity, one stop bit
  uart16550[UART_REG_FCR << uart16550_reg_shift] = 0xC7;                // Enable FIFO, clear them, with 14-byte threshold
}

void query_uart16550(uintptr_t fdt)
{
  struct fdt_cb cb;
  struct uart16550_scan scan;

  memset(&cb, 0, sizeof(cb));
  cb.open = uart16550_open;
  cb.prop = uart16550_prop;
  cb.done = uart16550_done;
  cb.extra = &scan;

  fdt_scan(fdt, &cb);
}
