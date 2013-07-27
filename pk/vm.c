#include "vm.h"
#include "file.h"
#include "atomic.h"
#include "pcr.h"
#include "pk.h"
#include <stdint.h>
#include <errno.h>

typedef struct {
  uintptr_t addr;
  size_t length;
  file_t* file;
  size_t offset;
  size_t refcnt;
  int prot;
} vmr_t;

#define MAX_VMR 32
spinlock_t vm_lock = SPINLOCK_INIT;
static vmr_t vmrs[MAX_VMR];

typedef uintptr_t pte_t;
static pte_t* root_page_table;
static uintptr_t first_free_page;
static size_t next_free_page;
static size_t free_pages;
static int have_vm;

static uintptr_t __page_alloc()
{
  if (next_free_page == free_pages)
    return 0;
  uintptr_t addr = first_free_page + RISCV_PGSIZE * next_free_page++;
  memset((void*)addr, 0, RISCV_PGSIZE);
  return addr;
}

static vmr_t* __vmr_alloc(uintptr_t addr, size_t length, file_t* file,
                          size_t offset, size_t refcnt, int prot)
{
  for (vmr_t* v = vmrs; v < vmrs + MAX_VMR; v++)
  {
    if (v->refcnt == 0)
    {
      v->addr = addr;
      v->length = length;
      v->file = file;
      v->offset = offset;
      v->refcnt = refcnt;
      v->prot = prot;
      return v;
    }
  }
  return NULL;
}

static void __vmr_decref(vmr_t* v, size_t dec)
{
  if ((v->refcnt -= dec) == 0)
  {
    if (v->file)
      file_decref(v->file);
  }
}

static size_t pte_ppn(pte_t pte)
{
  return pte >> RISCV_PGSHIFT;
}

