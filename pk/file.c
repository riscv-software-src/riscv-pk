// See LICENSE for license details.

#include <string.h>
#include <errno.h>
#include "file.h"
#include "pk.h"
#include "frontend.h"
#include "pcr.h"

#define MAX_FDS 32
file_t* fds[MAX_FDS];
#define MAX_FILES 32
file_t files[MAX_FILES] = {[0 ... MAX_FILES-1] = {-1,{0}}};
file_t *stdout, *stdin, *stderr;

static void file_incref(file_t* f)
{
  atomic_add(&f->refcnt,1);
}

void file_decref(file_t* f)
{
  if(atomic_add(&f->refcnt,-1) == 2)
  {
    if(f->kfd != -1)
    {
      frontend_syscall(SYS_close,f->kfd,0,0,0);
      f->kfd = -1;
    }
    atomic_add(&f->refcnt,-1); // I think this could just be atomic_set(..,0)
  }
}

static file_t* file_get_free()
{
  for(int i = 0; i < MAX_FILES; i++)
  {
    if(atomic_read(&files[i].refcnt) == 0)
    {
      if(atomic_add(&files[i].refcnt,1) == 0)
      {
        atomic_add(&files[i].refcnt,1);
        return &files[i];
      }
      file_decref(&files[i]);
    }
  }
  return NULL;
}

static int fd_get_free()
{
  for(int i = 0; i < MAX_FDS; i++)
    if(fds[i] == NULL)
      return i;
  return -1;
}

int file_dup(file_t* f)
{
  int fd = fd_get_free();
  if(fd == -1)
    return -1;
  file_incref(f);
  fds[fd] = f;
  return fd;
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
  return fd < 0 || fd >= MAX_FDS ? NULL : fds[fd];
}

sysret_t file_open(const char* fn, size_t len, int flags, int mode)
{
  file_t* f = file_get_free();
  if(!f)
    return (sysret_t){-1,ENOMEM};

  sysret_t ret = frontend_syscall(SYS_open,(long)fn,len,flags,mode);
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
  if(!f)
    return -1;
  fds[fd] = NULL;
  file_decref(f);
  return 0;
}

sysret_t file_read(file_t* f, char* buf, size_t size)
{
  return frontend_syscall(SYS_read,f->kfd,(long)buf,size,0);
}

sysret_t file_pread(file_t* f, char* buf, size_t size, off_t offset)
{
  return frontend_syscall(SYS_pread,f->kfd,(long)buf,size,offset);
}

sysret_t file_write(file_t* f, const char* buf, size_t size)
{
  return frontend_syscall(SYS_write,f->kfd,(long)buf,size,0);
}

sysret_t file_pwrite(file_t* f, const char* buf, size_t size, off_t offset)
{
  return frontend_syscall(SYS_pwrite,f->kfd,(long)buf,size,offset);
}

sysret_t file_stat(file_t* f, struct stat* s)
{
  return frontend_syscall(SYS_fstat,f->kfd,(long)s,0,0);
}

sysret_t file_lseek(file_t* f, size_t ptr, int dir)
{
  return frontend_syscall(SYS_lseek,f->kfd,ptr,dir,0);
}
