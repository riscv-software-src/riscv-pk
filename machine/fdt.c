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
  struct fdt_scan_node *node,
  const struct fdt_cb *cb)
{
  struct fdt_scan_node child;
  struct fdt_scan_prop prop;
  int last = 0;

  child.parent = node;
  child.address_cells = 2;
  child.size_cells = 1;
  prop.node = node;

  while (1) {
    switch (bswap(lex[0])) {
      case FDT_NOP: {
        lex += 1;
        break;
      }
      case FDT_PROP: {
        assert (!last);
        prop.name  = strings + bswap(lex[2]);
        prop.len   = bswap(lex[1]);
        prop.value = lex + 3;
        if (node && !strcmp(prop.name, "#address-cells")) { node->address_cells = bswap(lex[3]); }
        if (node && !strcmp(prop.name, "#size-cells"))    { node->size_cells    = bswap(lex[3]); }
        lex += 3 + (prop.len+3)/4;
        cb->prop(&prop, cb->extra);
        break;
      }
      case FDT_BEGIN_NODE: {
        if (!last && node && cb->done) cb->done(node, cb->extra);
        last = 1;
        child.name = (const char *)(lex+1);
        if (cb->open) cb->open(&child, cb->extra);
        lex = fdt_scan_helper(
          lex + 2 + strlen(child.name)/4,
          strings, &child, cb);
        if (cb->close) cb->close(&child, cb->extra);
        break;
      }
      case FDT_END_NODE: {
        if (!last && node && cb->done) cb->done(node, cb->extra);
        return lex + 1;
      }
      default: { // FDT_END
        if (!last && node && cb->done) cb->done(node, cb->extra);
        return lex;
      }
    }
  }
}

