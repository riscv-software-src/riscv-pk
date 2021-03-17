// See LICENSE for license details.

#include "syscall.h"
#include "pk.h"
#include "file.h"
#include "bits.h"
#include "frontend.h"
#include "mmap.h"
#include "boot.h"
#include "usermem.h"
#include <string.h>
#include <errno.h>

typedef long (*syscall_t)(long, long, long, long, long, long, long);

#define CLOCK_FREQ 1000000000

#define MAX_BUF 512

void sys_exit(int code)
{
  if (current.cycle0) {
    uint64_t dt = rdtime64() - current.time0;
    uint64_t dc = rdcycle64() - current.cycle0;
    uint64_t di = rdinstret64() - current.instret0;

    printk("%lld ticks\n", dt);
    printk("%lld cycles\n", dc);
    printk("%lld instructions\n", di);
    printk("%d.%d%d CPI\n", (int)(dc/di), (int)(10ULL*dc/di % 10),
        (int)((100ULL*dc)/di % 10));
  }
  shutdown(code);
}

ssize_t sys_read(int fd, char* buf, size_t n)
{
  ssize_t r = -EBADF;
  file_t* f = file_get(fd);
  char kbuf[MAX_BUF];

  if (f) {
    for (size_t total = 0; ; ) {
      size_t cur = MIN(n - total, MAX_BUF);
      r = file_read(f, kbuf, cur);
      if (r < 0)
        break;

      memcpy_to_user(buf, kbuf, r);

      total += r;
      buf += r;
      if (r < cur || total == n) {
        r = total;
        break;
      }
    }

    file_decref(f);
  }

  return r;
}

ssize_t sys_pread(int fd, char* buf, size_t n, off_t offset)
{
  ssize_t r = -EBADF;
  file_t* f = file_get(fd);
  char kbuf[MAX_BUF];

  if (f) {
    for (size_t total = 0; ; ) {
      size_t cur = MIN(n - total, MAX_BUF);
      r = file_pread(f, kbuf, cur, offset);
      if (r < 0)
        break;

      memcpy_to_user(buf, kbuf, r);

      total += r;
      buf += r;
      offset += r;
      if (r < cur || total == n) {
        r = total;
        break;
      }
    }

    file_decref(f);
  }

  return r;
}

ssize_t sys_write(int fd, const char* buf, size_t n)
{
  ssize_t r = -EBADF;
  file_t* f = file_get(fd);
  char kbuf[MAX_BUF];

  if (f) {
    for (size_t total = 0; ; ) {
      size_t cur = MIN(n - total, MAX_BUF);
      memcpy_from_user(kbuf, buf, cur);

      r = file_write(f, kbuf, cur);

      if (r < 0)
        break;

      total += r;
      buf += r;
      if (r < cur || total == n) {
        r = total;
        break;
      }
    }

    file_decref(f);
  }

  return r;
}

static int at_kfd(int dirfd)
{
  if (dirfd == AT_FDCWD)
    return AT_FDCWD;
  file_t* dir = file_get(dirfd);
  if (dir == NULL)
    return -1;
  return dir->kfd;
}

int sys_openat(int dirfd, const char* name, int flags, int mode)
{
  int kfd = at_kfd(dirfd);
  if (kfd != -1) {
    char kname[MAX_BUF];
    if (!strcpy_from_user(kname, name, MAX_BUF))
      return -ENAMETOOLONG;

    file_t* file = file_openat(kfd, kname, flags, mode);
    if (IS_ERR_VALUE(file))
      return PTR_ERR(file);

    int fd = file_dup(file);
    if (fd < 0) {
      file_decref(file);
      return -ENOMEM;
    }

    return fd;
  }
  return -EBADF;
}

int sys_open(const char* name, int flags, int mode)
{
  return sys_openat(AT_FDCWD, name, flags, mode);
}

int sys_close(int fd)
{
  int ret = fd_close(fd);
  if (ret < 0)
    return -EBADF;
  return ret;
}

