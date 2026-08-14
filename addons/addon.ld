OUTPUT_FORMAT(elf32-xtensa-le)
ENTRY(main)

SECTIONS {
  . = 0x40080000;
  _blob_start = .;

  .func_table : {
    LONG(1)
    LONG(main - _blob_start)
  }

  .literal : {
    *(.literal .literal.*)
  }

  .text : {
    *(.text .text.* .text.startup .text.startup.*)
  }

  /DISCARD/ : {
    *(.comment)
    *(.xt.lit)
    *(.xt.prop)
    *(.debug*)
    *(.data)
    *(.data.*)
    *(.bss)
    *(.rodata)
    *(.rodata.*)
  }
}
