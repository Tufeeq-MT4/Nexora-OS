ENTRY(_start)
SECTIONS {
    . = 0x10000;
    .text   : { *(.text.start) *(.text .text.*) }
    .rodata : ALIGN(4) { *(.rodata .rodata.*) }
    .data   : ALIGN(4) { *(.data .data.*) }
    . = ALIGN(4);
    __bss_start = .;
    .bss    : { *(.bss .bss.*) *(COMMON) }
    . = ALIGN(4);
    __bss_end = .;
    /DISCARD/ : { *(.note*) *(.comment) *(.eh_frame*) }
}
