#include <string.h>
#include "file.h"
#include "pk.h"

#define MAX_FDS 1000
file_t* fds[MAX_FDS];
#define MAX_FILES 1000
file_t files[MAX_FILES];
file_t *stdout, *stdin, *stderr;

static void file_incref(file_t* f)
{
  f->refcnt++;
}

static void file_decref(file_t* f)
{
  f->refcnt--;
}

static file_t* file_get_free()
{
  for(int i = 0; i < MAX_FILES; i++)
  {
    if(files[i].refcnt == 0)
    {
      files[i].refcnt = 1;
      return &files[i];
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
  file_incref(f);
  int fd = fd_get_free();
  if(fd == -1)
    return -1;
  fds[fd] = f;
  return fd;
}

void file_init()
{
  stdin = file_get_free();
  stdout = file_get_free();
  stderr = file_get_free();
  kassert(stdin && stdout && stderr);

  stdin->kfd = 0;
  stdout->kfd = 1;
  stderr->kfd = 2;

  kassert(file_dup(stdin) == 0);
  kassert(file_dup(stdout) == 1);
  kassert(file_dup(stderr) == 2);
}

file_t* file_get(int fd)
{
  return fd < 0 || fd >= MAX_FDS ? NULL : fds[fd];
}

file_t* file_open(const char* fn)
{
  return NULL;
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

static void putch(int c)
{
  char ch = c;
  asm volatile ("mtc0 %0,$8" : : "r"(ch));
}

sysret_t file_write(file_t* f, const void* buf, size_t size)
{
  kassert(f == stdout);
  for(int i = 0; i < size; i++)
    putch(((char*)buf)[i]);
  return (sysret_t){0,0};
}

sysret_t file_stat(file_t* f, struct stat* s)
{
  kassert(f == stdout);
  s->st_mode = S_IFCHR;
  return (sysret_t){0,0};
}
