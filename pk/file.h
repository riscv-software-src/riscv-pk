// See LICENSE for license details.

#ifndef _FILE_H
#define _FILE_H

#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include "atomic.h"

enum file_type
{
  FILE_HOST,
  FILE_DEVICE
};

typedef struct file
{
  enum file_type typ;
  uint32_t refcnt;
  uint64_t padding[1]; // padding to make room for device parameters
} file_t;

typedef struct host_file
{
  enum file_type typ;
  uint32_t refcnt;
  int32_t kfd; // file descriptor on the host side of the HTIF
} host_file_t;

typedef struct device
{
  enum file_type typ;
  uint32_t refcnt;
  uint64_t base;
  uint64_t size;
} device_t;

extern file_t files[];
#define stdin  (files + 0)
#define stdout (files + 1)
#define stderr (files + 2)

file_t* file_get(int fd);
file_t* file_open(const char* fn, int flags, int mode);
void file_decref(file_t*);
void file_incref(file_t*);
int file_dup(file_t*);

file_t* file_openat(int dirfd, const char* fn, int flags, int mode);
ssize_t file_pwrite(file_t* f, const void* buf, size_t n, off_t off);
ssize_t file_pread(file_t* f, void* buf, size_t n, off_t off);
ssize_t file_write(file_t* f, const void* buf, size_t n);
ssize_t file_read(file_t* f, void* buf, size_t n);
ssize_t file_lseek(file_t* f, size_t ptr, int dir);
int file_truncate(file_t* f, off_t len);
int file_stat(file_t* f, struct stat* s);
int fd_close(int fd);

void file_init();

device_t *device_open(const char *name, int flags);
ssize_t device_pread(device_t *dev, void *buf, size_t size, off_t offset);
ssize_t device_pwrite(device_t *dev, void *buf, size_t size, off_t offset);

#endif
