#include "pk.h"
#include "pcr.h"

static uint64_t tohost_sync(unsigned dev, unsigned cmd, uint64_t payload)
{
  uint64_t tohost = (uint64_t)dev << 56 | (uint64_t)cmd << 48 | payload;
  uint64_t fromhost;
  __sync_synchronize();
  while (mtpcr(PCR_TOHOST, tohost));
  while ((fromhost = mtpcr(PCR_FROMHOST, 0)) == 0);
  __sync_synchronize();
  return fromhost;
}

void enumerate_devices()
{
  char buf[64] __attribute__((aligned(64)));

  for (uint64_t dev = 0; dev < 256; dev++)
  {
    tohost_sync(dev, 0xFF, (uintptr_t)buf << 8 | 0xFF);
    if (buf[0])
    {
      printk("device %d: %s\n", dev, buf);
      for (uint64_t cmd = 0; cmd < 255; cmd++)
      {
        tohost_sync(dev, 0xFF, (uintptr_t)buf << 8 | cmd);
        if (buf[0])
          printk("  command %d: %s\n", cmd, buf);
      }
    }
  }
}
