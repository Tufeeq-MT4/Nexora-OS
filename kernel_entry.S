    .code32
    .section .text.start,"ax"
    .global _start
_start:
    cli
    cld
    mov $0x9F000, %esp
    mov $__bss_start, %edi
    mov $__bss_end, %ecx
    sub %edi, %ecx
    xor %eax, %eax
    rep stosb
    call kmain
1:  hlt
    jmp 1b
    .section .note.GNU-stack,"",@progbits
