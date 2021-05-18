// See LICENSE for license details.

#include "mmap.h"
#include "atomic.h"
#include "pk.h"
#include "boot.h"
#include "bits.h"
#include "mtrap.h"
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

uintptr_t kva2pa_offset;

typedef struct vmr_t {
  struct vmr_t* next;
  uintptr_t addr;
  size_t length;
  file_t* file;
  size_t offset;
  unsigned refcnt;
  int prot;
} vmr_t;

static vmr_t* vmr_freelist_head;

static pte_t* root_page_table;

#define RISCV_PGLEVELS ((VA_BITS - RISCV_PGSHIFT) / RISCV_PGLEVEL_BITS)

static spinlock_t vm_lock = SPINLOCK_INIT;

static uintptr_t first_free_page;
static size_t next_free_page;
static size_t free_pages;
static size_t pages_promised;

int demand_paging = 1; // unless -p flag is given
uint64_t randomize_mapping; // set by --randomize-mapping

typedef struct freelist_node_t {
  uintptr_t addr;
} freelist_node_t;

size_t page_freelist_depth;
static freelist_node_t* page_freelist_storage;

static uintptr_t free_page_addr(size_t idx)
{
  return first_free_page + idx * RISCV_PGSIZE;
}

static uintptr_t __early_pgalloc_align(size_t num_pages, size_t align)
{
  size_t skip_pages = (align - 1) & -(free_page_addr(next_free_page) / RISCV_PGSIZE);
  num_pages += skip_pages;

  if (num_pages + next_free_page < num_pages || num_pages + next_free_page > free_pages)
    return 0;

  uintptr_t addr = free_page_addr(next_free_page + skip_pages);
  next_free_page += num_pages;
  return addr;
}

static uintptr_t __early_alloc(size_t size)
{
  size_t num_pages = ROUNDUP(size, RISCV_PGSIZE) / RISCV_PGSIZE;
  return __early_pgalloc_align(num_pages, 1);
}

static void __maybe_fuzz_page_freelist();

static void __page_freelist_insert(freelist_node_t node)
{
  __maybe_fuzz_page_freelist();

  page_freelist_storage[page_freelist_depth++] = node;
}

static freelist_node_t __page_freelist_remove()
{
  __maybe_fuzz_page_freelist();

  return page_freelist_storage[--page_freelist_depth];
}

static bool __augment_page_freelist()
{
  uintptr_t page = __early_alloc(RISCV_PGSIZE);
  if (page != 0) {
    freelist_node_t node = { .addr = page };
    __page_freelist_insert(node);
  }
  return page;
}

static void __maybe_fuzz_page_freelist()
{
  if (randomize_mapping) {
    randomize_mapping = lfsr63(randomize_mapping);

    if (randomize_mapping % 2 == 0 && page_freelist_depth) {
      size_t swap_idx = randomize_mapping % page_freelist_depth;
      freelist_node_t tmp = page_freelist_storage[swap_idx];
      page_freelist_storage[swap_idx] = page_freelist_storage[page_freelist_depth-1];
      page_freelist_storage[page_freelist_depth-1] = tmp;
    }

    if (randomize_mapping % 16 == 0)
      __augment_page_freelist();
  }
}

static bool __page_freelist_empty()
{
  return page_freelist_depth == 0;
}

static size_t __num_free_pages()
{
  return page_freelist_depth + (free_pages - next_free_page);
}

static uintptr_t __page_alloc()
{
  if (__page_freelist_empty() && !__augment_page_freelist())
    return 0;

  freelist_node_t node = __page_freelist_remove();

  memset((void*)pa2kva(node.addr), 0, RISCV_PGSIZE);

  return node.addr;
}

static uintptr_t __page_alloc_assert()
{
  uintptr_t res = __page_alloc();

  if (!res)
    panic("Out of memory!");

  return res;
}

static void __page_free(uintptr_t addr)
{
  freelist_node_t node = { .addr = addr };
  __page_freelist_insert(node);
}

static vmr_t* __vmr_alloc(uintptr_t addr, size_t length, file_t* file,
                          size_t offset, unsigned refcnt, int prot)
{
  if (vmr_freelist_head == NULL) {
    vmr_t* new_vmrs = (vmr_t*)pa2kva(__page_alloc());
    if (new_vmrs == NULL)
      return NULL;

    vmr_freelist_head = new_vmrs;

    for (size_t i = 0; i < (RISCV_PGSIZE / sizeof(vmr_t)) - 1; i++)
      new_vmrs[i].next = &new_vmrs[i+1];
  }

  vmr_t* v = vmr_freelist_head;
  vmr_freelist_head = v->next;

  pages_promised += refcnt;

  if (file)
    file_incref(file);

  v->addr = addr;
  v->length = length;
  v->file = file;
  v->offset = offset;
  v->refcnt = refcnt;
  v->prot = prot;
  return v;
}