int sys_renameat(int old_fd, const char *old_path, int new_fd, const char *new_path) {

  int old_kfd = at_kfd(old_fd);
  int new_kfd = at_kfd(new_fd);
  if(old_kfd != -1 && new_kfd != -1) {
    char kold_path[MAX_BUF], knew_path[MAX_BUF];
    if (!strcpy_from_user(kold_path, old_path, MAX_BUF) || !strcpy_from_user(knew_path, new_path, MAX_BUF))
      return -ENAMETOOLONG;

    size_t old_size = strlen(kold_path)+1;
    size_t new_size = strlen(knew_path)+1;

    return frontend_syscall(SYS_renameat, old_kfd, kva2pa(kold_path), old_size,
                                           new_kfd, kva2pa(knew_path), new_size, 0);
  }
  return -EBADF;
}

int sys_fstat(int fd, void* st)
{
  int r = -EBADF;
  file_t* f = file_get(fd);

  if (f)
  {
    struct frontend_stat buf;
    r = frontend_syscall(SYS_fstat, f->kfd, kva2pa(&buf), 0, 0, 0, 0, 0);
    memcpy_to_user(st, &buf, sizeof(buf));
    file_decref(f);
  }

  return r;
}

int sys_fcntl(int fd, int cmd, int arg)
{
  int r = -EBADF;
  file_t* f = file_get(fd);

  if (f)
  {
    r = frontend_syscall(SYS_fcntl, f->kfd, cmd, arg, 0, 0, 0, 0);
    file_decref(f);
  }

  return r;
}

int sys_ftruncate(int fd, off_t len)
{
  int r = -EBADF;
  file_t* f = file_get(fd);

  if (f)
  {
    r = file_truncate(f, len);
    file_decref(f);
  }

  return r;
}

int sys_dup(int fd)
{
  int r = -EBADF;
  file_t* f = file_get(fd);

  if (f)
  {
    r = file_dup(f);
    file_decref(f);
  }

  return r;
}

int sys_dup3(int fd, int newfd, int flags)
{
  kassert(flags == 0);
  int r = -EBADF;
  file_t* f = file_get(fd);

  if (f)
  {
    r = file_dup3(f, newfd);
    file_decref(f);
  }

  return r;
}

ssize_t sys_lseek(int fd, size_t ptr, int dir)
{
  ssize_t r = -EBADF;
  file_t* f = file_get(fd);

  if (f)
  {
    r = file_lseek(f, ptr, dir);
    file_decref(f);
  }

  return r;
}

long sys_lstat(const char* name, void* st)
{
  struct frontend_stat buf;

  char kname[MAX_BUF];
  if (!strcpy_from_user(kname, name, MAX_BUF))
    return -ENAMETOOLONG;

  size_t name_size = strlen(kname)+1;

  long ret = frontend_syscall(SYS_lstat, kva2pa(kname), name_size, kva2pa(&buf), 0, 0, 0, 0);
  memcpy(st, &buf, sizeof(buf));
  return ret;
}

long sys_fstatat(int dirfd, const char* name, void* st, int flags)
{
  int kfd = at_kfd(dirfd);
  if (kfd != -1) {
    struct frontend_stat buf;

    char kname[MAX_BUF];
    if (!strcpy_from_user(kname, name, MAX_BUF))
      return -ENAMETOOLONG;

    size_t name_size = strlen(kname)+1;

    long ret = frontend_syscall(SYS_fstatat, kfd, kva2pa(kname), name_size, kva2pa(&buf), flags, 0, 0);
    memcpy_to_user(st, &buf, sizeof(buf));
    return ret;
  }
  return -EBADF;
}

long sys_stat(const char* name, void* st)
{
  return sys_fstatat(AT_FDCWD, name, st, 0);
}

