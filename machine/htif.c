#include "htif.h"
#include "atomic.h"
#include "mtrap.h"

#include <string.h>

volatile uint64_t tohost __attribute__((section(".htif")));
volatile uint64_t fromhost __attribute__((section(".htif")));
volatile int htif_console_buf;
static spinlock_t htif_lock = SPINLOCK_INIT;

static void __check_fromhost()
{
  uint64_t fh = fromhost;
  if (!fh)
    return;
  fromhost = 0;

  // this should be from the console
  assert(FROMHOST_DEV(fh) == 1);
  switch (FROMHOST_CMD(fh)) {
    case 0:
      htif_console_buf = 1 + (uint8_t)FROMHOST_DATA(fh);
      break;
    case 1:
      break;
    default:
      assert(0);
  }
}

static void __set_tohost(uintptr_t dev, uintptr_t cmd, uintptr_t data)
{
  while (tohost)
    __check_fromhost();
  tohost = TOHOST_CMD(dev, cmd, data);
}

int htif_console_getchar()
{
  spinlock_lock(&htif_lock);
    __check_fromhost();
    int ch = htif_console_buf;
    if (ch >= 0) {
      htif_console_buf = -1;
      __set_tohost(1, 0, 0);
    }
  spinlock_unlock(&htif_lock);

  return ch - 1;
}

static void do_tohost_fromhost(uintptr_t dev, uintptr_t cmd, uintptr_t data)
{
  spinlock_lock(&htif_lock);
    __set_tohost(dev, cmd, data);

    while (1) {
      uint64_t fh = fromhost;
      if (fh) {
        if (FROMHOST_DEV(fh) == dev && FROMHOST_CMD(fh) == cmd) {
          fromhost = 0;
          break;
        }
        __check_fromhost();
      }
    }
  spinlock_unlock(&htif_lock);
}

void htif_syscall(uintptr_t arg)
{
  do_tohost_fromhost(0, 0, arg);
}

void htif_console_putchar(uint8_t ch)
{
  spinlock_lock(&htif_lock);
    __set_tohost(1, 1, ch);
  spinlock_unlock(&htif_lock);
}

void htif_poweroff()
{
  while (1) {
    fromhost = 0;
    tohost = 1;
  }
}

struct request {
  uint64_t addr;
  uint64_t offset;
  uint64_t size;
  uint64_t tag;
};

void htif_disk_read(uintptr_t addr, uintptr_t offset, size_t size)
{
  struct request req;

  req.addr = addr;
  req.offset = offset;
  req.size = size;
  req.tag = 0;

  do_tohost_fromhost(2, 0, (uintptr_t) &req);
}

void htif_disk_write(uintptr_t addr, uintptr_t offset, size_t size)
{
  struct request req;

  req.addr = addr;
  req.offset = offset;
  req.size = size;
  req.tag = 0;

  do_tohost_fromhost(2, 1, (uintptr_t) &req);
}

unsigned long htif_disk_size(void)
{
  char idbuf[128];
  uintptr_t addr = (uintptr_t) idbuf;
  unsigned long payload;
  char *id = idbuf, *s;

  // The buffer address needs to be aligned to 64 bytes
  if (addr % 64 != 0) {
    unsigned long inc = 64 - (addr % 64);
    addr += inc;
    id += inc;
  }

  payload = (addr << 8) | 0xff;
  do_tohost_fromhost(2, 255, payload);

  s = strstr(id, "size=");
  if (s == NULL)
    return 0;
  s += 5;
  return atol(s);
}
