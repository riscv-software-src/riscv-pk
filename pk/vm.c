#include "vm.h"
#include "file.h"
#include "atomic.h"
#include "pk.h"
#include <stdint.h>
#include <errno.h>

typedef struct {
  uintptr_t addr;
  size_t length;
  file_t* file;
  size_t offset;
  unsigned refcnt;
  int prot;
} vmr_t;

#define MAX_VMR 32
spinlock_t vm_lock = SPINLOCK_INIT;
static vmr_t vmrs[MAX_VMR];

pte_t* root_page_table;
static uintptr_t first_free_page;
static size_t next_free_page;
static size_t free_pages;

static uintptr_t __page_alloc()
{
  kassert(next_free_page != free_pages);
  uintptr_t addr = first_free_page + RISCV_PGSIZE * next_free_page++;
  memset((void*)addr, 0, RISCV_PGSIZE);
  return addr;
}

static vmr_t* __vmr_alloc(uintptr_t addr, size_t length, file_t* file,
                          size_t offset, unsigned refcnt, int prot)
{
  for (vmr_t* v = vmrs; v < vmrs + MAX_VMR; v++)
  {
    if (v->refcnt == 0)
    {
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
  }
  return NULL;
}

static void __vmr_decref(vmr_t* v, unsigned dec)
{
  if ((v->refcnt -= dec) == 0)
  {
    if (v->file)
      file_decref(v->file);
  }
}

static size_t pte_ppn(pte_t pte)
{
  return pte >> PTE_PPN_SHIFT;
}

static pte_t ptd_create(uintptr_t ppn)
{
  return (ppn << PTE_PPN_SHIFT) | PTE_V | PTE_TYPE_TABLE;
}

static inline pte_t pte_create(uintptr_t ppn, int prot, int user)
{
  pte_t pte = (ppn << PTE_PPN_SHIFT) | PTE_V;
  if (prot & PROT_WRITE)
    pte |= PTE_TYPE_URW_SRW;
  if (prot & PROT_EXEC)
    pte |= PTE_TYPE_URX_SRX;
  if (!user)
    pte |= PTE_TYPE_SR;
  return pte;
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

static void __maybe_create_root_page_table()
{
  if (root_page_table)
    return;
  root_page_table = (void*)__page_alloc();
  if (have_vm)
    write_csr(sptbr, root_page_table);
}

static pte_t* __walk_internal(uintptr_t addr, int create)
{
  const size_t pte_per_page = RISCV_PGSIZE/sizeof(void*);
  __maybe_create_root_page_table();
  pte_t* t = root_page_table;
  unsigned levels = (VA_BITS - RISCV_PGSHIFT) / RISCV_PGLEVEL_BITS;

  for (unsigned i = levels-1; i > 0; i--)
  {
    size_t idx = pt_idx(addr, i);
    if (!(t[idx] & PTE_V))
    {
      if (!create)
        return 0;
      uintptr_t page = __page_alloc();
      t[idx] = ptd_create(ppn(page));
    }
    else
      kassert(PTE_TABLE(t[idx]));
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

int __valid_user_range(uintptr_t vaddr, size_t len)
{
  if (vaddr + len < vaddr)
    return 0;
  return vaddr >= current.first_free_paddr && vaddr + len <= current.mmap_max;
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
    uintptr_t ppn = vpn;

    vmr_t* v = (vmr_t*)*pte;
    *pte = pte_create(ppn, PROT_READ|PROT_WRITE, 0);
    flush_tlb();
    if (v->file)
    {
      size_t flen = MIN(RISCV_PGSIZE, v->length - (vaddr - v->addr));
      ssize_t ret = file_pread(v->file, (void*)vaddr, flen, vaddr - v->addr + v->offset);
      kassert(ret > 0);
      if (ret < RISCV_PGSIZE)
        memset((void*)vaddr + ret, 0, RISCV_PGSIZE - ret);
    }
    else
      memset((void*)vaddr, 0, RISCV_PGSIZE);
    __vmr_decref(v, 1);
    *pte = pte_create(ppn, v->prot, 1);
  }

  pte_t perms = pte_create(0, prot, 1);
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

static void __do_munmap(uintptr_t addr, size_t len)
{
  for (uintptr_t a = addr; a < addr + len; a += RISCV_PGSIZE)
  {
    pte_t* pte = __walk(a);
    if (pte == 0 || *pte == 0)
      continue;

    if (!(*pte & PTE_V))
      __vmr_decref((vmr_t*)*pte, 1);

    *pte = 0;
  }
  flush_tlb(); // TODO: shootdown
}

uintptr_t __do_mmap(uintptr_t addr, size_t length, int prot, int flags, file_t* f, off_t offset)
{
  size_t npage = (length-1)/RISCV_PGSIZE+1;
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

  if (!have_vm || (flags & MAP_POPULATE))
    for (uintptr_t a = addr; a < addr + length; a += RISCV_PGSIZE)
      kassert(__handle_page_fault(a, prot) == 0);

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
      current.brk_max = ROUNDUP(addr + length, RISCV_PGSIZE);
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
  if (current.brk > newbrk_page)
    __do_munmap(newbrk_page, current.brk - newbrk_page);
  else if (current.brk < newbrk_page)
    kassert(__do_mmap(current.brk, newbrk_page - current.brk, -1, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, 0, 0) == current.brk);
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
        if (((prot & PROT_WRITE) && !PTE_UW(*pte))
            || ((prot & PROT_EXEC) && !PTE_UX(*pte))) {
          //TODO:look at file to find perms
          res = -EACCES;
          break;
        }
        *pte = pte_create(pte_ppn(*pte), prot, 1);
      }
    }
  spinlock_unlock(&vm_lock);
 
  return res;
}

void __map_kernel_range(uintptr_t vaddr, uintptr_t paddr, size_t len, int prot)
{
  uintptr_t n = ROUNDUP(len, RISCV_PGSIZE) / RISCV_PGSIZE;
  for (uintptr_t a = vaddr, i = 0; i < n; i++, a += RISCV_PGSIZE)
  {
    pte_t* pte = __walk_create(a);
    kassert(pte);
    *pte = pte_create((a - vaddr + paddr) >> RISCV_PGSHIFT, prot, 0);
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

static uintptr_t sbi_top_paddr()
{
  extern char _end;
  return ROUNDUP((uintptr_t)&_end, RISCV_PGSIZE);
}

#define first_free_paddr() (sbi_top_paddr() + num_harts * RISCV_PGSIZE)

void vm_init()
{
  current.first_free_paddr = first_free_paddr();

  size_t mem_pages = mem_size >> RISCV_PGSHIFT;
  free_pages = MAX(8, mem_pages >> (RISCV_PGLEVEL_BITS-1));
  first_free_page = mem_size - free_pages * RISCV_PGSIZE;
  current.mmap_max = current.brk_max = first_free_page;
}

void supervisor_vm_init()
{
  uintptr_t highest_va = -current.first_free_paddr;
  mem_size = MIN(mem_size, highest_va - current.first_user_vaddr) & -SUPERPAGE_SIZE;

  pte_t* sbi_pt = (pte_t*)(current.first_vaddr_after_user + current.bias);
  memset(sbi_pt, 0, RISCV_PGSIZE);
  pte_t* middle_pt = (void*)sbi_pt + RISCV_PGSIZE;
#ifndef __riscv64
  size_t num_middle_pts = 1;
  pte_t* root_pt = middle_pt;
  memset(root_pt, 0, RISCV_PGSIZE);
#else
  size_t num_middle_pts = (-current.first_user_vaddr - 1) / MEGAPAGE_SIZE + 1;
  pte_t* root_pt = (void*)middle_pt + num_middle_pts * RISCV_PGSIZE;
  memset(middle_pt, 0, (num_middle_pts + 1) * RISCV_PGSIZE);
  for (size_t i = 0; i < num_middle_pts; i++)
    root_pt[(1<<RISCV_PGLEVEL_BITS)-num_middle_pts+i] = ptd_create(((uintptr_t)middle_pt >> RISCV_PGSHIFT) + i);
#endif

  for (uintptr_t vaddr = current.first_user_vaddr, paddr = vaddr + current.bias, end = current.first_vaddr_after_user;
       paddr < mem_size; vaddr += SUPERPAGE_SIZE, paddr += SUPERPAGE_SIZE) {
    int l2_shift = RISCV_PGLEVEL_BITS + RISCV_PGSHIFT;
    size_t l2_idx = (current.first_user_vaddr >> l2_shift) & ((1 << RISCV_PGLEVEL_BITS)-1);
    l2_idx += ((vaddr - current.first_user_vaddr) >> l2_shift);
    middle_pt[l2_idx] = pte_create(paddr >> RISCV_PGSHIFT, PROT_READ|PROT_WRITE|PROT_EXEC, 0);
  }
  current.first_vaddr_after_user += (void*)root_pt + RISCV_PGSIZE - (void*)sbi_pt;

  // map SBI at top of vaddr space
  uintptr_t num_sbi_pages = sbi_top_paddr() / RISCV_PGSIZE;
  for (uintptr_t i = 0; i < num_sbi_pages; i++) {
    uintptr_t idx = (1 << RISCV_PGLEVEL_BITS) - num_sbi_pages + i;
    sbi_pt[idx] = pte_create(i, PROT_READ|PROT_EXEC, 0);
  }
  pte_t* sbi_pte = middle_pt + ((num_middle_pts << RISCV_PGLEVEL_BITS)-1);
  kassert(!*sbi_pte);
  *sbi_pte = ptd_create((uintptr_t)sbi_pt >> RISCV_PGSHIFT);

  // disable our allocator
  kassert(next_free_page == 0);
  free_pages = 0;

  mb();
  root_page_table = root_pt;
  write_csr(sptbr, root_pt);
}

uintptr_t pk_vm_init()
{
  // keep RV32 addresses positive
  if (!current.elf64)
    current.mmap_max = MIN(current.mmap_max, 0x80000000U);

  __map_kernel_range(0, 0, current.first_free_paddr, PROT_READ|PROT_WRITE|PROT_EXEC);
  __map_kernel_range(first_free_page, first_free_page, free_pages * RISCV_PGSIZE, PROT_READ|PROT_WRITE);

  size_t stack_size = RISCV_PGSIZE * CLAMP(mem_size/(RISCV_PGSIZE*32), 1, 256);
  current.stack_bottom = __do_mmap(current.mmap_max - stack_size, stack_size, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, 0, 0);
  current.stack_top = current.stack_bottom + stack_size;
  kassert(current.stack_bottom != (uintptr_t)-1);

  uintptr_t kernel_stack_top = __page_alloc() + RISCV_PGSIZE;
  return kernel_stack_top;
}