long sys_statx(int dirfd, const char* name, int flags, unsigned int mask, void * st)
{
  int kfd = at_kfd(dirfd);
  if (kfd != -1) {
    char buf[FRONTEND_STATX_SIZE];

    char kname[MAX_BUF];
    if (!strcpy_from_user(kname, name, MAX_BUF))
      return -ENAMETOOLONG;

    size_t name_size = strlen(kname)+1;

    long ret = frontend_syscall(SYS_statx, kfd, kva2pa(kname), name_size, flags, mask, kva2pa(&buf), 0);
    memcpy_to_user(st, &buf, sizeof(buf));
    return ret;
  }
  return -EBADF;
}

long sys_faccessat(int dirfd, const char *name, int mode)
{
  int kfd = at_kfd(dirfd);
  if (kfd != -1) {
    char kname[MAX_BUF];
    if (!strcpy_from_user(kname, name, MAX_BUF))
      return -ENAMETOOLONG;

    size_t name_size = strlen(kname)+1;

    return frontend_syscall(SYS_faccessat, kfd, kva2pa(kname), name_size, mode, 0, 0, 0);
  }
  return -EBADF;
}

long sys_access(const char *name, int mode)
{
  return sys_faccessat(AT_FDCWD, name, mode);
}

long sys_linkat(int old_dirfd, const char* old_name, int new_dirfd, const char* new_name, int flags)
{
  int old_kfd = at_kfd(old_dirfd);
  int new_kfd = at_kfd(new_dirfd);
  if (old_kfd != -1 && new_kfd != -1) {

    char kold_name[MAX_BUF], knew_name[MAX_BUF];
    if (!strcpy_from_user(kold_name, old_name, MAX_BUF) || !strcpy_from_user(knew_name, new_name, MAX_BUF))
      return -ENAMETOOLONG;

    size_t old_size = strlen(kold_name)+1;
    size_t new_size = strlen(knew_name)+1;

    return frontend_syscall(SYS_linkat, old_kfd, kva2pa(kold_name), old_size,
                                        new_kfd, kva2pa(knew_name), new_size,
                                        flags);
  }
  return -EBADF;
}

long sys_link(const char* old_name, const char* new_name)
{
  return sys_linkat(AT_FDCWD, old_name, AT_FDCWD, new_name, 0);
}

long sys_unlinkat(int dirfd, const char* name, int flags)
{
  int kfd = at_kfd(dirfd);
  if (kfd != -1) {
    char kname[MAX_BUF];
    if (!strcpy_from_user(kname, name, MAX_BUF))
      return -ENAMETOOLONG;

    size_t name_size = strlen(kname)+1;

    return frontend_syscall(SYS_unlinkat, kfd, kva2pa(kname), name_size, flags, 0, 0, 0);
  }
  return -EBADF;
}

long sys_unlink(const char* name)
{
  return sys_unlinkat(AT_FDCWD, name, 0);
}

long sys_mkdirat(int dirfd, const char* name, int mode)
{
  int kfd = at_kfd(dirfd);
  if (kfd != -1) {
    char kname[MAX_BUF];
    if (!strcpy_from_user(kname, name, MAX_BUF))
      return -ENAMETOOLONG;

    size_t name_size = strlen(kname)+1;

    return frontend_syscall(SYS_mkdirat, kfd, kva2pa(kname), name_size, mode, 0, 0, 0);
  }
  return -EBADF;
}

long sys_mkdir(const char* name, int mode)
{
  return sys_mkdirat(AT_FDCWD, name, mode);
}

long sys_getcwd(char* buf, size_t size)
{
  char kbuf[MAX_BUF];
  long ret = frontend_syscall(SYS_getcwd, kva2pa(kbuf), MIN(size, MAX_BUF), 0, 0, 0, 0, 0);
  if (ret == 0)
    memcpy_to_user(buf, kbuf, strlen(kbuf) + 1);
  return ret;
}

size_t sys_brk(size_t pos)
{
  return do_brk(pos);
}

