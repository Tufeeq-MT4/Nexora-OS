[bits 16]
[org 0x7C00]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; VGA Mode 13h (320x200) - Sab se reliable mode
    mov ax, 0x0013
    int 0x10

    ; Kernel load karo
    mov ah, 0x02
    mov al, 20
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov bx, 0x1000
    int 0x13
    jc disk_error

    ; 32-bit mode on karo
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp 0x08:init_pm

disk_error:
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
    jmp $

[bits 32]
init_pm:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    jmp 0x1000

gdt_start:
    dq 0x0
gdt_code:
    dw 0xFFFF, 0x0
    db 0x0, 0x9A, 0xCF, 0x0
gdt_data:
    dw 0xFFFF, 0x0
    db 0x0, 0x92, 0xCF, 0x0

gdt_descriptor:
    dw gdt_descriptor - gdt_start - 1
    dd gdt_start

times 510-($-$$) db 0
dw 0xAA55
