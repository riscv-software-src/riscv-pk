#include "pk.h"
#include "file.h"
#include "frontend.h"
#include <stdarg.h>
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
            *out++ = (d < 10 ? '0'+d : 'A'+d-10);
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
  cons_write(out,strlen(out));

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
    "z ", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "a4", "a5", "a6", "a7", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "s8", "ra"
  };

  tf->gpr[0] = 0;

  for(int i = 0; i < 32; i+=4)
  {
    for(int j = 0; j < 4; j++)
      printk("%s %lx%c",regnames[i+j],tf->gpr[i+j],j < 3 ? ' ' : '\n');
  }
  printk("sr %lx pc %lx va %lx\n",tf->sr,tf->epc,tf->badvaddr);
}

void unhandled_trap(trapframe_t* tf)
{
  dump_tf(tf);
  panic("unhandled trap!");
}

void handle_breakpoint(trapframe_t* tf)
{
  printk("Breakpoint\n");
  dump_tf(tf);
  tf->epc += 4;
  pop_tf(tf);
}

void bss_init()
{
  extern char edata,end;
  memset(&edata,0,&end-&edata);
}

void mainvars_init()
{
  sysret_t r = frontend_syscall(SYS_getmainvars,
    USER_MEM_SIZE-USER_MAINVARS_SIZE, USER_MAINVARS_SIZE, 0, 0);

  kassert(r.result == 0);
}

void jump_usrstart()
{
  trapframe_t tf;
  memset(&tf,0,sizeof(tf));
  tf.gpr[29] = USER_MEM_SIZE-USER_MAINVARS_SIZE;
  tf.epc = USER_START;
  pop_tf(&tf);
}

void boot()
{
  bss_init();
  file_init();
  mainvars_init();
  jump_usrstart();
}