int sys_uname(void* buf)
{
  const int sz = 65, sz_total = sz * 6;
  char kbuf[sz_total];
  memset(kbuf, 0, sz_total);

  strcpy(kbuf + 0*sz, "Proxy Kernel");
  strcpy(kbuf + 1*sz, "");
  strcpy(kbuf + 2*sz, "4.15.0");
  strcpy(kbuf + 3*sz, "");
  strcpy(kbuf + 4*sz, "");
  strcpy(kbuf + 5*sz, "");

  memcpy_to_user(buf, kbuf, sz_total);

  return 0;
}

pid_t sys_getpid()
{
  return 0;
}

int sys_getuid()
{
  return 0;
}

uintptr_t sys_mmap(uintptr_t addr, size_t length, int prot, int flags, int fd, off_t offset)
{
#if __riscv_xlen == 32
  if (offset != (offset << 12 >> 12))
    return -ENXIO;
  offset <<= 12;
#endif
  return do_mmap(addr, length, prot, flags, fd, offset);
}

int sys_munmap(uintptr_t addr, size_t length)
{
  return do_munmap(addr, length);
}

uintptr_t sys_mremap(uintptr_t addr, size_t old_size, size_t new_size, int flags)
{
  return do_mremap(addr, old_size, new_size, flags);
}

uintptr_t sys_mprotect(uintptr_t addr, size_t length, int prot)
{
  return do_mprotect(addr, length, prot);
}

int sys_rt_sigaction(int sig, const void* act, void* oact, size_t sssz)
{
  if (oact) {
    long koact[3] = {0};
    memcpy_to_user(oact, koact, sizeof(koact));
  }

  return 0;
}

long sys_time(long* loc)
{
  long t = (long)(rdcycle64() / CLOCK_FREQ);
  if (loc)
    memcpy_to_user(loc, &t, sizeof(t));
  return t;
}

int sys_times(long* loc)
{
  uint64_t t = rdcycle64();
  kassert(CLOCK_FREQ % 1000000 == 0);

  long kloc[4] = {0};
  kloc[0] = t / (CLOCK_FREQ / 1000000);

  memcpy_to_user(loc, kloc, sizeof(kloc));
  
  return 0;
}

int sys_gettimeofday(long* loc)
{
  uint64_t t = rdcycle64();

  long kloc[2];
  kloc[0] = t / CLOCK_FREQ;
  kloc[1] = (t % CLOCK_FREQ) / (CLOCK_FREQ / 1000000);

  memcpy_to_user(loc, kloc, sizeof(kloc));
  
  return 0;
}

long sys_clock_gettime(int clk_id, long *loc)
{
  uint64_t t = rdcycle64();

  long kloc[2];
  kloc[0] = t / CLOCK_FREQ;
  kloc[1] = (t % CLOCK_FREQ) / (CLOCK_FREQ / 1000000000);

  memcpy_to_user(loc, kloc, sizeof(kloc));

  return 0;
}

ssize_t sys_writev(int fd, const long* iov, int cnt)
{
  ssize_t ret = 0;
  for (int i = 0; i < cnt; i++) {
    long kiov[2];
    memcpy_from_user(kiov, iov + 2*i, 2*sizeof(long));

    ssize_t r = sys_write(fd, (void*)kiov[0], kiov[1]);
    if (r < 0)
      return r;
    ret += r;
  }
  return ret;
}

int sys_chdir(const char *path)
{
  char kbuf[MAX_BUF];
  if (!strcpy_from_user(kbuf, path, MAX_BUF))
    return -ENAMETOOLONG;

  return frontend_syscall(SYS_chdir, kva2pa(kbuf), 0, 0, 0, 0, 0, 0);
}

void sys_tgkill(int tgid, int tid, int sig)
{
  // assume target is current thread
  sys_exit(sig);
}

int sys_getdents(int fd, void* dirbuf, int count)
{
  return 0; //stub
}

static int sys_stub_success()
{
  return 0;
}

static int sys_stub_nosys()
{
  return -ENOSYS;
}

