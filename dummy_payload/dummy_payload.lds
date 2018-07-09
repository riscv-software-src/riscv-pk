/* See LICENSE for license details. */

ENTRY(_start)

SECTIONS {
  . = -0x80000000;

  .text.init : { *(.text.init) }
}
