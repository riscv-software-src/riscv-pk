// See LICENSE for license details.

#include "syscall.h"
#include "pk.h"
#include "file.h"
#include "frontend.h"
#include "vm.h"
#include <string.h>
#include <errno.h>

typedef sysret_t (*syscall_t)(long, long, long, long, long, long, long);

#define long_bytes (4 + 4*current.elf64)
#define get_long(base, i) ({ long res; \
  if (current.elf64) res = ((long*)base)[i]; \
  else res = ((int*)base)[i]; \
  res; })
#define put_long(base, i, data) ({ long res; \
  if (current.elf64) ((long*)base)[i] = (data); \
  else ((int*)base)[i] = (data); })

#define CLOCK_FREQ 1000000000

void sys_exit(int code)
{
  if (current.t0)
    printk("%ld cycles\n", rdcycle() - current.t0);

  frontend_syscall(SYS_exit, code, 0, 0, 0);
  while (1);
}

sysret_t sys_read(int fd, char* buf, size_t n)
{
  sysret_t r = {-1,EBADF};
  file_t* f = file_get(fd);

  if (f)
  {
    r = file_read(f, buf, n);
    file_decref(f);
  }

  return r;
}

sysret_t sys_write(int fd, const char* buf, size_t n)
{
  sysret_t r = {-1,EBADF};
  file_t* f = file_get(fd);

  if (f)
  {
    r = file_write(f, buf, n);
    file_decref(f);
  }

  return r;
}

sysret_t sys_open(const char* name, int flags, int mode)
{
  sysret_t ret = file_open(name, flags, mode);
  if(ret.result == -1)
    return ret;

  if((ret.result = file_dup((file_t*)ret.result)) == -1)
    ret.err = ENOMEM;

  return ret;
}

sysret_t sys_close(int fd)
{
  int ret = fd_close(fd);
  return (sysret_t){ret, ret & EBADF};
}

sysret_t sys_fstat(int fd, void* st)
{
  sysret_t r = {-1,EBADF};
  file_t* f = file_get(fd);

  if (f)
  {
    r = file_stat(f, st);
    file_decref(f);
  }

  return r;
}

sysret_t sys_lseek(int fd, size_t ptr, int dir)
{
  sysret_t r = {-1,EBADF};
  file_t* f = file_get(fd);

  if (f)
  {
    r = file_lseek(f, ptr, dir);
    file_decref(f);
  }

  return r;
}

sysret_t sys_stat(const char* name, void* st)
{
  size_t name_size = strlen(name)+1;
  populate_mapping(st, sizeof(struct stat), PROT_WRITE);
  return frontend_syscall(SYS_stat, (uintptr_t)name, name_size, (uintptr_t)st, 0);
}

sysret_t sys_lstat(const char* name, void* st)
{
  size_t name_size = strlen(name)+1;
  populate_mapping(st, sizeof(struct stat), PROT_WRITE);
  return frontend_syscall(SYS_lstat, (uintptr_t)name, name_size, (uintptr_t)st, 0);
}

sysret_t sys_link(const char* old_name, const char* new_name)
{
  size_t old_size = strlen(old_name)+1;
  size_t new_size = strlen(new_name)+1;
  return frontend_syscall(SYS_link, (uintptr_t)old_name, old_size,
                                    (uintptr_t)new_name, new_size);
}

sysret_t sys_unlink(const char* name, size_t len)
{
  size_t name_size = strlen(name)+1;
  return frontend_syscall(SYS_unlink, (uintptr_t)name, name_size, 0, 0);
}

sysret_t sys_brk(size_t pos)
{
  return do_brk(pos);
}

sysret_t sys_uname(void* buf)
{
  const int sz = 65;
  strcpy(buf + 0*sz, "Proxy Kernel");
  strcpy(buf + 1*sz, "");
  strcpy(buf + 2*sz, "3.4.5");
  strcpy(buf + 3*sz, "");
  strcpy(buf + 4*sz, "");
  strcpy(buf + 5*sz, "");
  return (sysret_t){0,0};
}

