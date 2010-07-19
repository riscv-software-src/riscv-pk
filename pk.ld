OUTPUT_ARCH( "mips:riscv" )

ENTRY( __start )

GROUP( -lc -lgloss -lgcc )

STARTUP( boot.o )


SECTIONS
{

  /*--------------------------------------------------------------------*/
  /* Code and read-only segment                                         */
  /*--------------------------------------------------------------------*/

  /* Begining of code and text segment */
  . = 0x00000000;
  .boottext :
  {
    boot.o(.text)
  }

  . = 0x70000000;
  _ftext = .;
  PROVIDE( eprol = . );

  /* text: Program code section */
  .text : 
  {
    *(.text)
    *(.text.*)
    *(.gnu.linkonce.t.*)
  }

  /* init: Code to execute before main (called by crt1.S) */
  .init : 
  {
    KEEP( *(.init) )
  }

  /* fini: Code to execute after main (called by crt1.S) */
  .fini : 
  {
    KEEP( *(.fini) )
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
  /* Global constructor/destructor segement                             */
  /*--------------------------------------------------------------------*/
  /* The .ctors/.dtors sections are special sections which contain a
     list of constructor/destructor function pointers. crtbegin.o
     includes code in a .init section which goes through the .ctors list
     and calls each constuctor. crtend.o includes code in a .fini
     section which goes through the .dtors list and calls each
     destructor. crtbegin.o includes a special null pointer in its own
     .ctors/.dtors sections which acts as a start indicator for those
     lists. crtend.o also includes a special null pointer in its own
     .ctors/.dtors sections which acts as an end indictor. The linker
     commands below are setup so that crtbegin.o's .ctors/.dtors
     sections are always first and crtend.o's .ctors/.dtors sections are
     always last. This is the only way the list of functions will have
     the begin and end indicators in the right place. */

  /* ctors : Array of global constructor function pointers */
  .ctors :
  {
    KEEP( *crtbegin.o(.ctors) )
    KEEP( *(EXCLUDE_FILE(*crtend.o) .ctors) )
    KEEP( *(SORT(.ctors.*)) )
    KEEP( *(.ctors) )
  }

  /* dtors : Array of global destructor function pointers */
  .dtors :
  {
    KEEP( *crtbegin.o(.dtors) )
    KEEP( *(EXCLUDE_FILE(*crtend.o) .dtors) )
    KEEP( *(SORT(.dtors.*)) )
    KEEP( *(.dtors) )
  }

  /*--------------------------------------------------------------------*/
  /* Other misc gcc segments (this was in idt32.ld)                     */
  /*--------------------------------------------------------------------*/
  /* I am not quite sure about these sections but it seems they are for
     C++ exception handling. I think .jcr is for "Java Class
     Registration" but it seems to end up in C++ binaries as well. */

  .eh_frame_hdr     : { *(.eh_frame_hdr)     }
  .eh_frame         : { KEEP( *(.eh_frame) ) }
  .gcc_except_table : { *(.gcc_except_table) }
  .jcr              : { KEEP (*(.jcr))       } 

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

  /* gp: Address for initializing gp register (used by crt1.S) */
  . = ALIGN(8);
  _gp = . + 0x8000;

  /* sdata: Writable small data */
  .sdata : 
  {
    *(.sdata)
    *(.sdata.*)
    *(.gnu.linkonce.s.*)
  }

  /* lit4: Single precision floating point constants created by gas */
  .lit4 : 
  {
    *(.lit4)
  }

  /* lit8: Double precision floating point constants created by gas */
  .lit8 : 
  {
    *(.lit8)
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
  _sbss_start = .;
  .sbss : 
  {
    *(.sbss)
    *(.sbss.*)
    *(.gnu.linkonce.sb.*)
    *(.scommon)
  }

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
