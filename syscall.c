#include <machine/syscall.h>
#include <string.h>
#include <errno.h>
#include "pk.h"
#include "file.h"

typedef sysret_t (*syscall_t)(long,long,long,long,long);

sysret_t sys_exit(int code)
{
  panic("bye!");
}

sysret_t sys_write(int fd, char* buf, size_t n)
{
  sysret_t r = {-1,EBADF};
  file_t* f = file_get(fd);
  if(!f)
    return r;

  return file_write(f,buf,n);
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

void handle_syscall(trapframe_t* tf)
{
  const static void* syscall_table[] = {
    [SYS_exit] = sys_exit,
    [SYS_write] = sys_write,
    [SYS_close] = sys_close,
    [SYS_fstat] = sys_fstat,
  };

  syscall_t p;
  unsigned long n = tf->gpr[2];
  if(n >= sizeof(syscall_table)/sizeof(void*) || !syscall_table[n])
  {
    dump_tf(tf);
    panic("bad syscall #%ld!",n);
  }
  else
    p = (syscall_t)syscall_table[n];

  sysret_t ret = p(tf->gpr[4],tf->gpr[5],tf->gpr[6],tf->gpr[7],n);
  tf->gpr[2] = ret.result;
  tf->gpr[3] = ret.result == -1 ? ret.err : 0;

  //printk("syscall %d (%x,%x,%x,%x) from %x == %d\n",n,tf->gpr[4],tf->gpr[5],tf->gpr[6],tf->gpr[7],tf->gpr[31],tf->gpr[2]);

  tf->epc += 4;
  pop_tf(tf);
}