static void __vmr_decref(vmr_t* v, unsigned dec)
{
  pages_promised -= dec;

  if ((v->refcnt -= dec) == 0) {
    if (v->file)
      file_decref(v->file);

    v->next = vmr_freelist_head;
    vmr_freelist_head = v;
  }
}

static size_t pte_ppn(pte_t pte)
{
  return pte >> PTE_PPN_SHIFT;
}

static uintptr_t ppn(uintptr_t addr)
{
  return addr >> RISCV_PGSHIFT;
}

static size_t pt_idx(uintptr_t addr, int level)
{
  size_t idx = addr >> (RISCV_PGLEVEL_BITS*level + RISCV_PGSHIFT);
  return idx & ((1 << RISCV_PGLEVEL_BITS) - 1);
}

static inline pte_t* __walk_internal(pte_t* t, uintptr_t addr, int create, int level)
{
  for (int i = RISCV_PGLEVELS - 1; i > level; i--) {
    size_t idx = pt_idx(addr, i);
    if (unlikely(!(t[idx] & PTE_V))) {
      if (create) {
        uintptr_t new_ptd = __page_alloc();
        if (!new_ptd)
          return 0;
        t[idx] = ptd_create(ppn(new_ptd));
      } else {
        return 0;
      }
    }
    t = (pte_t*)pa2kva(pte_ppn(t[idx]) << RISCV_PGSHIFT);
  }
  return &t[pt_idx(addr, level)];
}

static pte_t* __walk(uintptr_t addr)
{
  return __walk_internal(root_page_table, addr, 0, 0);
}

static pte_t* __walk_create(uintptr_t addr)
{
  return __walk_internal(root_page_table, addr, 1, 0);
}

static int __va_avail(uintptr_t vaddr)
{
  pte_t* pte = __walk(vaddr);
  return pte == 0 || *pte == 0;
}

static uintptr_t __vm_alloc_at(uintptr_t start, uintptr_t end, size_t npage)
{
  for (uintptr_t a = start; a <= end; a += RISCV_PGSIZE) {
    if (!__va_avail(a))
      continue;
    uintptr_t first = a, last = a + (npage-1) * RISCV_PGSIZE;
    for (a = last; a > first && __va_avail(a); a -= RISCV_PGSIZE)
      ;
    if (a > first)
      continue;
    return a;
  }
  return 0;
}

static uintptr_t __vm_alloc(size_t npage)
{
  uintptr_t end = current.mmap_max - npage * RISCV_PGSIZE;
  if (current.vm_alloc_guess) {
    uintptr_t ret = __vm_alloc_at(current.vm_alloc_guess, end, npage);
    if (ret)
      return ret;
  }

  return __vm_alloc_at(current.brk, end, npage);
}

static inline pte_t prot_to_type(int prot, int user)
{
  pte_t pte = 0;
  if (prot & PROT_READ) pte |= PTE_R | PTE_A;
  if (prot & PROT_WRITE) pte |= PTE_W | PTE_D;
  if (prot & PROT_EXEC) pte |= PTE_X | PTE_A;
  if (pte == 0) pte = PTE_R;
  if (user) pte |= PTE_U;
  return pte;
}

int __valid_user_range(uintptr_t vaddr, size_t len)
{
  uintptr_t last_vaddr = vaddr + len - 1;
  if (last_vaddr < vaddr)
    return 0;
  return last_vaddr < current.mmap_max;
}

static void flush_tlb_entry(uintptr_t vaddr)
{
  asm volatile ("sfence.vma %0" : : "r" (vaddr) : "memory");
}

