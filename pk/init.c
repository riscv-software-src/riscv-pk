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

void init_tf(trapframe_t* tf, long pc, long sp)
{
  memset(tf, 0, sizeof(*tf));
  tf->status = (read_csr(sstatus) &~ SSTATUS_SPP &~ SSTATUS_SIE) | SSTATUS_SPIE;
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

    default:
      panic("unrecognized option: `%c'", s[1]);
      break;
  }
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

void prepare_supervisor_mode()
{
  uintptr_t mstatus = read_csr(mstatus);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_MPP, PRV_S);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_MPIE, 0);
  write_csr(mstatus, mstatus);
}
