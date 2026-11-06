    .section .rodata
    .balign 4
    .global logo_bin
logo_bin:   .incbin "logo.bin"
    .balign 4
    .global font0_bin
font0_bin:  .incbin "font0.bin"
    .balign 4
    .global font1_bin
font1_bin:  .incbin "font1.bin"
    .balign 4
    .global font2_bin
font2_bin:  .incbin "font2.bin"
    .section .note.GNU-stack,"",@progbits
