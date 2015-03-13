// See LICENSE for license details.

#include "pk.h"
#include "file.h"
#include "vm.h"
#include "frontend.h"
#include "elf.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

elf_info current;
int have_vm = 1; // unless -p flag is given

int uarch_counters_enabled;
long uarch_counters[NUM_COUNTERS];
char* uarch_counter_names[NUM_COUNTERS];

void init_tf(trapframe_t* tf, long pc, long sp, int user64)
{
  memset(tf, 0, sizeof(*tf));
  if (user64) {
    kassert(sizeof(void*) == 8);
    set_csr(sstatus, UA_RV64 * (SSTATUS_UA & ~(SSTATUS_UA << 1)));
  }
  tf->status = read_csr(sstatus);
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

    case 'm': // memory capacity in MiB
    {
      uintptr_t mem_mb = atol(&s[2]);
      if (!mem_mb)
        goto need_nonzero_int;
      mem_size = mem_mb << 20;
      if ((mem_size >> 20) < mem_mb)
        mem_size = (typeof(mem_size))-1 & -RISCV_PGSIZE;
      break;
    }

    case 'p': // number of harts
      num_harts = atol(&s[2]);
      if (!num_harts)
        goto need_nonzero_int;
      break;

    default:
      panic("unrecognized option: `%c'", s[1]);
      break;
  }
  return;

need_nonzero_int:
  panic("the -%c flag requires a nonzero argument", s[1]);
}

struct mainvars* parse_args(struct mainvars* args)
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

uintptr_t boot_loader(struct mainvars* args)
{
  // load program named by argv[0]
  long phdrs[128];
  current.phdr = (uintptr_t)phdrs;
  current.phdr_size = sizeof(phdrs);
  if (!args->argc)
    panic("tell me what ELF to load!");
  load_elf((char*)(uintptr_t)args->argv[0], &current);

  if (current.is_supervisor) {
    supervisor_vm_init();
    write_csr(mepc, current.entry);
    asm volatile("mret");
    __builtin_unreachable();
  }

  pk_vm_init();
  asm volatile("la t0, 1f; csrw mepc, t0; mret; 1:" ::: "t0");

  // copy phdrs to user stack
  size_t stack_top = current.stack_top - current.phdr_size;
  memcpy((void*)stack_top, (void*)current.phdr, current.phdr_size);
  current.phdr = stack_top;

  // copy argv to user stack
  for (size_t i = 0; i < args->argc; i++) {
    size_t len = strlen((char*)(uintptr_t)args->argv[i])+1;
    stack_top -= len;
    memcpy((void*)stack_top, (void*)(uintptr_t)args->argv[i], len);
    args->argv[i] = stack_top;
  }
  stack_top &= -sizeof(void*);

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
