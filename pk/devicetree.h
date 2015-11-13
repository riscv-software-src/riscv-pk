// See LICENSE for license details.

#ifndef _PK_DEVICETREE_H
#define _PK_DEVICETREE_H

#include <stdint.h>

#define FDT_MAGIC 0xd00dfeedU
#define FDT_VERSION 17
#define FDT_COMP_VERSION 16
#define FDT_BEGIN_NODE 1
#define FDT_END_NODE 2
#define FDT_PROP 3
#define FDT_NOP 4
#define FDT_END 9

struct fdt_header {
  uint32_t magic;
  uint32_t totalsize;
  uint32_t off_dt_struct;
  uint32_t off_dt_strings;
  uint32_t off_rsvmap;
  uint32_t version;
  uint32_t last_comp_version;
  uint32_t boot_cpuid_phys;
  uint32_t size_dt_strings;
  uint32_t size_dt_struct;
};

struct fdt_reserve_entry {
  uint64_t address;
  uint64_t size;
};

void parse_device_tree();

#endif