void fdt_scan(uintptr_t fdt, const struct fdt_cb *cb)
{
  struct fdt_header *header = (struct fdt_header *)fdt;

  // Only process FDT that we understand
  if (bswap(header->magic) != FDT_MAGIC ||
      bswap(header->last_comp_version) > FDT_VERSION) return;

  const char *strings = (const char *)(fdt + bswap(header->off_dt_strings));
  const uint32_t *lex = (const uint32_t *)(fdt + bswap(header->off_dt_struct));

  fdt_scan_helper(lex, strings, 0, cb);
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

//////////////////////////////////////////// MEMORY SCAN /////////////////////////////////////////

struct mem_scan {
  int memory;
  const uint32_t *reg_value;
  int reg_len;
};

static void mem_open(const struct fdt_scan_node *node, void *extra)
{
  struct mem_scan *scan = (struct mem_scan *)extra;
  memset(scan, 0, sizeof(*scan));
}

static void mem_prop(const struct fdt_scan_prop *prop, void *extra)
{
  struct mem_scan *scan = (struct mem_scan *)extra;
  if (!strcmp(prop->name, "device_type") && !strcmp((const char*)prop->value, "memory")) {
    scan->memory = 1;
  } else if (!strcmp(prop->name, "reg")) {
    scan->reg_value = prop->value;
    scan->reg_len = prop->len;
  }
}

static void mem_done(const struct fdt_scan_node *node, void *extra)
{
  struct mem_scan *scan = (struct mem_scan *)extra;
  const uint32_t *value = scan->reg_value;
  const uint32_t *end = value + scan->reg_len/4;
  uintptr_t self = (uintptr_t)mem_done;

  if (!scan->memory) return;
  assert (scan->reg_value && scan->reg_len % 4 == 0);

  while (end - value > 0) {
    uintptr_t base, size;
    value = fdt_get_address(node->parent, value, &base);
    value = fdt_get_size   (node->parent, value, &size);
    if (base <= self && self <= base + size) { mem_size = size; }
  }
  assert (end == value);
}

void query_mem(uintptr_t fdt)
{
  struct fdt_cb cb;
  struct mem_scan scan;

  memset(&cb, 0, sizeof(cb));
  cb.open = mem_open;
  cb.prop = mem_prop;
  cb.done = mem_done;
  cb.extra = &scan;

  mem_size = 0;
  fdt_scan(fdt, &cb);
  assert (mem_size > 0);
}

///////////////////////////////////////////// HART SCAN //////////////////////////////////////////

static uint32_t hart_phandles[MAX_HARTS];

struct hart_scan {
  int cpu;
  int controller;
  int cells;
  int hart;
  uint32_t phandle;
};

static void hart_open(const struct fdt_scan_node *node, void *extra)
{
  struct hart_scan *scan = (struct hart_scan *)extra;
  memset(scan, 0, sizeof(*scan));
  scan->cells = -1;
  scan->hart = -1;
}

static void hart_prop(const struct fdt_scan_prop *prop, void *extra)
{
  struct hart_scan *scan = (struct hart_scan *)extra;
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

static void hart_done(const struct fdt_scan_node *node, void *extra)
{
  struct hart_scan *scan = (struct hart_scan *)extra;

  if (!scan->cpu) return;
  assert (scan->controller == 1);
  assert (scan->cells == 1);
  assert (scan->hart >= 0);

  if (scan->hart < MAX_HARTS) {
    hart_phandles[scan->hart] = scan->phandle;
    if (scan->hart >= num_harts) num_harts = scan->hart + 1;
  }
}

void query_harts(uintptr_t fdt)
{
  struct fdt_cb cb;
  struct hart_scan scan;

  memset(&cb, 0, sizeof(cb));
  cb.open = hart_open;
  cb.prop = hart_prop;
  cb.done = hart_done;
  cb.extra = &scan;

  num_harts = 0;
  fdt_scan(fdt, &cb);
  assert (num_harts > 0);
}

///////////////////////////////////////////// CLINT SCAN /////////////////////////////////////////

struct clint_scan
{
  int compat;
  uintptr_t reg;
  const uint32_t *int_value;
  int int_len;
  int done;
};

static void clint_open(const struct fdt_scan_node *node, void *extra)
{
  struct clint_scan *scan = (struct clint_scan *)extra;
  scan->compat = 0;
  scan->reg = 0;
  scan->int_value = 0;
}

static void clint_prop(const struct fdt_scan_prop *prop, void *extra)
{
  struct clint_scan *scan = (struct clint_scan *)extra;
  if (!strcmp(prop->name, "compatible") && !strcmp((const char*)prop->value, "riscv,clint0")) {
    scan->compat = 1;
  } else if (!strcmp(prop->name, "reg")) {
    fdt_get_address(prop->node->parent, prop->value, &scan->reg);
  } else if (!strcmp(prop->name, "interrupts-extended")) {
    scan->int_value = prop->value;
    scan->int_len = prop->len;
  }
}

static void clint_done(const struct fdt_scan_node *node, void *extra)
{
  struct clint_scan *scan = (struct clint_scan *)extra;
  const uint32_t *value = scan->int_value;
  const uint32_t *end = value + scan->int_len/4;

  if (!scan->compat) return;
  assert (scan->reg != 0);
  assert (scan->int_value && scan->int_len % 16 == 0);
  assert (!scan->done); // only one clint

  scan->done = 1;
  mtime = (void*)(scan->reg + 0xbff8);

  for (int index = 0; end - value > 0; ++index) {
    uint32_t phandle = bswap(value[0]);
    int hart;
    for (hart = 0; hart < MAX_HARTS; ++hart)
      if (hart_phandles[hart] == phandle)
        break;
    if (hart < MAX_HARTS) {
      hls_t *hls = hls_init(hart);
      hls->ipi = (void*)(scan->reg + index * 4);
      hls->timecmp = (void*)(scan->reg + 0x4000 + (index * 8));
      *hls->ipi = 1; // wakeup the hart
    }
    value += 4;
  }
}

void query_clint(uintptr_t fdt)
{
  struct fdt_cb cb;
  struct clint_scan scan;

  memset(&cb, 0, sizeof(cb));
  cb.open = clint_open;
  cb.prop = clint_prop;
  cb.done = clint_done;
  cb.extra = &scan;

  scan.done = 0;
  fdt_scan(fdt, &cb);
  assert (scan.done);
}