long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, unsigned long n)
{
  const static void* syscall_table[] = {
    [SYS_exit] = sys_exit,
    [SYS_exit_group] = sys_exit,
    [SYS_read] = sys_read,
    [SYS_pread] = sys_pread,
    [SYS_write] = sys_write,
    [SYS_openat] = sys_openat,
    [SYS_close] = sys_close,
    [SYS_fstat] = sys_fstat,
    [SYS_statx] = sys_statx,
    [SYS_lseek] = sys_lseek,
    [SYS_fstatat] = sys_fstatat,
    [SYS_linkat] = sys_linkat,
    [SYS_unlinkat] = sys_unlinkat,
    [SYS_mkdirat] = sys_mkdirat,
    [SYS_renameat] = sys_renameat,
    [SYS_getcwd] = sys_getcwd,
    [SYS_brk] = sys_brk,
    [SYS_uname] = sys_uname,
    [SYS_getpid] = sys_getpid,
    [SYS_getuid] = sys_getuid,
    [SYS_geteuid] = sys_getuid,
    [SYS_getgid] = sys_getuid,
    [SYS_getegid] = sys_getuid,
    [SYS_gettid] = sys_getuid,
    [SYS_tgkill] = sys_tgkill,
    [SYS_mmap] = sys_mmap,
    [SYS_munmap] = sys_munmap,
    [SYS_mremap] = sys_mremap,
    [SYS_mprotect] = sys_mprotect,
    [SYS_prlimit64] = sys_stub_nosys,
    [SYS_rt_sigaction] = sys_rt_sigaction,
    [SYS_gettimeofday] = sys_gettimeofday,
    [SYS_times] = sys_times,
    [SYS_writev] = sys_writev,
    [SYS_faccessat] = sys_faccessat,
    [SYS_fcntl] = sys_fcntl,
    [SYS_ftruncate] = sys_ftruncate,
    [SYS_getdents] = sys_getdents,
    [SYS_dup] = sys_dup,
    [SYS_dup3] = sys_dup3,
    [SYS_readlinkat] = sys_stub_nosys,
    [SYS_rt_sigprocmask] = sys_stub_success,
    [SYS_ioctl] = sys_stub_nosys,
    [SYS_clock_gettime] = sys_clock_gettime,
    [SYS_getrusage] = sys_stub_nosys,
    [SYS_getrlimit] = sys_stub_nosys,
    [SYS_setrlimit] = sys_stub_nosys,
    [SYS_chdir] = sys_chdir,
    [SYS_set_tid_address] = sys_stub_nosys,
    [SYS_set_robust_list] = sys_stub_nosys,
    [SYS_madvise] = sys_stub_nosys,
  };

  const static void* old_syscall_table[] = {
    [-OLD_SYSCALL_THRESHOLD + SYS_open] = sys_open,
    [-OLD_SYSCALL_THRESHOLD + SYS_link] = sys_link,
    [-OLD_SYSCALL_THRESHOLD + SYS_unlink] = sys_unlink,
    [-OLD_SYSCALL_THRESHOLD + SYS_mkdir] = sys_mkdir,
    [-OLD_SYSCALL_THRESHOLD + SYS_access] = sys_access,
    [-OLD_SYSCALL_THRESHOLD + SYS_stat] = sys_stat,
    [-OLD_SYSCALL_THRESHOLD + SYS_lstat] = sys_lstat,
    [-OLD_SYSCALL_THRESHOLD + SYS_time] = sys_time,
  };

  syscall_t f = 0;

  if (n < ARRAY_SIZE(syscall_table))
    f = syscall_table[n];
  else if (n - OLD_SYSCALL_THRESHOLD < ARRAY_SIZE(old_syscall_table))
    f = old_syscall_table[n - OLD_SYSCALL_THRESHOLD];

  if (!f)
    panic("bad syscall #%ld!",n);

  f = (void*)pa2kva(f);

  return f(a0, a1, a2, a3, a4, a5, n);
}
