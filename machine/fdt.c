#include <stdint.h>
#include <string.h>
#include "fdt.h"
#include "mtrap.h"

static inline uint32_t bswap(uint32_t x)
{
  uint32_t y = (x & 0x00FF00FF) <<  8 | (x & 0xFF00FF00) >>  8;
  uint32_t z = (y & 0x0000FFFF) << 16 | (y & 0xFFFF0000) >> 16;
  return z;
}

static const uint32_t *fdt_scan_helper(
  const uint32_t *lex,
  const char *strings,
  const char *name,
  struct fdt_scan_node *parent,
  fdt_cb cb,
  void *extra)
{
  struct fdt_scan_node node;
  struct fdt_scan_prop prop;

  node.parent = parent;
  node.base = lex;
  node.name = name;
  node.address_cells = 2;
  node.size_cells = 1;
  prop.node = &node;

  while (1) {
    switch (bswap(lex[0])) {
      case FDT_BEGIN_NODE: {
        const char *child_name = (const char *)(lex+1);
        lex = fdt_scan_helper(
          lex + 2 + strlen(child_name)/4,
          strings, child_name, &node, cb, extra);
        break;
      }
      case FDT_PROP: {
        prop.name  = strings + bswap(lex[2]);
        prop.len   = bswap(lex[1]);
        prop.value = lex + 3;
        if (!strcmp(prop.name, "#address-cells")) { node.address_cells = bswap(lex[3]); }
        if (!strcmp(prop.name, "#size-cells"))    { node.size_cells    = bswap(lex[3]); }
        lex += 3 + (prop.len+3)/4;
        cb(&prop, extra);
        break;
      }
      case FDT_END_NODE: return lex + 1;
      case FDT_NOP:      lex += 1; break;
      default:           return lex; // FDT_END
    }
  }
}

void fdt_scan(uintptr_t fdt, fdt_cb cb, void *extra)
{
  struct fdt_header *header = (struct fdt_header *)fdt;

  // Only process FDT that we understand
  if (bswap(header->magic) != FDT_MAGIC ||
      bswap(header->last_comp_version) > FDT_VERSION) return;

  const char *strings = (const char *)(fdt + bswap(header->off_dt_strings));
  const uint32_t *lex = (const uint32_t *)(fdt + bswap(header->off_dt_struct));

  fdt_scan_helper(lex, strings, "/", 0, cb, extra);
}

uint32_t fdt_size(uintptr_t fdt)
{
  struct fdt_header *header = (struct fdt_header *)fdt;

  // Only process FDT that we understand
  if (bswap(header->magic) != FDT_MAGIC ||
      bswap(header->last_comp_version) > FDT_VERSION) return 0;
  return bswap(header->totalsize);
}

const uint32_t *fdt_get_address(const struct fdt_scan_node *node, const uint32_t *value, uintptr_t *result)
{
  *result = 0;
  for (int cells = node->address_cells; cells > 0; --cells) *result += bswap(*value++);
  return value;
}

const uint32_t *fdt_get_size(const struct fdt_scan_node *node, const uint32_t *value, uintptr_t *result)
{
  *result = 0;
  for (int cells = node->size_cells; cells > 0; --cells) *result += bswap(*value++);
  return value;
}

static void find_mem(const struct fdt_scan_prop *prop, void *extra)
{
  const uint32_t **base = (const uint32_t **)extra;
  if (!strcmp(prop->name, "device_type") && !strcmp((const char*)prop->value, "memory")) {
     *base = prop->node->base;
  } else if (!strcmp(prop->name, "reg") && prop->node->base == *base) {
    const uint32_t *value = prop->value;
    const uint32_t *end = value + prop->len/4;
    assert (prop->len % 4 == 0);
    while (end - value > 0) {
      uintptr_t base, size;
      value = fdt_get_address(prop->node->parent, value, &base);
      value = fdt_get_size   (prop->node->parent, value, &size);
      if (base == DRAM_BASE) { mem_size = size; }
    }
    assert (end == value);
  }
}

void query_mem(uintptr_t fdt)
{
  mem_size = 0;
  const uint32_t *base = 0;
  fdt_scan(fdt, &find_mem, &base);
  assert (mem_size > 0);
}

