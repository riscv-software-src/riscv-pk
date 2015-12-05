// See LICENSE for license details.

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "file.h"
#include "pk.h"
#include "frontend.h"
#include "vm.h"
#include "devicetree.h"
#include "sbi.h"
#include "mcall.h"

#define MAX_FDS 128
static file_t* fds[MAX_FDS];
#define MAX_FILES 128
file_t files[MAX_FILES] = {0};

void file_incref(file_t* f)
{
  long prev = atomic_add(&f->refcnt, 1);
  kassert(prev > 0);
}

void file_decref(file_t* f)
{
  if (atomic_add(&f->refcnt, -1) == 2)
  {
    enum file_type typ = f->typ;
    int kfd = ((host_file_t *) f)->kfd;
    mb();
    atomic_set(&f->refcnt, 0);

    if (typ == FILE_HOST)
      frontend_syscall(SYS_close, kfd, 0, 0, 0, 0, 0, 0);
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
    if (atomic_cas(&fds[i], 0, f) == 0)
    {
      file_incref(f);
      return i;
    }
  }
  return -1;
}

void file_init()
{
  // create stdin, stdout, stderr and FDs 0-2
  for (int i = 0; i < 3; i++) {
    host_file_t* f = (host_file_t *) file_get_free();
    f->typ = FILE_HOST;
    f->kfd = i;
    file_dup((file_t *) f);
  }
}

file_t* file_get(int fd)
{
  file_t* f;
  if (fd < 0 || fd >= MAX_FDS || (f = atomic_read(&fds[fd])) == NULL)
    return 0;

  long old_cnt;
  do {
    old_cnt = atomic_read(&f->refcnt);
    if (old_cnt == 0)
      return 0;
  } while (atomic_cas(&f->refcnt, old_cnt, old_cnt+1) != old_cnt);

  return f;
}

file_t* file_open(const char* fn, int flags, int mode)
{
  return file_openat(AT_FDCWD, fn, flags, mode);
}

device_t *device_open(const char *name, int flags)
{
  struct fdt_table_entry *entry;
  device_t *dev;
  int retval;

  entry = fdt_find_device(name, 0);
  if (entry == NULL)
    return NULL;

  // check permissions
  if ((entry->prot & PROT_READ) == 0) {
    debug_printk("insufficient permissions to access device %s\n", name);
    return NULL;
  }

  if (flags != O_RDONLY && (entry->prot & PROT_WRITE) == 0) {
    debug_printk("device %s is read only\n", name);
    return NULL;
  }

  dev = (device_t *) file_get_free();
  dev->typ = FILE_DEVICE;
  dev->base = entry->base;
  dev->size = entry->size;

  return dev;
}

host_file_t *host_file_openat(int dirfd, const char *fn, int flags, int mode)
{
  host_file_t* f = (host_file_t *) file_get_free();
  if (f == NULL)
    return ERR_PTR(-ENOMEM);

  size_t fn_size = strlen(fn)+1;
  long ret = frontend_syscall(SYS_openat, dirfd, (long)fn, fn_size, flags, mode, 0, 0);
  if (ret >= 0)
  {
    f->typ = FILE_HOST;
    f->kfd = ret;
    return f;
  }
  else
  {
    file_decref((file_t *) f);
    return ERR_PTR(ret);
  }
}

file_t* file_openat(int dirfd, const char* fn, int flags, int mode)
{
  if (strncmp("/dev/", fn, 5) == 0)
    return (file_t *) device_open(fn + 5, flags);
  return (file_t *) host_file_openat(dirfd, fn, flags, mode);
}

int fd_close(int fd)
{
  file_t* f = file_get(fd);
  if (!f)
    return -1;
  file_t* old = atomic_cas(&fds[fd], f, 0);
  file_decref(f);
  if (old != f)
    return -1;
  file_decref(f);
  return 0;
}

ssize_t file_read(file_t* f, void* buf, size_t size)
{
  populate_mapping(buf, size, PROT_WRITE);
  if (f->typ == FILE_HOST) {
    host_file_t *hf = (host_file_t *) f;
    return frontend_syscall(SYS_read, hf->kfd, (uintptr_t)buf, size, 0, 0, 0, 0);
  }
  panic("read not supported on device file\n");
}

ssize_t file_pread(file_t* f, void* buf, size_t size, off_t offset)
{
  populate_mapping(buf, size, PROT_WRITE);
  if (f->typ == FILE_HOST) {
    host_file_t *hf = (host_file_t *) f;
    return frontend_syscall(SYS_pread, hf->kfd, (uintptr_t)buf, size, offset, 0, 0, 0);
  }
  panic("pread not supported on device file\n");
}

ssize_t file_write(file_t* f, const void* buf, size_t size)
{
  populate_mapping(buf, size, PROT_READ);
  if (f->typ == FILE_HOST) {
    host_file_t *hf = (host_file_t *) f;
    return frontend_syscall(SYS_write, hf->kfd, (uintptr_t)buf, size, 0, 0, 0, 0);
  }
  panic("write not supported on device file\n");
}

ssize_t file_pwrite(file_t* f, const void* buf, size_t size, off_t offset)
{
  populate_mapping(buf, size, PROT_READ);
  if (f->typ == FILE_HOST) {
    host_file_t *hf = (host_file_t *) f;
    return frontend_syscall(SYS_pwrite, hf->kfd, (uintptr_t)buf, size, offset, 0, 0, 0);
  }
  panic("pwrite not supported on device file\n");
}

int file_stat(file_t* f, struct stat* s)
{
  populate_mapping(s, sizeof(*s), PROT_WRITE);
  if (f->typ == FILE_HOST) {
    host_file_t *hf = (host_file_t *) f;
    return frontend_syscall(SYS_fstat, hf->kfd, (uintptr_t)s, 0, 0, 0, 0, 0);
  }
  panic("stat not supported on device file\n");
}

int file_truncate(file_t* f, off_t len)
{
  if (f->typ == FILE_HOST) {
    host_file_t *hf = (host_file_t *) f;
    return frontend_syscall(SYS_ftruncate, hf->kfd, len, 0, 0, 0, 0, 0);
  }
  panic("stat not supported on device file\n");
}

ssize_t file_lseek(file_t* f, size_t ptr, int dir)
{
  if (f->typ == FILE_HOST) {
    host_file_t *hf = (host_file_t *) f;
    return frontend_syscall(SYS_lseek, hf->kfd, ptr, dir, 0, 0, 0, 0);
  }
  panic("stat not supported on device file\n");
}