static int __handle_page_fault(uintptr_t vaddr, int prot)
{
  uintptr_t vpn = vaddr >> RISCV_PGSHIFT;
  vaddr = vpn << RISCV_PGSHIFT;

  pte_t* pte = __walk(vaddr);

  if (pte == 0 || *pte == 0 || !__valid_user_range(vaddr, 1))
    return -1;
  else if (!(*pte & PTE_V))
  {
    uintptr_t ppn = __page_alloc_assert() / RISCV_PGSIZE;
    uintptr_t kva = pa2kva(ppn * RISCV_PGSIZE);

    vmr_t* v = (vmr_t*)*pte;
    *pte = pte_create(ppn, prot_to_type(PROT_READ|PROT_WRITE, 0));
    flush_tlb_entry(vaddr);
    if (v->file)
    {
      size_t flen = MIN(RISCV_PGSIZE, v->length - (vaddr - v->addr));
      ssize_t ret = file_pread(v->file, (void*)kva, flen, vaddr - v->addr + v->offset);
      kassert(ret > 0);
      if (ret < RISCV_PGSIZE)
        memset((void*)vaddr + ret, 0, RISCV_PGSIZE - ret);
    }
    __vmr_decref(v, 1);
    *pte = pte_create(ppn, prot_to_type(v->prot, 1));
    flush_tlb_entry(vaddr);
  }

  pte_t perms = pte_create(0, prot_to_type(prot, 1));
  if ((*pte & perms) != perms)
    return -1;

  return 0;
}

int handle_page_fault(uintptr_t vaddr, int prot)
{
  spinlock_lock(&vm_lock);
    int ret = __handle_page_fault(vaddr, prot);
  spinlock_unlock(&vm_lock);
  return ret;
}

static void __do_munmap(uintptr_t addr, size_t len)
{
  for (uintptr_t a = addr; a < addr + len; a += RISCV_PGSIZE)
  {
    pte_t* pte = __walk(a);
    if (pte == 0 || *pte == 0)
      continue;

    if (*pte & PTE_V)
      __page_free(pte_ppn(*pte) << RISCV_PGSHIFT);
    else
      __vmr_decref((vmr_t*)*pte, 1);

    *pte = 0;
    flush_tlb_entry(a);
  }
}

uintptr_t __do_mmap(uintptr_t addr, size_t length, int prot, int flags, file_t* f, off_t offset)
{
  size_t npage = (length-1)/RISCV_PGSIZE+1;

  if (npage * 17 / 16 + 16 + pages_promised >= __num_free_pages())
    return (uintptr_t)-1;

  if (flags & MAP_FIXED)
  {
    if ((addr & (RISCV_PGSIZE-1)) || !__valid_user_range(addr, length))
      return (uintptr_t)-1;
  }
  else if ((addr = __vm_alloc(npage)) == 0)
    return (uintptr_t)-1;

  vmr_t* v = __vmr_alloc(addr, length, f, offset, npage, prot);
  if (!v)
    return (uintptr_t)-1;

  for (uintptr_t a = addr; a < addr + length; a += RISCV_PGSIZE)
  {
    pte_t* pte = __walk_create(a);
    kassert(pte);

    if (*pte)
      __do_munmap(a, RISCV_PGSIZE);

    *pte = (pte_t)v;
  }

  if (!demand_paging || (flags & MAP_POPULATE))
    for (uintptr_t a = addr; a < addr + length; a += RISCV_PGSIZE)
      kassert(__handle_page_fault(a, prot) == 0);

  current.vm_alloc_guess = addr + npage * RISCV_PGSIZE;

  return addr;
}

int do_munmap(uintptr_t addr, size_t length)
{
  if ((addr & (RISCV_PGSIZE-1)) || !__valid_user_range(addr, length))
    return -EINVAL;

  spinlock_lock(&vm_lock);
    __do_munmap(addr, length);
  spinlock_unlock(&vm_lock);

  return 0;
}

uintptr_t do_mmap(uintptr_t addr, size_t length, int prot, int flags, int fd, off_t offset)
{
  if (!(flags & MAP_PRIVATE) || length == 0 || (offset & (RISCV_PGSIZE-1)))
    return -EINVAL;

  file_t* f = NULL;
  if (!(flags & MAP_ANONYMOUS) && (f = file_get(fd)) == NULL)
    return -EBADF;

  spinlock_lock(&vm_lock);
    addr = __do_mmap(addr, length, prot, flags, f, offset);

    if (addr < current.brk_max)
      current.brk_max = addr;
  spinlock_unlock(&vm_lock);

  if (f) file_decref(f);
  return addr;
}

uintptr_t __do_brk(size_t addr)
{
  uintptr_t newbrk = addr;
  if (addr < current.brk_min)
    newbrk = current.brk_min;
  else if (addr > current.brk_max)
    newbrk = current.brk_max;

  if (current.brk == 0)
    current.brk = ROUNDUP(current.brk_min, RISCV_PGSIZE);

  uintptr_t newbrk_page = ROUNDUP(newbrk, RISCV_PGSIZE);
  if (current.brk > newbrk_page) {
    __do_munmap(newbrk_page, current.brk - newbrk_page);
  } else if (current.brk < newbrk_page) {
    if (__do_mmap(current.brk, newbrk_page - current.brk, -1, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, 0, 0) != current.brk)
      return current.brk;
  }
  current.brk = newbrk_page;

  return newbrk;
}

