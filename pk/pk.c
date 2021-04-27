// See LICENSE for license details.

#include "pk.h"
#include "mmap.h"
#include "boot.h"
#include "elf.h"
#include "mtrap.h"
#include "frontend.h"
#include "bits.h"
#include "usermem.h"
#include "flush_icache.h"
#include <stdbool.h>

elf_info current;
long disabled_hart_mask;

static void help()
{
  printk("Proxy kernel\n\n");
  printk("usage: pk [pk options] <user program> [program options]\n");
  printk("Options:\n");
  printk("  -h, --help            Print this help message\n");
  printk("  -p                    Disable on-demand program paging\n");
  printk("  -s                    Print cycles upon termination\n");

  shutdown(0);
}

static void suggest_help()
{
  printk("Try 'pk --help' for more information.\n");
  shutdown(1);
}

static void handle_option(const char* arg)
{
  if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
    help();
    return;
  }

  if (strcmp(arg, "-s") == 0) {  // print cycle count upon termination
    current.cycle0 = 1;
    return;
  }

  if (strcmp(arg, "-p") == 0) { // disable demand paging
    demand_paging = 0;
    return;
  }

  if (strcmp(arg, "--randomize-mapping") == 0) {
    randomize_mapping = 1;
    return;
  }

  panic("unrecognized option: `%s'", arg);
  suggest_help();
}

#define MAX_ARGS 256
typedef union {
  uint64_t buf[MAX_ARGS];
  char* argv[MAX_ARGS];
} arg_buf;

static size_t parse_args(arg_buf* args)
{
  long r = frontend_syscall(SYS_getmainvars, kva2pa(args), sizeof(*args), 0, 0, 0, 0, 0);
  if (r != 0)
    panic("args must not exceed %d bytes", (int)sizeof(arg_buf));

  kassert(r == 0);
  uint64_t* pk_argv = &args->buf[1];
  // pk_argv[0] is the proxy kernel itself.  skip it and any flags.
  size_t pk_argc = args->buf[0], arg = 1;
  for ( ; arg < pk_argc && *(char*)pa2kva(pk_argv[arg]) == '-'; arg++)
    handle_option((const char*)pa2kva(pk_argv[arg]));

  for (size_t i = 0; arg + i < pk_argc; i++)
    args->argv[i] = (char*)pa2kva(pk_argv[arg + i]);
  return pk_argc - arg;
}

static void init_tf(trapframe_t* tf, long pc, long sp)
{
  memset(tf, 0, sizeof(*tf));
  tf->status = (read_csr(sstatus) &~ SSTATUS_SPP &~ SSTATUS_SIE) | SSTATUS_SPIE;
  tf->gpr[2] = sp;
  tf->epc = pc;
}

static void run_loaded_program(size_t argc, char** argv, uintptr_t kstack_top)
{
  size_t mem_pages = mem_size >> RISCV_PGSHIFT;
  size_t stack_size = MIN(mem_pages >> 5, 2048) * RISCV_PGSIZE;
  size_t stack_bottom = __do_mmap(current.mmap_max - stack_size, stack_size, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, 0, 0);
  kassert(stack_bottom != (uintptr_t)-1);
  current.stack_top = stack_bottom + stack_size;

  // copy phdrs to user stack
  size_t stack_top = current.stack_top - current.phdr_size;
  memcpy_to_user((void*)stack_top, (void*)current.phdr, current.phdr_size);
  current.phdr = stack_top;

  // copy argv to user stack
  for (size_t i = 0; i < argc; i++) {
    size_t len = strlen((char*)(uintptr_t)argv[i])+1;
    stack_top -= len;
    memcpy_to_user((void*)stack_top, (void*)(uintptr_t)argv[i], len);
    argv[i] = (void*)stack_top;
  }

  // copy envp to user stack
  const char* envp[] = {
    // environment goes here
  };
  size_t envc = sizeof(envp) / sizeof(envp[0]);
  for (size_t i = 0; i < envc; i++) {
    size_t len = strlen(envp[i]) + 1;
    stack_top -= len;
    memcpy_to_user((void*)stack_top, envp[i], len);
    envp[i] = (void*)stack_top;
  }

  // align stack
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
    type __tmp = (type)(value); \
    memcpy_to_user(sp, &__tmp, sizeof(type)); \
    sp ++; \
  } while (0)

  #define STACK_INIT(type) do { \
    unsigned naux = sizeof(aux)/sizeof(aux[0]); \
    stack_top -= (1 + argc + 1 + envc + 1 + 2*naux) * sizeof(type); \
    stack_top &= -16; \
    type *sp = (void*)stack_top; \
    PUSH_ARG(int, argc); \
    for (unsigned i = 0; i < argc; i++) \
      PUSH_ARG(type, argv[i]); \
    PUSH_ARG(type, 0); /* argv[argc] = NULL */ \
    for (unsigned i = 0; i < envc; i++) \
      PUSH_ARG(type, envp[i]); \
    PUSH_ARG(type, 0); /* envp[envc] = NULL */ \
    for (unsigned i = 0; i < naux; i++) { \
      PUSH_ARG(type, aux[i].key); \
      PUSH_ARG(type, aux[i].value); \
    } \
  } while (0)

  STACK_INIT(uintptr_t);

  if (current.cycle0) { // start timer if so requested
    current.time0 = rdtime64();
    current.cycle0 = rdcycle64();
    current.instret0 = rdinstret64();
  }

  trapframe_t tf;
  init_tf(&tf, current.entry, stack_top);
  __riscv_flush_icache();
  write_csr(sscratch, kstack_top);
  start_user(&tf);
}

void rest_of_boot_loader(uintptr_t kstack_top);

asm ("\n\
  .globl rest_of_boot_loader\n\
rest_of_boot_loader:\n\
  mv sp, a0\n\
  tail rest_of_boot_loader_2");

void rest_of_boot_loader_2(uintptr_t kstack_top)
{
  file_init();

  static arg_buf args; // avoid large stack allocation
  size_t argc = parse_args(&args);
  if (!argc)
    panic("tell me what ELF to load!");

  // load program named by argv[0]
  static long phdrs[128]; // avoid large stack allocation
  current.phdr = (uintptr_t)phdrs;
  current.phdr_size = sizeof(phdrs);
  load_elf(args.argv[0], &current);

  run_loaded_program(argc, args.argv, kstack_top);
}

void boot_loader(uintptr_t dtb)
{
  uintptr_t kernel_stack_top = pk_vm_init();

  extern char trap_entry;
  write_csr(stvec, pa2kva(&trap_entry));
  write_csr(sscratch, 0);
  write_csr(sie, 0);
  set_csr(sstatus, SSTATUS_FS | SSTATUS_VS);

  enter_supervisor_mode((void*)pa2kva(rest_of_boot_loader), pa2kva(kernel_stack_top), 0);
}

void boot_other_hart(uintptr_t dtb)
{
  // stall all harts besides hart 0
  while (1)
    wfi();
}
