#ifndef _FILE_H
#define _FILE_H

#include <sys/stat.h>
#include <machine/syscall.h>
#include "atomic.h"

typedef struct file
{
  int kfd; // file descriptor on the appserver side
  atomic_t refcnt;
} file_t;

extern file_t *stdin, *stdout, *stderr;

file_t* file_get(int fd);
sysret_t file_open(const char* fn, int mode);
int file_dup(file_t*);

sysret_t file_write(file_t* f, const void* buf, size_t n);
sysret_t file_read(file_t* f, void* buf, size_t n);
sysret_t file_stat(file_t* f, struct stat* s);
int fd_close(int fd);

void file_init();

#define cons_write(buf,sz) file_write(stdout,buf,sz)

#endif
