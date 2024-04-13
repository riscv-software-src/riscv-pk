// See LICENSE for license details.

#include "pk.h"
#include "mtrap.h"
#include "boot.h"
#include "bits.h"
#include "elf.h"
#include "usermem.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "mmap.h"

// Used to make a named pipe
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <unistd.h> // Used to spawn new process

/**
 * The protection flags are in the p_flags section of the program header.
 * But rather annoyingly, they are the reverse of what mmap expects.
 */
static inline int get_prot(uint32_t p_flags)
{
  int prot_x = (p_flags & PF_X) ? PROT_EXEC : PROT_NONE;
  int prot_w = (p_flags & PF_W) ? PROT_WRITE : PROT_NONE;
  int prot_r = (p_flags & PF_R) ? PROT_READ : PROT_NONE;

  return (prot_x | prot_w | prot_r);
}

void load_elf(const char *fn, elf_info *info)
{
  file_t *file = file_open(fn, O_RDONLY, 0);
  if (IS_ERR_VALUE(file))
    goto fail;

  Elf_Ehdr eh;
  ssize_t ehdr_size = file_pread(file, &eh, sizeof(eh), 0);
  if (ehdr_size < (ssize_t)sizeof(eh) ||
      !(eh.e_ident[0] == '\177' && eh.e_ident[1] == 'E' &&
        eh.e_ident[2] == 'L' && eh.e_ident[3] == 'F'))
    goto fail;

#if __riscv_xlen == 64
  assert(IS_ELF64(eh));
#else
  assert(IS_ELF32(eh));
#endif

#ifndef __riscv_compressed
  assert(!(eh.e_flags & EF_RISCV_RVC));
#endif

  size_t phdr_size = eh.e_phnum * sizeof(Elf_Phdr);
  if (phdr_size > info->phdr_size)
    goto fail;
  ssize_t ret = file_pread(file, (void *)info->phdr, phdr_size, eh.e_phoff);
  if (ret < (ssize_t)phdr_size)
    goto fail;
  info->phnum = eh.e_phnum;               // Read directly from file, so no change?
  info->phent = sizeof(Elf_Phdr);         // Read directly from file, so no change?
  Elf_Phdr *ph = (typeof(ph))info->phdr;


  //----------------------------------------------------------------------------------------------
  file_decref(file); // close file before we do anything else

  // Set up our call to the Angr python script.
  // testScript.py is the script that starts and queries Angr to help dynamically load the program.
  
  // Pipe names and communication strings
  const char* loader = "loader_pipe";
  const char* PKWait = "PK_wait_pipe";
  const char* pythonWait = "Python_wait_pipe";
  const char* ready = "ready";
  const char* terminate = "terminate";

  // Open shared pipes
  mkfifo(loader, 0666); // 0666 is default permissions.
  mkfifo(PKWait, 0666);
  mkfifo(pythonWait, 0666); // Additional pipe to avoid races

  // Argument vector to be passed into child process.
  char *childArgv[] = {"testScript.py", fn, loader, PKWait, pythonWait, ready, terminate};
  
  // Fork child process to run python script
  int res;
  res = fork();
  if (!res)
  {
    execv(childArgv[0], childArgv);
  }

  // Points to the currently open pipe.
  FILE* fptr;
  // Max string length for any line. The number 255 is arbitrary.
  int strLen = 255;
  // Holds strings read from fifo pipe
  char output[strLen];
  // Minimum address of the next segment to be read.
  int minAddr;
  // Memory size of the next segment to be read.
  int memSize;


  // First value output by child process is entry point.
  fptr = fopen(loader, "r");    // Open file.
  if(!fgets(output, strLen, fptr)) goto fail;  // Read one line into output.
  fclose(fptr);

  info->entry = atoi(output);

  // Tell script to continue
  fptr = fopen(pythonWait, "w");
  puts(ready);
  fclose(fptr);

   // While output is not "terminate"
  while(!strcmp(output, terminate)){
    // Wait for script
    fptr = fopen(PKWait, "r");
    if(!fgets(output, strLen, fptr)) goto fail;
    fclose(fptr);

    fptr = fopen(loader, "r");
    if(!fgets(output, strLen, fptr)) goto fail;
    minAddr = atoi(output);
    if(!fgets(output, strLen, fptr)) goto fail;
    memSize = atoi(output);
    fclose(fptr);

    // Update floor for red zone
    if (minAddr + mem_size > info->brk_min)
        info->brk_min = minAddr + mem_size

    // Tell script to continue and wait for it to finish.
    fptr = fopen(pythonWait, "w");
    puts(ready);
    fclose(fptr);
    fptr = fopen(PKWait, "r");
    if(!fgets(output, strLen, fptr)) goto fail;
    fclose(fptr);

    // Map filled file to memory
    file_t *loader_t = file_open(loader, O_RDONLY, 0); // Open loaded pipe
    if (__do_mmap(minAddr, mem_size, prot | PROT_WRITE, flags2, loader_t, 0) != minAddr)
        goto fail; // returns minAddr on success, goto fail otherwise.
    file_decref(loader_t);

    // Tell script to continue and wait for it to finish.
    fptr = fopen(pythonWait, "w");
    puts(ready);
    fclose(fptr);
    fptr = fopen(PKWait, "r");
    if(!fgets(output, strLen, fptr)) goto fail;
    fclose(fptr);
  }

  // Clean up named pipes
  remove(loader);
  remove(PKWait);
  remove(pythonWait);

  //------------------------------------------------------------------------------------------------

  info->brk = ROUNDUP(info->brk_min, RISCV_PGSIZE);
  return;

fail:
  panic("couldn't open ELF program: %s!", fn);
}
