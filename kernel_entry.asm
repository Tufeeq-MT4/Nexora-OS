[bits 32]
[extern kmain]      ; C wale function ka naam

section .text
global _start

_start:
    call kmain      ; C ka code chalao
    jmp $           ; Jab C ka code khatam ho, yahin ruk jao
    section .note.GNU-stack noalloc noexec nowrite progbits
