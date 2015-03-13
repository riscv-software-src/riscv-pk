#include "pk.h"
#include "frontend.h"
#include <string.h>
#include <stdlib.h>

void enumerate_devices()
{
  char buf[64] __attribute__((aligned(64)));

  for (int dev = 0; dev < 256; dev++)
  {
    tohost_sync(dev, 0xFF, (uintptr_t)buf << 8 | 0xFF);
    if (buf[0])
    {
      printk("device %d: %s\n", dev, buf);

      for (int cmd = 0; cmd < 255; cmd++)
      {
        tohost_sync(dev, 0xFF, (uintptr_t)buf << 8 | cmd);
        if (buf[0])
          printk("  command %d: %s\n", cmd, buf);
      }
    }
  }
}

void disk_test()
{
  struct disk_req {
    uint64_t addr;
    uint64_t offset;
    uint64_t size;
    uint64_t tag;
  };

  // find disk
  const char* disk_str = "disk size=";
  char buf[64] __attribute__((aligned(64)));

  for (int dev = 0; dev < 256; dev++)
  {
    tohost_sync(dev, 0xFF, (uintptr_t)buf << 8 | 0xFF);
    if (strncmp(buf, disk_str, strlen(disk_str)) == 0)
    {
      long size = atol(buf + strlen(disk_str));
      printk("found disk device %d, size %ld\n", dev, size);

      long sec_size = 512;
      char buf[sec_size] __attribute__((aligned(64)));

      // read block 3
      struct disk_req req = { (uintptr_t)buf, 3*sec_size, sec_size, 0 };
      tohost_sync(dev, 0, (uintptr_t)&req);
      // copy block 3 to block 5
      req.offset = 5*sec_size;
      tohost_sync(dev, 1, (uintptr_t)&req);

      printk("copied block 3 to block 5\n");
    }
  }
}

void rfb_test()
{
  char buf[64] __attribute__((aligned(64)));

  int bpp = 16;
  int width = 32;
  int height = 32;
  uint16_t fb[width * height] __attribute__((aligned(64)));

  for (int dev = 0; dev < 256; dev++)
  { 
    tohost_sync(dev, 0xFF, (uintptr_t)buf << 8 | 0xFF);
    if (strcmp(buf, "rfb") == 0)
    {

      tohost_sync(dev, 0, width | height << 16 | (uint64_t)bpp << 32);
      tohost_sync(dev, 1, (uintptr_t)fb);

      for (int pixel = 0; ; )
        for (int i = 0, value = 0; i < width * height; i++, pixel++)
          fb[i] = pixel;
    }
  }
}
