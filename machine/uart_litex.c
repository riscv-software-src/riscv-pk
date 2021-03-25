// See LICENSE for license details.

#include <string.h>
#include "uart_litex.h"
#include "fdt.h"

volatile unsigned int *uart_litex;

#define UART_REG_RXTX       0
#define UART_REG_TXFULL     1
#define UART_REG_RXEMPTY    2
#define UART_REG_EV_STATUS  3
#define UART_REG_EV_PENDING 4
#define UART_REG_EV_ENABLE  5

void uart_litex_putchar(uint8_t c)
{
    while ((uart_litex[UART_REG_TXFULL] & 0x01)); // wait while tx-buffer full
    uart_litex[UART_REG_RXTX] = c;
}

int uart_litex_getchar()
{
    int c = -1;
    if (!(uart_litex[UART_REG_RXEMPTY] & 0x01)) { // if rx-buffer not empty
        c = uart_litex[UART_REG_RXTX];
        uart_litex[UART_REG_EV_PENDING] = 0x02; // ack (UART_EV_RX)
    }
    return c;
}

struct uart_litex_scan
{
    int compat;
    uint64_t reg;
};

static void uart_litex_open(const struct fdt_scan_node *node, void *extra)
{
    struct uart_litex_scan *scan = (struct uart_litex_scan *)extra;
    memset(scan, 0, sizeof(*scan));
}

static void uart_litex_prop(const struct fdt_scan_prop *prop, void *extra)
{
    struct uart_litex_scan *scan = (struct uart_litex_scan *)extra;
    if (!strcmp(prop->name, "compatible") &&
        (!strcmp((const char *)prop->value, "litex,uart0") ||
         !strcmp((const char *)prop->value, "litex,liteuart"))) {
        scan->compat = 1;
    } else if (!strcmp(prop->name, "reg")) {
        fdt_get_address(prop->node->parent, prop->value, &scan->reg);
    }
}

static void uart_litex_done(const struct fdt_scan_node *node, void *extra)
{
    struct uart_litex_scan *scan = (struct uart_litex_scan *)extra;
    if (!scan->compat || !scan->reg || uart_litex)
        return;

    // Initialize LiteX UART
    uart_litex = (void *)(uintptr_t)scan->reg;
}

void query_uart_litex(uintptr_t fdt)
{
    struct fdt_cb cb;
    struct uart_litex_scan scan;

    memset(&cb, 0, sizeof(cb));
    cb.open = uart_litex_open;
    cb.prop = uart_litex_prop;
    cb.done = uart_litex_done;
    cb.extra = &scan;

    fdt_scan(fdt, &cb);
}
