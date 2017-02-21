#include "htif.h"
#include "atomic.h"
#include "mtrap.h"

volatile uint64_t tohost __attribute__((section("htif")));
volatile uint64_t fromhost __attribute__((section("htif")));
volatile int htif_console_buf;
static spinlock_t htif_lock = SPINLOCK_INIT;

static void request_htif_keyboard_interrupt()
{
  assert(tohost == 0);
  tohost = TOHOST_CMD(1, 0, 0);
}

static void __check_fromhost()
{
  // we should only be interrupted by keypresses
  uint64_t fh = fromhost;
  if (!fh)
    return;
  assert(FROMHOST_DEV(fh) == 1 && FROMHOST_CMD(fh) == 0);
  htif_console_buf = 1 + (uint8_t)FROMHOST_DATA(fh);
  fromhost = 0;
}

int htif_console_getchar()
{
  if (spinlock_trylock(&htif_lock) == 0) {
    __check_fromhost();
    spinlock_unlock(&htif_lock);
  }

  int ch = atomic_swap(&htif_console_buf, -1);
  if (ch >= 0)
    request_htif_keyboard_interrupt();
  return ch - 1;
}

static void do_tohost_fromhost(uintptr_t dev, uintptr_t cmd, uintptr_t data)
{
  spinlock_lock(&htif_lock);
    while (tohost)
      __check_fromhost();
    tohost = TOHOST_CMD(dev, cmd, data);

    while (1) {
      uint64_t fh = fromhost;
      if (fh) {
        if (FROMHOST_DEV(fh) == dev && FROMHOST_CMD(fh) == cmd) {
          fromhost = 0;
          break;
        }
        __check_fromhost();
      }
      wfi();
    }
  spinlock_unlock(&htif_lock);
}

void htif_syscall(uintptr_t arg)
{
  do_tohost_fromhost(0, 0, arg);
}

void htif_console_putchar(uint8_t ch)
{
  do_tohost_fromhost(1, 1, ch);
}

void htif_poweroff()
{
  while (1) {
    fromhost = 0;
    tohost = 1;
  }
}
