#include <machine/syscall.h>
#include <string.h>
#include <errno.h>
#include "pk.h"
#include "pcr.h"
#include "file.h"
#include "frontend.h"

typedef sysret_t (*syscall_t)(long,long,long,long,long);

void sys_exit(int code)
{
  frontend_syscall(SYS_exit,code,0,0,0);
  panic("exit didn't exit!");
}

sysret_t sys_read(int fd, char* buf, size_t n)
{
  sysret_t r = {-1,EBADF};
  file_t* f = file_get(fd);
  if(!f)
    return r;

  return file_read(f,buf,n);
}

sysret_t sys_write(int fd, const char* buf, size_t n)
{
  sysret_t r = {-1,EBADF};
  file_t* f = file_get(fd);
  if(!f)
    return r;

  return file_write(f,buf,n);
}

sysret_t sys_open(const char* name, size_t len, int flags, int mode)
{
  sysret_t ret = file_open(name, len, flags, mode);
  if(ret.result == -1)
    return ret;

  if((ret.result = file_dup((file_t*)ret.result)) == -1)
    ret.err = ENOMEM;

  return ret;
}

sysret_t sys_close(int fd)
{
  return (sysret_t){fd_close(fd),EBADF};
}

sysret_t sys_fstat(int fd, void* st)
{
  sysret_t r = {-1,EBADF};
  file_t* f = file_get(fd);
  if(!f)
    return r;

  return file_stat(f,st);
}

sysret_t sys_lseek(int fd, size_t ptr, int dir)
{
  sysret_t r = {-1,EBADF};
  file_t* f = file_get(fd);
  if(!f)
    return r;

  return file_lseek(f,ptr,dir);
}

sysret_t sys_stat(const char* name, size_t len, void* st)
{
  return frontend_syscall(SYS_stat,(long)name,len,(long)st,0);
}

sysret_t sys_lstat(const char* name, size_t len, void* st)
{
  return frontend_syscall(SYS_lstat,(long)name,len,(long)st,0);
}

sysret_t sys_link(const char* old_name, size_t old_len,
                  const char* new_name, size_t new_len)
{
  return frontend_syscall(SYS_link,(long)old_name,old_len,
                                   (long)new_name,new_len);
}

sysret_t sys_unlink(const char* name, size_t len)
{
  return frontend_syscall(SYS_unlink,(long)name,len,0,0);
}

sysret_t sys_brk(size_t pos)
{
  if(pos > (mfpcr(PCR_MEMSIZE) << MEMSIZE_SHIFT))
    return (sysret_t){-1, ENOMEM};
  return (sysret_t){0,0};
}

sysret_t syscall(long a0, long a1, long a2, long a3, long n)
{
  const static void* syscall_table[] = {
    [SYS_exit] = sys_exit,
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
  };

  if(n >= ARRAY_SIZE(syscall_table) || !syscall_table[n])
    panic("bad syscall #%ld!",n);

  return ((syscall_t)syscall_table[n])(a0, a1, a2, a3, n);
}
