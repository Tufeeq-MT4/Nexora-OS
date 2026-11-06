[bits 32]
[extern kmain]
section .text
global _start
_start:
    pop eax         ; Bootloader se Framebuffer address uthao
    push eax        ; C function ko pass karo
    call kmain
    jmp $
section .note.GNU-stack noalloc noexec nowrite progbits
