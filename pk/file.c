// See LICENSE for license details.

#include <string.h>
#include <errno.h>
#include "file.h"
#include "pk.h"
#include "frontend.h"
#include "pcr.h"
#include "vm.h"

#define MAX_FDS 32
static file_t* fds[MAX_FDS];
#define MAX_FILES 32
static file_t files[MAX_FILES] = {[0 ... MAX_FILES-1] = {-1,{0}}};
file_t *stdout, *stdin, *stderr;

void file_incref(file_t* f)
{
  atomic_add(&f->refcnt, 1);
}

void file_decref(file_t* f)
{
  if (atomic_add(&f->refcnt, -1) == 2)
  {
    int kfd = f->kfd;
    mb();
    atomic_set(&f->refcnt, 0);

    frontend_syscall(SYS_close, kfd, 0, 0, 0);
  }
}

static file_t* file_get_free()
{
  for (file_t* f = files; f < files + MAX_FILES; f++)
    if (atomic_read(&f->refcnt) == 0 && atomic_cas(&f->refcnt, 0, 2) == 0)
      return f;
  return NULL;
}

int file_dup(file_t* f)
{
  for (int i = 0; i < MAX_FDS; i++)
  {
    if (fds[i] == NULL && __sync_bool_compare_and_swap(&fds[i], 0, f))
    {
      file_incref(f);
      return i;
    }
  }
  return -1;
}

void file_init()
{
  stdin = file_get_free();
  stdout = file_get_free();
  stderr = file_get_free();

  stdin->kfd = 0;
  stdout->kfd = 1;
  stderr->kfd = 2;

  // create user FDs 0, 1, and 2
  file_dup(stdin);
  file_dup(stdout);
  file_dup(stderr);
}

file_t* file_get(int fd)
{
  file_t* f;
  if (fd < 0 || fd >= MAX_FDS || (f = fds[fd]) == NULL)
    return 0;

  long old_cnt;
  do {
    old_cnt = atomic_read(&f->refcnt);
    if (old_cnt == 0)
      return 0;
  } while (atomic_cas(&f->refcnt, old_cnt, old_cnt+1) != old_cnt);

  return f;
}

sysret_t file_open(const char* fn, int flags, int mode)
{
  file_t* f = file_get_free();
  if(!f)
    return (sysret_t){-1,ENOMEM};

  size_t fn_size = strlen(fn)+1;
  sysret_t ret = frontend_syscall(SYS_open, (long)fn, fn_size, flags, mode);
  if(ret.result != -1)
  {
    f->kfd = ret.result;
    ret.result = (long)f;
  }
  else
    file_decref(f);

  return ret;
}

int fd_close(int fd)
{
  file_t* f = file_get(fd);
  if (!f)
    return -1;
  int success = __sync_bool_compare_and_swap(&fds[fd], f, 0);
  file_decref(f);
  if (!success)
    return -1;
  file_decref(f);
  return 0;
}

sysret_t file_read(file_t* f, void* buf, size_t size)
{
  populate_mapping(buf, size, PROT_WRITE);
  return frontend_syscall(SYS_read, f->kfd, (uintptr_t)buf, size, 0);
}

sysret_t file_pread(file_t* f, void* buf, size_t size, off_t offset)
{
  populate_mapping(buf, size, PROT_WRITE);
  return frontend_syscall(SYS_pread, f->kfd, (uintptr_t)buf, size, offset);
}

sysret_t file_write(file_t* f, const void* buf, size_t size)
{
  populate_mapping(buf, size, PROT_READ);
  return frontend_syscall(SYS_write, f->kfd, (uintptr_t)buf, size, 0);
}

sysret_t file_pwrite(file_t* f, const void* buf, size_t size, off_t offset)
{
  populate_mapping(buf, size, PROT_READ);
  return frontend_syscall(SYS_pwrite, f->kfd, (uintptr_t)buf, size, offset);
}

sysret_t file_stat(file_t* f, struct stat* s)
{
  populate_mapping(s, sizeof(*s), PROT_WRITE);
  return frontend_syscall(SYS_fstat, f->kfd, (uintptr_t)s, 0, 0);
}

sysret_t file_lseek(file_t* f, size_t ptr, int dir)
{
  return frontend_syscall(SYS_lseek, f->kfd, ptr, dir, 0);
}
