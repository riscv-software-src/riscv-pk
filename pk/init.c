// See LICENSE for license details.

#include "pcr.h"
#include "pk.h"
#include "file.h"
#include "vm.h"
#include "frontend.h"
#include "elf.h"
#include <stdint.h>
#include <string.h>

elf_info current;

void init_tf(trapframe_t* tf, long pc, long sp, int user64)
{
  memset(tf,0,sizeof(*tf));
  if(sizeof(void*) != 8)
    kassert(!user64);
  tf->sr = (mfpcr(PCR_SR) & (SR_IM | SR_S64 | SR_VM)) | SR_S | SR_PEI;
  if(user64)
    tf->sr |= SR_U64;
  tf->gpr[14] = sp;
  tf->epc = pc;
}

static void user_init()
{
  struct args {
    uint64_t argc;
    uint64_t argv[];
  };

  const int argc_argv_size = 1024;
  size_t stack_top = current.stack_top;
  struct args* args = (struct args*)(stack_top - argc_argv_size);
  populate_mapping(args, argc_argv_size, PROT_WRITE);
  sysret_t r = frontend_syscall(SYS_getmainvars, (long)args, argc_argv_size, 0, 0);
  kassert(r.result == 0);

  // argv[0] is the proxy kernel itself.  skip it.
  args->argv[0] = args->argc - 1;
  args = (struct args*)args->argv;
  stack_top = (uintptr_t)args;

  // load program named by argv[0]
  current.phdr_top = stack_top;
  load_elf((char*)args->argv[0], &current);

  struct {
    long key;
    long value;
  } aux[] = {
    {AT_ENTRY, current.entry},
    {AT_PHNUM, current.phnum},
    {AT_PHENT, current.phent},
    {AT_PHDR, current.phdr},
    {AT_PAGESZ, RISCV_PGSIZE},
    {AT_NULL, 0}
  };

  // place argc, argv, envp, auxp on stack
  #define PUSH_ARG(type, value) do { \
    *((type*)sp) = value; \
    sp += sizeof(type); \
  } while (0)

  #define STACK_INIT(type) do { \
    unsigned naux = sizeof(aux)/sizeof(aux[0]); \
    stack_top -= (1 + args->argc + 1 + 1 + 2*naux) * sizeof(type); \
    stack_top &= -16; \
    long sp = stack_top; \
    PUSH_ARG(type, args->argc); \
    for (unsigned i = 0; i < args->argc; i++) \
      PUSH_ARG(type, args->argv[i]); \
    PUSH_ARG(type, 0); /* argv[argc] = NULL */ \
    PUSH_ARG(type, 0); /* envp[0] = NULL */ \
    for (unsigned i = 0; i < naux; i++) { \
      PUSH_ARG(type, aux[i].key); \
      PUSH_ARG(type, aux[i].value); \
    } \
  } while (0)

  if (current.elf64)
    STACK_INIT(uint64_t);
  else
    STACK_INIT(uint32_t);

  trapframe_t tf;
  init_tf(&tf, current.entry, stack_top, current.elf64);
  __clear_cache(0, 0);
  pop_tf(&tf);
}

void boot()
{
  file_init();
  vm_init();
  user_init();
}
