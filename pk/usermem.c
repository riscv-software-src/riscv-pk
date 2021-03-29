// See LICENSE for license details.

#include "usermem.h"
#include "mmap.h"
#include <string.h>
#include <stdint.h>

void memset_user(void* dst, int ch, size_t n)
{
  if ((uintptr_t)dst + n < (uintptr_t)dst || !is_uva(dst + n - 1))
    handle_page_fault((uintptr_t)dst, PROT_WRITE);

  uintptr_t sstatus = set_csr(sstatus, SSTATUS_SUM);

  memset(dst, ch, n);

  write_csr(sstatus, sstatus);
}

void memcpy_to_user(void* dst, const void* src, size_t n)
{
  if ((uintptr_t)dst + n < (uintptr_t)dst || !is_uva(dst + n - 1))
    handle_page_fault((uintptr_t)dst, PROT_WRITE);

  uintptr_t sstatus = set_csr(sstatus, SSTATUS_SUM);

  memcpy(dst, src, n);

  write_csr(sstatus, sstatus);
}

void memcpy_from_user(void* dst, const void* src, size_t n)
{
  if ((uintptr_t)src + n < (uintptr_t)src || !is_uva(src + n - 1))
    handle_page_fault((uintptr_t)src, PROT_READ);

  uintptr_t sstatus = set_csr(sstatus, SSTATUS_SUM);

  memcpy(dst, src, n);

  write_csr(sstatus, sstatus);
}

bool strcpy_from_user(char* dst, const char* src, size_t n)
{
  bool res = false;

  uintptr_t sstatus = set_csr(sstatus, SSTATUS_SUM);

  while (n > 0) {
    if (!is_uva(src))
      handle_page_fault((uintptr_t)src, PROT_READ);

    char ch = *(volatile const char*)src;
    *dst = ch;

    if (ch == 0) {
      res = true;
      break;
    }

    src++;
    dst++;
    n--;
  }

  write_csr(sstatus, sstatus);

  return res;
}