sysret_t sys_getuid()
{
  return (sysret_t){0,0};
}

sysret_t sys_mmap(uintptr_t addr, size_t length, int prot, int flags, int fd, off_t offset)
{
  return do_mmap(addr, length, prot, flags, fd, offset);
}

sysret_t sys_munmap(uintptr_t addr, size_t length)
{
  return do_munmap(addr, length);
}

sysret_t sys_mremap(uintptr_t addr, size_t old_size, size_t new_size, int flags)
{
  return do_mremap(addr, old_size, new_size, flags);
}

sysret_t sys_rt_sigaction(int sig, const void* act, void* oact, size_t sssz)
{
  if (oact)
  {
    size_t sz = long_bytes * 3;
    populate_mapping(oact, sz, PROT_WRITE);
    memset(oact, 0, sz);
  }

  return (sysret_t){0, 0};
}

sysret_t sys_time(void* loc)
{
  uintptr_t t = rdcycle();
  if (loc)
  {
    populate_mapping(loc, long_bytes, PROT_WRITE);
    put_long(loc, 0, t / CLOCK_FREQ);
  }
  return (sysret_t){t, 0};
}

sysret_t sys_times(void* restrict loc)
{
  populate_mapping(loc, 4*long_bytes, PROT_WRITE);

  uintptr_t t = rdcycle();
  kassert(CLOCK_FREQ % 1000000 == 0);
  put_long(loc, 0, t / (CLOCK_FREQ / 1000000));
  put_long(loc, 1, 0);
  put_long(loc, 2, 0);
  put_long(loc, 3, 0);
  
  return (sysret_t){0, 0};
}

sysret_t sys_gettimeofday(long* loc)
{
  populate_mapping(loc, 2*long_bytes, PROT_WRITE);

  uintptr_t t = rdcycle();
  put_long(loc, 0, t/CLOCK_FREQ);
  put_long(loc, 1, (t % CLOCK_FREQ) / (CLOCK_FREQ / 1000000));
  
  return (sysret_t){0, 0};
}

sysret_t sys_writev(int fd, const void* iov, int cnt)
{
  populate_mapping(iov, cnt*2*long_bytes, PROT_READ);

  ssize_t ret = 0;
  for (int i = 0; i < cnt; i++)
  {
    sysret_t r = sys_write(fd, (void*)get_long(iov, 2*i), get_long(iov, 2*i+1));
    if (r.result < 0)
      return r;
    ret += r.result;
  }
  return (sysret_t){ret, 0};
}

sysret_t syscall(long a0, long a1, long a2, long a3, long a4, long a5, long n)
{
  const static void* syscall_table[] = {
    [SYS_exit] = sys_exit,
    [SYS_exit_group] = sys_exit,
    [SYS_read] = sys_read,
    [SYS_write] = sys_write,
    [SYS_open] = sys_open,
    [SYS_close] = sys_close,
    [SYS_fstat] = sys_fstat,
    [SYS_lseek] = sys_lseek,
    [SYS_stat] = sys_stat,
    [SYS_lstat] = sys_lstat,
    [SYS_link] = sys_link,
    [SYS_unlink] = sys_unlink,
    [SYS_brk] = sys_brk,
    [SYS_uname] = sys_uname,
    [SYS_getuid] = sys_getuid,
    [SYS_geteuid] = sys_getuid,
    [SYS_getgid] = sys_getuid,
    [SYS_getegid] = sys_getuid,
    [SYS_mmap] = sys_mmap,
    [SYS_munmap] = sys_munmap,
    [SYS_mremap] = sys_mremap,
    [SYS_rt_sigaction] = sys_rt_sigaction,
    [SYS_time] = sys_time,
    [SYS_gettimeofday] = sys_gettimeofday,
    [SYS_times] = sys_times,
    [SYS_writev] = sys_writev,
  };

  if(n >= ARRAY_SIZE(syscall_table) || !syscall_table[n])
    panic("bad syscall #%ld!",n);

  sysret_t r = ((syscall_t)syscall_table[n])(a0, a1, a2, a3, a4, a5, n);
  return r;
}
