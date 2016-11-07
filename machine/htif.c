#include "mtrap.h"
#include "mimpl.h"
#include "htif.h"
#include "atomic.h"

volatile uint64_t tohost __attribute__((aligned(64))) __attribute__((section("htif")));
volatile uint64_t fromhost __attribute__((aligned(64))) __attribute__((section("htif")));
static spinlock_t htif_lock = SPINLOCK_INIT;

static void request_htif_keyboard_interrupt()
{
  assert(tohost == 0);
  tohost = TOHOST_CMD(1, 0, 0);
}

static void __htif_interrupt()
{
  // we should only be interrupted by keypresses
  uint64_t fh = fromhost;
  if (!fh)
    return;
  if (!(FROMHOST_DEV(fh) == 1 && FROMHOST_CMD(fh) == 0))
    return;
  HLS()->console_ibuf = 1 + (uint8_t)FROMHOST_DATA(fh);
  fromhost = 0;
  set_csr(mip, MIP_SSIP);
}

static void do_tohost_fromhost(uintptr_t dev, uintptr_t cmd, uintptr_t data)
{
  spinlock_lock(&htif_lock);
    while (tohost)
      __htif_interrupt();
    tohost = TOHOST_CMD(dev, cmd, data);

    while (1) {
      uint64_t fh = fromhost;
      if (fh) {
        if (FROMHOST_DEV(fh) == dev && FROMHOST_CMD(fh) == cmd) {
          fromhost = 0;
          break;
        }
        __htif_interrupt();
      }
    }
  spinlock_unlock(&htif_lock);
}

static void htif_interrupt()
{
  if (spinlock_trylock(&htif_lock) == 0) {
    __htif_interrupt();
    spinlock_unlock(&htif_lock);
  }
}

static void htif_putchar(uint8_t ch)
{
  do_tohost_fromhost(1, 1, ch);
}

static uintptr_t htif_getchar()
{
  int ch = atomic_swap(&HLS()->console_ibuf, -1);
  if (ch >= 0)
    request_htif_keyboard_interrupt();
  reset_ssip();
  return ch - 1;
}

static void __attribute((noreturn)) htif_poweroff()
{
  while (1)
    tohost = 1;
}

static int htif_swint_pending()
{
  return HLS()->console_ibuf > 0;
}

static void htif_timer_callback()
{
  htif_interrupt();
}

struct mimpl_ops htif_ops = {
  .console_putchar	= htif_putchar,
  .console_getchar	= htif_getchar,
  .power_off		= htif_poweroff,
  .swint_pending	= htif_swint_pending,
  .timer_callback	= htif_timer_callback,
};

struct mimpl_ops* mimpl_ops(void)
{
  return &htif_ops;
}

void htif_syscall(uintptr_t magic_mem)
{
  do_tohost_fromhost(0, 0, magic_mem);
}
