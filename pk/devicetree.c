#include "devicetree.h"
#include "encoding.h"
#include "pk.h"
#include "mtrap.h"
#include <stdbool.h>

#define ntohl(x) __builtin_bswap32(x)

#define FDT_TABLE_CAP 16

struct fdt_table_entry fdt_table[FDT_TABLE_CAP];
int fdt_table_size = 0;

uintptr_t fdt_base;
size_t fdt_size;

static uintptr_t max_hart_id;

static uint64_t fdt_read_uint64(uint32_t* addr) {
  return ((uint64_t)ntohl(addr[0]) << 32) | ntohl(addr[1]);
}

static void fdt_handle_cpu(const char* isa, uint32_t* reg_addr, uint32_t reg_len)
{
  int xlen = sizeof(long) * 8;
  kassert(reg_len == 8);
  kassert(isa && isa[0]=='r' && isa[1]=='v' && isa[2]=='0'+(xlen/10));

  uintptr_t* base_addr = (uintptr_t*)(uintptr_t)fdt_read_uint64(reg_addr);
  debug_printk("at %p, ", base_addr);
  uintptr_t hart_id = *(uintptr_t*)(base_addr + CSR_MHARTID);
  kassert(hart_id < MAX_HARTS);
  debug_printk("found hart %ld\n", hart_id);
  hls_init(hart_id, base_addr);
  num_harts++;
  max_hart_id = MAX(max_hart_id, hart_id);
}

static void fdt_handle_mem(uint32_t* reg_addr, uint32_t reg_len)
{
  kassert(reg_len == 16);
  uint64_t base = fdt_read_uint64(reg_addr);
  uint64_t size = fdt_read_uint64(reg_addr+2);
  debug_printk("at %p, found %d MiB of memory\n", base, (int)(size >> 20));
  kassert(base == 0);
  mem_size = size;
}

static void fdt_handle_device(const char *dev_type,
    uint32_t *reg_addr, uint32_t reg_len, int prot)
{
  struct fdt_table_entry *entry;

  kassert(reg_len == 16);
  kassert(fdt_table_size < FDT_TABLE_CAP);

  entry = &fdt_table[fdt_table_size];

  entry->dev_type = dev_type;
  entry->base = fdt_read_uint64(reg_addr);
  entry->size = fdt_read_uint64(reg_addr + 2);
  entry->prot = prot;

  debug_printk("found device %s@%lx (size: %ld, prot: %d)\n",
		  dev_type, entry->base, entry->size, prot);

  fdt_table_size++;
}

struct fdt_table_entry *fdt_find_device(const char *dev_type, int pos)
{
  int i;
  int found = 0;

  for (i = 0; i < fdt_table_size; i++) {
    if (strcmp(fdt_table[i].dev_type, dev_type) == 0) {
      if (found == pos)
	return &fdt_table[i];
      found++;
    }
  }

  return NULL;
}

// This code makes the following assumptions about FDTs:
// - They are trusted and don't need to be sanitized
// - All addresses and sizes are 64 bits (we don't parse #address-cells etc)

static uint32_t* parse_node(uint32_t* token, char* strings)
{
  const char* nodename = (const char*)token, *s, *dev_type = 0, *isa = 0;
  uint32_t reg_len = 0, *reg_addr = 0, prot = 0;
  token = (uint32_t*)nodename + strlen(nodename)/4+1;

  while (1) switch (ntohl(*token)) {
    case FDT_PROP: {
      token++;
      uint32_t len = ntohl(*token++);
      const char* name = strings + ntohl(*token++);
      if (strcmp(name, "device_type") == 0) {
        dev_type = (char*)token;
      } else if (strcmp(name, "isa") == 0) {
        isa = (char*)token;
      } else if (strcmp(name, "reg") == 0) {
        reg_len = len;
        reg_addr = token;
      } else if (strcmp(name, "protection") == 0) {
	prot = ntohl(*token);
      }
      token += (len+3)/4;
      continue;
    }
    case FDT_BEGIN_NODE:
      token = parse_node(token+1, strings);
      continue;
    case FDT_END_NODE:
      goto out;
    case FDT_NOP:
      continue;
    default:
      kassert(0);
  }

out:
  if (dev_type) {
    if (strcmp(dev_type, "cpu") == 0)
      fdt_handle_cpu(isa, reg_addr, reg_len);
    else if (strcmp(dev_type, "memory") == 0)
      fdt_handle_mem(reg_addr, reg_len);
    else if (prot)
      fdt_handle_device(dev_type, reg_addr, reg_len, prot);
  }

  return token+1;
}

void parse_device_tree()
{
  fdt_base = read_csr(miobase);
  struct fdt_header* hdr = (struct fdt_header*) fdt_base;
  debug_printk("reading device tree at %p\n", hdr);
  kassert(ntohl(hdr->magic) == FDT_MAGIC);
  fdt_size = ntohl(hdr->totalsize);
  char* strings = (char*)hdr + ntohl(hdr->off_dt_strings);
  uint32_t* root = (uint32_t*)((char*)hdr + ntohl(hdr->off_dt_struct));
  while (ntohl(*root++) != FDT_BEGIN_NODE);
  parse_node(root, strings);
  kassert(max_hart_id == num_harts-1);
}