uintptr_t do_brk(size_t addr)
{
  spinlock_lock(&vm_lock);
    addr = __do_brk(addr);
  spinlock_unlock(&vm_lock);
  
  return addr;
}

uintptr_t do_mremap(uintptr_t addr, size_t old_size, size_t new_size, int flags)
{
  return -ENOSYS;
}

uintptr_t do_mprotect(uintptr_t addr, size_t length, int prot)
{
  uintptr_t res = 0;
  if ((addr) & (RISCV_PGSIZE-1))
    return -EINVAL;

  spinlock_lock(&vm_lock);
    for (uintptr_t a = addr; a < addr + length; a += RISCV_PGSIZE)
    {
      pte_t* pte = __walk(a);
      if (pte == 0 || *pte == 0) {
        res = -ENOMEM;
        break;
      }
  
      if (!(*pte & PTE_V)) {
        vmr_t* v = (vmr_t*)*pte;
        if((v->prot ^ prot) & ~v->prot){
          //TODO:look at file to find perms
          res = -EACCES;
          break;
        }
        v->prot = prot;
      } else {
        if (!(*pte & PTE_U) ||
            ((prot & PROT_READ) && !(*pte & PTE_R)) ||
            ((prot & PROT_WRITE) && !(*pte & PTE_W)) ||
            ((prot & PROT_EXEC) && !(*pte & PTE_X))) {
          //TODO:look at file to find perms
          res = -EACCES;
          break;
        }
        *pte = pte_create(pte_ppn(*pte), prot_to_type(prot, 1));
      }

      flush_tlb_entry(a);
    }
  spinlock_unlock(&vm_lock);

  return res;
}

static inline void __map_kernel_page(uintptr_t vaddr, uintptr_t paddr, int level, int prot)
{
  pte_t* pte = __walk_internal(root_page_table, vaddr, 1, level);
  kassert(pte);
  *pte = pte_create(paddr >> RISCV_PGSHIFT, prot_to_type(prot, 0));
}

static void __map_kernel_range(uintptr_t vaddr, uintptr_t paddr, size_t len, int prot)
{
  size_t megapage_size = RISCV_PGSIZE << RISCV_PGLEVEL_BITS;
  bool megapage_coaligned = (vaddr ^ paddr) % megapage_size == 0;

  // could support misaligned mappings, but no need today
  kassert((vaddr | paddr | len) % megapage_size == 0);

  while (len > 0) {
    __map_kernel_page(vaddr, paddr, 1, prot);

    len -= megapage_size;
    vaddr += megapage_size;
    paddr += megapage_size;
  }
}

void populate_mapping(const void* start, size_t size, int prot)
{
  uintptr_t a0 = ROUNDDOWN((uintptr_t)start, RISCV_PGSIZE);
  for (uintptr_t a = a0; a < (uintptr_t)start+size; a += RISCV_PGSIZE)
  {
    if (prot & PROT_WRITE)
      atomic_add((int*)a, 0);
    else
      atomic_read((int*)a);
  }
}

static void init_early_alloc()
{
  // PA space must fit within half of VA space
  uintptr_t user_size = -KVA_START;
  mem_size = MIN(mem_size, user_size);

  current.mmap_max = current.brk_max = user_size;

  extern char _end;
  first_free_page = ROUNDUP((uintptr_t)&_end, RISCV_PGSIZE);
  free_pages = (mem_size - (first_free_page - MEM_START)) / RISCV_PGSIZE;
}

uintptr_t pk_vm_init()
{
  init_early_alloc();

  size_t num_freelist_nodes = mem_size / RISCV_PGSIZE;
  page_freelist_storage = (freelist_node_t*)__early_alloc(num_freelist_nodes * sizeof(freelist_node_t));

  root_page_table = (void*)__page_alloc_assert();
  __map_kernel_range(KVA_START, MEM_START, mem_size, PROT_READ|PROT_WRITE|PROT_EXEC);

  flush_tlb();
  write_csr(satp, ((uintptr_t)root_page_table >> RISCV_PGSHIFT) | SATP_MODE_CHOICE);

  uintptr_t kernel_stack_top = __page_alloc_assert() + RISCV_PGSIZE;

  // relocate
  kva2pa_offset = KVA_START - MEM_START;
  page_freelist_storage = (void*)pa2kva(page_freelist_storage);
  root_page_table = (void*)pa2kva(root_page_table);

  return kernel_stack_top;
}
