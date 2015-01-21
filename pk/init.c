// See LICENSE for license details.

#include "pk.h"
#include "file.h"
#include "vm.h"
#include "frontend.h"
#include "elf.h"
#include <stdint.h>
#include <string.h>

elf_info current;
int have_vm = 1; // unless -p flag is given
int have_fp;
int have_accelerator;

int uarch_counters_enabled;
long uarch_counters[NUM_COUNTERS];
char* uarch_counter_names[NUM_COUNTERS];

void init_tf(trapframe_t* tf, long pc, long sp, int user64)
{
  memset(tf,0,sizeof(*tf));
  if(sizeof(void*) != 8)
    kassert(!user64);
  tf->sr = (read_csr(status) & (SR_IM | SR_S64 | SR_VM)) | SR_S | SR_PEI;
  if(user64)
    tf->sr |= SR_U64;
  tf->gpr[2] = sp;
  tf->epc = pc;
}

static void handle_option(const char* s)
{
  switch (s[1])
  {
    case 's': // print cycle count upon termination
      current.t0 = 1;
      break;

    case 'c': // print uarch counters upon termination
              // If your HW doesn't support uarch counters, then don't use this flag!
      uarch_counters_enabled = 1;
      break;

    case 'p': // physical memory mode
      have_vm = 0;
      break;

    default:
      panic("unrecognized option: `%c'", s[1]);
      break;
  }
}

struct mainvars {
  uint64_t argc;
  uint64_t argv[127]; // this space is shared with the arg strings themselves
};

static struct mainvars* handle_args(struct mainvars* args)
{
  long r = frontend_syscall(SYS_getmainvars, (uintptr_t)args, sizeof(*args), 0, 0, 0, 0, 0);
  kassert(r == 0);

  // argv[0] is the proxy kernel itself.  skip it and any flags.
  unsigned a0 = 1;
  for ( ; a0 < args->argc && *(char*)(uintptr_t)args->argv[a0] == '-'; a0++)
    handle_option((const char*)(uintptr_t)args->argv[a0]);
  args->argv[a0-1] = args->argc - a0;
  return (struct mainvars*)&args->argv[a0-1];
}

static void user_init(struct mainvars* args)
{
  // copy argv to user stack
  size_t stack_top = current.stack_top;
  for (size_t i = 0; i < args->argc; i++) {
    size_t len = strlen((char*)(uintptr_t)args->argv[i])+1;
    stack_top -= len;
    memcpy((void*)stack_top, (void*)(uintptr_t)args->argv[i], len);
    args->argv[i] = stack_top;
  }
  stack_top &= -sizeof(void*);
  populate_mapping((void*)stack_top, current.stack_top - stack_top, PROT_WRITE);

  // load program named by argv[0]
  current.phdr_top = stack_top;
  load_elf((char*)(uintptr_t)args->argv[0], &current);
  stack_top = current.phdr;

  struct {
    long key;
    long value;
  } aux[] = {
    {AT_ENTRY, current.entry},
    {AT_PHNUM, current.phnum},
    {AT_PHENT, current.phent},
    {AT_PHDR, current.phdr},
    {AT_PAGESZ, RISCV_PGSIZE},
    {AT_SECURE, 0},
    {AT_RANDOM, stack_top},
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

  if (current.t0) // start timer if so requested
    current.t0 = rdcycle();

  if (uarch_counters_enabled) { // start tracking the uarch counters if requested
    size_t i = 0;
    #define READ_CTR_INIT(name) do { \
      while (i >= NUM_COUNTERS) ; \
      long csr = read_csr(name); \
      uarch_counters[i++] = csr; \
    } while (0)
    READ_CTR_INIT(cycle);   READ_CTR_INIT(instret);
    READ_CTR_INIT(uarch0);  READ_CTR_INIT(uarch1);  READ_CTR_INIT(uarch2);
    READ_CTR_INIT(uarch3);  READ_CTR_INIT(uarch4);  READ_CTR_INIT(uarch5);
    READ_CTR_INIT(uarch6);  READ_CTR_INIT(uarch7);  READ_CTR_INIT(uarch8);
    READ_CTR_INIT(uarch9);  READ_CTR_INIT(uarch10); READ_CTR_INIT(uarch11);
    READ_CTR_INIT(uarch12); READ_CTR_INIT(uarch13); READ_CTR_INIT(uarch14);
    READ_CTR_INIT(uarch15);
    #undef READ_CTR_INIT
  }

  trapframe_t tf;
  init_tf(&tf, current.entry, stack_top, current.elf64);
  __clear_cache(0, 0);
  pop_tf(&tf);
}

void boot()
{
  file_init();
  struct mainvars args0;
  struct mainvars* args = handle_args(&args0);
  vm_init();
  user_init(args);
}