static pte_t ptd_create(uintptr_t ppn)
{
  return ppn << RISCV_PGSHIFT | PTE_T | PTE_V;
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

static pte_t super_pte_create(uintptr_t ppn, int kprot, int uprot, int level)
{
  kprot &= (PROT_READ | PROT_WRITE | PROT_EXEC);
  uprot &= (PROT_READ | PROT_WRITE | PROT_EXEC);
  int perm = (kprot * PTE_SR) | (uprot * PTE_UR) | PTE_V;
  return (ppn << (RISCV_PGLEVEL_BITS*level + RISCV_PGSHIFT)) | perm;
}

static pte_t pte_create(uintptr_t ppn, int kprot, int uprot)
{
  return super_pte_create(ppn, kprot, uprot, 0);
}

static __attribute__((always_inline)) pte_t* __walk_internal(uintptr_t addr, int create)
{
  const size_t pte_per_page = RISCV_PGSIZE/sizeof(void*);
  pte_t* t = root_page_table;

  for (unsigned i = RISCV_PGLEVELS-1; i > 0; i--)
  {
    size_t idx = pt_idx(addr, i);
    if (!(t[idx] & PTE_V))
    {
      if (!create)
        return 0;
      uintptr_t page = __page_alloc();
      if (page == 0)
        return 0;
      t[idx] = ptd_create(ppn(page));
    }
    else
      kassert(t[idx] & PTE_T);
    t = (pte_t*)(pte_ppn(t[idx]) << RISCV_PGSHIFT);
  }
  return &t[pt_idx(addr, 0)];
}

static pte_t* __walk(uintptr_t addr)
{
  return __walk_internal(addr, 0);
}

static pte_t* __walk_create(uintptr_t addr)
{
  return __walk_internal(addr, 1);
}

static int __va_avail(uintptr_t vaddr)
{
  pte_t* pte = __walk(vaddr);
  return pte == 0 || *pte == 0;
}

static uintptr_t __vm_alloc(size_t npage)
{
  uintptr_t start = current.brk, end = current.mmap_max - npage*RISCV_PGSIZE;
  for (uintptr_t a = start; a <= end; a += RISCV_PGSIZE)
  {
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

static void flush_tlb()
{
  mtpcr(PCR_PTBR, mfpcr(PCR_PTBR));
}

static int __handle_page_fault(uintptr_t vaddr, int prot)
{
  uintptr_t vpn = vaddr >> RISCV_PGSHIFT;
  vaddr = vpn << RISCV_PGSHIFT;

  pte_t* pte = __walk(vaddr);

  if (pte == 0 || *pte == 0)
    return -1;
  else if (!(*pte & PTE_V))
  {
    kassert(vaddr < current.stack_top && vaddr >= current.user_min);
    uintptr_t ppn = vpn;

    vmr_t* v = (vmr_t*)*pte;
    *pte = pte_create(ppn, PROT_READ|PROT_WRITE, 0);
    if (v->file)
    {
      size_t flen = MIN(RISCV_PGSIZE, v->length - (vaddr - v->addr));
      kassert(flen == file_pread(v->file, (void*)vaddr, flen, vaddr - v->addr + v->offset).result);
      if (flen < RISCV_PGSIZE)
        memset((void*)vaddr + flen, 0, RISCV_PGSIZE - flen);
    }
    else
      memset((void*)vaddr, 0, RISCV_PGSIZE);
    *pte = pte_create(ppn, v->prot, v->prot);
  }

  pte_t perms = pte_create(0, prot, prot);
  if ((*pte & perms) != perms)
    return -1;

  flush_tlb();
  return 0;
}

int handle_page_fault(uintptr_t vaddr, int prot)
{
  spinlock_lock(&vm_lock);
    int ret = __handle_page_fault(vaddr, prot);
  spinlock_unlock(&vm_lock);
  return ret;
}

uintptr_t __do_mmap(uintptr_t addr, size_t length, int prot, int flags, file_t* f, off_t offset)
{
  size_t npage = (length-1)/RISCV_PGSIZE+1;
  vmr_t* v = __vmr_alloc(addr, length, f, offset, npage, prot);
  if (!v)
    goto fail_vmr;

  if (flags & MAP_FIXED)
  {
    if ((addr & (RISCV_PGSIZE-1)) || addr < current.user_min ||
        addr + length > current.stack_top || addr + length < addr)
      goto fail_vma;
  }
  else if ((addr = __vm_alloc(npage)) == 0)
    goto fail_vma;

  for (uintptr_t a = addr; a < addr + length; a += RISCV_PGSIZE)
  {
    pte_t* pte = __walk_create(a);
    kassert(pte);

    if (*pte)
      kassert(*pte == 0); // TODO __do_munmap

    *pte = (pte_t)v;
  }

  if (!have_vm || (flags & MAP_POPULATE))
    for (uintptr_t a = addr; a < addr + length; a += RISCV_PGSIZE)
      kassert(__handle_page_fault(a, prot) == 0);

  if (f) file_incref(f);

  return addr;

fail_vma:
  __vmr_decref(v, npage);
fail_vmr:
  return (uintptr_t)-1;
}

sysret_t do_mmap(uintptr_t addr, size_t length, int prot, int flags, int fd, off_t offset)
{
  if (!(flags & MAP_PRIVATE) || length == 0 || (offset & (RISCV_PGSIZE-1)))
    return (sysret_t){-1, EINVAL};

  file_t* f = NULL;
  if (!(flags & MAP_ANONYMOUS) && (f = file_get(fd)) == NULL)
    return (sysret_t){-1, EBADF};

  spinlock_lock(&vm_lock);
    addr = __do_mmap(addr, length, prot, flags, f, offset);
    if (addr < current.brk_max)
      current.brk_max = addr;
  spinlock_unlock(&vm_lock);

  if (f) file_decref(f);
  return (sysret_t){addr, 0};
}

size_t __do_brk(size_t addr)
{
  size_t newbrk = addr;
  if (addr < current.brk_min)
    newbrk = current.brk_min;
  else if (addr > current.brk_max)
    newbrk = current.brk_max;

  if (current.brk == 0)
    current.brk = ROUNDUP(current.brk_min, RISCV_PGSIZE);

  size_t newbrk_page = ROUNDUP(newbrk, RISCV_PGSIZE);
  if (current.brk > newbrk_page)
    kassert(0); // TODO __do_munmap
  else if (current.brk < newbrk_page)
    kassert(__do_mmap(current.brk, newbrk_page - current.brk, -1, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, 0, 0) == current.brk);
  current.brk = newbrk_page;

  return newbrk;
}

sysret_t do_brk(size_t addr)
{
  spinlock_lock(&vm_lock);
    addr = __do_brk(addr);
  spinlock_unlock(&vm_lock);
  
  return (sysret_t){addr, 0};
}

static void __map_kernel_range(uintptr_t paddr, size_t len, int prot)
{
  pte_t perms = pte_create(0, prot, 0);
  for (uintptr_t a = paddr; a < paddr + len; a += RISCV_PGSIZE)
  {
    pte_t* pte = __walk_create(a);
    kassert(pte);
    *pte = a | perms;
  }
}

void populate_mapping(const void* start, size_t size, int prot)
{
  uintptr_t a0 = ROUNDDOWN((uintptr_t)start, RISCV_PGSIZE);
  for (uintptr_t a = a0; a < (uintptr_t)start+size; a += RISCV_PGSIZE)
  {
    atomic_t* atom = (atomic_t*)(a & -sizeof(atomic_t));
    if (prot & PROT_WRITE)
      atomic_add(atom, 0);
    else
      atomic_read(atom);
  }
}

void vm_init()
{
  extern char _end;
  current.user_min = ROUNDUP((uintptr_t)&_end, RISCV_PGSIZE);
  current.brk_min = current.user_min;
  current.brk = 0;

  uint32_t mem_mb = *(volatile uint32_t*)0;

  if (mem_mb == 0)
  {
    current.stack_bottom = 0;
    current.stack_top = 0;
    current.brk_max = 0;
    current.mmap_max = 0;
  }
  else
  {
    uintptr_t max_addr = (uintptr_t)mem_mb << 20;
    size_t mem_pages = max_addr >> RISCV_PGSHIFT;
    const size_t min_free_pages = 2*RISCV_PGLEVELS;
    const size_t min_stack_pages = 8;
    const size_t max_stack_pages = 128;
    kassert(mem_pages > min_free_pages + min_stack_pages);
    free_pages = MAX(mem_pages >> (RISCV_PGLEVEL_BITS-1), min_free_pages);
    size_t stack_pages = CLAMP(mem_pages/32, min_stack_pages, max_stack_pages);
    first_free_page = max_addr - free_pages * RISCV_PGSIZE;

    uintptr_t root_page_table_paddr = __page_alloc();
    kassert(root_page_table_paddr);
    root_page_table = (pte_t*)root_page_table_paddr;

    __map_kernel_range(0, current.user_min, PROT_READ|PROT_WRITE|PROT_EXEC);

    mtpcr(PCR_PTBR, root_page_table_paddr);
    setpcr(PCR_SR, SR_VM);
    have_vm = mfpcr(PCR_SR) & SR_VM;
    clearpcr(PCR_SR, SR_VM);

    size_t stack_size = RISCV_PGSIZE * stack_pages;
    current.stack_top = first_free_page;
    uintptr_t stack_bot = current.stack_top - stack_size;

    if (have_vm)
    {
      __map_kernel_range(first_free_page, free_pages * RISCV_PGSIZE, PROT_READ|PROT_WRITE);
      kassert(__do_mmap(stack_bot, stack_size, -1, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, 0, 0) == stack_bot);
      setpcr(PCR_SR, SR_VM);
    }

    current.stack_bottom = stack_bot;
    stack_bot -= RISCV_PGSIZE; // guard page
    current.mmap_max = current.brk_max = stack_bot;
  }
}
