[bits 32]
[extern kmain]
section .text
global _start
_start:
    call kmain
    jmp $
section .note.GNU-stack noalloc noexec nowrite progbits
