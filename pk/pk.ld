OUTPUT_ARCH( "riscv" )

ENTRY( _start )

SECTIONS
{

  /*--------------------------------------------------------------------*/
  /* Code and read-only segment                                         */
  /*--------------------------------------------------------------------*/

  /* Begining of code and text segment */
  . = 0x00002000;
  _ftext = .;
  PROVIDE( eprol = . );

  .text :
  {
    pk.o(.text)
  }

  /* text: Program code section */
  .text : 
  {
    *(.text)
    *(.text.*)
    *(.gnu.linkonce.t.*)
  }

  /* rodata: Read-only data */
  .rodata : 
  {
    *(.rdata)
    *(.rodata)
    *(.rodata.*)
    *(.gnu.linkonce.r.*)
  }

  /* End of code and read-only segment */
  PROVIDE( etext = . );
  _etext = .;

  /*--------------------------------------------------------------------*/
  /* Initialized data segment                                           */
  /*--------------------------------------------------------------------*/

  /* Start of initialized data segment */
  . = ALIGN(16);
   _fdata = .;

  /* data: Writable data */
  .data : 
  {
    *(.data)
    *(.data.*)
    *(.gnu.linkonce.d.*)
  }

  /* End of initialized data segment */
  . = ALIGN(4);
  PROVIDE( edata = . );
  _edata = .;

  /*--------------------------------------------------------------------*/
  /* Uninitialized data segment                                         */
  /*--------------------------------------------------------------------*/

  /* Start of uninitialized data segment */
  . = .;
  _fbss = .;

  /* sbss: Uninitialized writeable small data section */
  . = .;

  /* bss: Uninitialized writeable data section */
  . = .;
  _bss_start = .;
  .bss : 
  {
    *(.bss)
    *(.bss.*)
    *(.gnu.linkonce.b.*)
    *(COMMON)
  }

  /* End of uninitialized data segment (used by syscalls.c for heap) */
  PROVIDE( end = . );
  _end = .;

  /*--------------------------------------------------------------------*/
  /* Special gcc sections (this was in idt32.ld)                        */
  /*--------------------------------------------------------------------*/

  .mdebug.abi32        0 : { KEEP(*(.mdebug.abi32))        }
  .mdebug.abiN32       0 : { KEEP(*(.mdebug.abiN32))       }
  .mdebug.abi64        0 : { KEEP(*(.mdebug.abi64))        }
  .mdebug.abiO64       0 : { KEEP(*(.mdebug.abiO64))       }
  .mdebug.eabi32       0 : { KEEP(*(.mdebug.eabi32))       }
  .mdebug.eabi64       0 : { KEEP(*(.mdebug.eabi64))       }
  .gcc_compiled_long32 0 : { KEEP(*(.gcc_compiled_long32)) }
  .gcc_compiled_long64 0 : { KEEP(*(.gcc_compiled_long64)) }

}
