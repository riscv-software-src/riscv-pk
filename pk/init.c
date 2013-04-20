// See LICENSE for license details.

#include "pcr.h"
#include "pk.h"
#include "file.h"
#include "frontend.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

static void vsprintk(char* out, const char* s, va_list vl)
{
  bool format = false;
  bool longarg = false;
  for( ; *s; s++)
  {
    if(format)
    {
      switch(*s)
      {
        case 'l':
          longarg = true;
          break;
        case 'x':
        {
          long n = longarg ? va_arg(vl,long) : va_arg(vl,int);
          for(int i = 2*(longarg ? sizeof(long) : sizeof(int))-1; i >= 0; i--)
          {
            int d = (n >> (4*i)) & 0xF;
            *out++ = (d < 10 ? '0'+d : 'a'+d-10);
          }
          longarg = false;
          format = false;
          break;
        }
        case 'd':
        {
          long n = longarg ? va_arg(vl,long) : va_arg(vl,int);
          if(n < 0)
          {
            n = -n;
            *out++ = '-';
          }
          long digits = 1;
          for(long nn = n ; nn /= 10; digits++);
          for(int i = digits-1; i >= 0; i--)
          {
            out[i] = '0' + n%10;
            n /= 10;
          }
          out += digits;
          longarg = false;
          format = false;
          break;
        }
        case 's':
        {
          const char* s2 = va_arg(vl,const char*);
          while(*s2)
            *out++ = *s2++;
          longarg = false;
          format = false;
          break;
        }
        case 'c':
        {
          *out++ = (char)va_arg(vl,int);
          longarg = false;
          format = false;
          break;
        }
        default:
          panic("bad fmt");
      }
    }
    else if(*s == '%')
      format = true;
    else
      *out++ = *s;
  }
  *out++ = '\0';
}

void printk(const char* s, ...)
{
  va_list vl;
  va_start(vl,s);

  char out[1024]; // XXX
  vsprintk(out,s,vl);
  file_write(stderr,out,strlen(out));

  va_end(vl);
}

void sprintk(char* out, const char* s, ...)
{
  va_list vl;
  va_start(vl,s);

  vsprintk(out,s,vl);

  va_end(vl);
}

void dump_tf(trapframe_t* tf)
{
  static const char* regnames[] = {
    "z ", "ra", "s0", "s1", "s2", "s3", "s4", "s5",
    "s6", "s7", "s8", "s9", "sA", "sB", "sp", "tp",
    "v0", "v1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "a8", "a9", "aA", "aB", "aC", "aD"
  };

  tf->gpr[0] = 0;

  for(int i = 0; i < 32; i+=4)
  {
    for(int j = 0; j < 4; j++)
      printk("%s %lx%c",regnames[i+j],tf->gpr[i+j],j < 3 ? ' ' : '\n');
  }
  printk("sr %lx pc %lx va %lx insn       %x\n",tf->sr,tf->epc,tf->badvaddr,
         (uint32_t)tf->insn);
}

void init_tf(trapframe_t* tf, long pc, long sp, int user64)
{
  memset(tf,0,sizeof(*tf));
  if(sizeof(void*) != 8)
    kassert(!user64);
  tf->sr = (mfpcr(PCR_SR) & (SR_IM | SR_S64)) | SR_S | SR_EC;
  if(user64)
    tf->sr |= SR_U64;
  tf->gpr[14] = sp;
  tf->epc = pc;
}

static void bss_init()
{
  // front-end server zeroes the bss automagically
}

struct args
{
  uint64_t argc;
  uint64_t argv[];
};

static struct args* stack_init(unsigned long* stack_top)
{
  *stack_top -= USER_MAINVARS_SIZE;

  struct args* args = (struct args*)(*stack_top - sizeof(args->argc));
  sysret_t r = frontend_syscall(SYS_getmainvars, (long)args, USER_MAINVARS_SIZE, 0, 0);
  kassert(r.result == 0);
  
  // chop off argv[0]
  args->argv[0] = args->argc-1;
  return (struct args*)args->argv;
}

static void jump_usrstart(const char* fn, long sp)
{
  trapframe_t tf;

  int user64;
  long start = load_elf(fn, &user64);
  __clear_cache(0, 0);

  init_tf(&tf, start, sp, user64);
  pop_tf(&tf);
}

uint32_t mem_mb;

void boot()
{
  bss_init();
  file_init();

  // word 0 of memory contains # of MB of memory
  mem_mb = *(uint32_t*)0;

  unsigned long stack_top = 0x80000000;
  if (mem_mb < stack_top / (1024 * 1024))
    stack_top = mem_mb * (1024 * 1024);

  struct args* args = stack_init(&stack_top);
  jump_usrstart((char*)(long)args->argv[0], stack_top);
}