static uint32_t hart_phandles[MAX_HARTS];

struct hart_scan {
  const uint32_t *base;
  int cpu;
  int controller;
  int cells;
  int hart;
  uint32_t phandle;
};

static void init_hart(struct hart_scan *scan, const uint32_t *base)
{
  scan->base = base;
  scan->cpu = 0;
  scan->controller = 0;
  scan->cells = -1;
  scan->hart = -1;
  scan->phandle = 0;
}

static void done_hart(struct hart_scan *scan)
{
  if (scan->cpu) {
    assert (scan->controller == 1);
    assert (scan->cells == 1);
    assert (scan->hart >= 0);
    if (scan->hart < MAX_HARTS) {
      hart_phandles[scan->hart] = scan->phandle;
      if (scan->hart >= num_harts) num_harts = scan->hart + 1;
    }
  }
}

static void count_harts(const struct fdt_scan_prop *prop, void *extra)
{
  struct hart_scan *scan = (struct hart_scan *)extra;
  if (prop->node->base != scan->base) {
    done_hart(scan);
    init_hart(scan, prop->node->base);
  }
  if (!strcmp(prop->name, "device_type") && !strcmp((const char*)prop->value, "cpu")) {
    scan->cpu = 1;
  } else if (!strcmp(prop->name, "interrupt-controller")) {
    scan->controller = 1;
  } else if (!strcmp(prop->name, "#interrupt-cells")) {
    scan->cells = bswap(prop->value[0]);
  } else if (!strcmp(prop->name, "phandle")) {
    scan->phandle = bswap(prop->value[0]);
  } else if (!strcmp(prop->name, "reg")) {
    uintptr_t reg;
    fdt_get_address(prop->node->parent, prop->value, &reg);
    scan->hart = reg;
  }
}

void query_harts(uintptr_t fdt)
{
  struct hart_scan scan;
  num_harts = 0;
  init_hart(&scan, 0);
  fdt_scan(fdt, &count_harts, &scan);
  done_hart(&scan);
  assert (num_harts > 0);
}

struct clint_scan
{
  const uint32_t *base;
  uintptr_t address;
};

static void find_clint_base(const struct fdt_scan_prop *prop, void *extra)
{
  struct clint_scan *scan = (struct clint_scan *)extra;
  if (!strcmp(prop->name, "compatible") && !strcmp((const char*)prop->value, "riscv,clint0"))
     scan->base = prop->node->base;
}

static void find_clint_reg(const struct fdt_scan_prop *prop, void *extra)
{
  struct clint_scan *scan = (struct clint_scan *)extra;
  if (!strcmp(prop->name, "reg") && scan->base == prop->node->base)
    fdt_get_address(prop->node->parent, prop->value, &scan->address);
}

static void setup_clint(const struct fdt_scan_prop *prop, void *extra)
{
  struct clint_scan *scan = (struct clint_scan *)extra;
  if (!strcmp(prop->name, "interrupts-extended") && scan->base == prop->node->base) {
    const uint32_t *value = prop->value;
    const uint32_t *end = value + prop->len/4;
    assert (prop->len % 16 == 0);
    for (int index = 0; end - value > 0; ++index) {
      uint32_t phandle = bswap(value[0]);
      int hart;
      for (hart = 0; hart < MAX_HARTS; ++hart)
        if (hart_phandles[hart] == phandle)
          break;
      if (hart < MAX_HARTS) {
        hls_t *hls = hls_init(hart);
        hls->ipi = (void*)(scan->address + index * 4);
        hls->timecmp = (void*)(scan->address + 0x4000 + (index * 8));
        *hls->ipi = 1; // wakeup the hart
      }
      value += 4;
    }
  }
}

void query_clint(uintptr_t fdt)
{
  struct clint_scan scan;
  scan.base = 0;
  scan.address = 0;

  fdt_scan(fdt, &find_clint_base, &scan);
  assert (scan.base != 0);

  fdt_scan(fdt, &find_clint_reg,  &scan);
  assert (scan.address != 0);

  mtime = (void*)(scan.address + 0xbff8);
  fdt_scan(fdt, &setup_clint, &scan);
}
