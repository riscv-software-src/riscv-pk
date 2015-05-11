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
#ifdef __riscv64
  if (!user64)
    panic("can't run 32-bit ELF on 64-bit pk");
#else
  if (user64)
    panic("can't run 64-bit ELF on 32-bit pk");
#endif
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

void boot_loader(struct mainvars* args)
{
  // load program named by argv[0]
  long phdrs[128];
  current.phdr = (uintptr_t)phdrs;
  current.phdr_size = sizeof(phdrs);
  if (!args->argc)
    panic("tell me what ELF to load!");
  load_elf((char*)(uintptr_t)args->argv[0], &current);

  run_loaded_program(args);
}
