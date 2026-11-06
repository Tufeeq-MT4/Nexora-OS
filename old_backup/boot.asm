[org 0x7c00]
[bits 16]

%macro vbe_write 2
    mov dx, 0x01CE
    mov ax, %1
    out dx, ax
    mov dx, 0x01CF
    mov ax, %2
    out dx, ax
%endmacro

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti
    mov [boot_drive], dl

    ; 1. Kernel load karo (graphics se PEHLE, taake error text me dikhe)
    mov ah, 0x02
    mov al, 50
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    mov bx, 0x1000
    int 0x13
    jc disk_error

    ; 2. Framebuffer address padho (VBE mode info 0x9000 par, offset 40)
    mov ax, 0x4F01
    mov cx, 0x144
    mov di, 0x9000
    int 0x10

    ; 3. 1920x1080, 32-bit, linear framebuffer set karo
    vbe_write 4, 0x00        ; pehle disable
    vbe_write 1, 1920        ; width
    vbe_write 2, 1080        ; height
    vbe_write 3, 32          ; bits per pixel
    vbe_write 4, 0x41        ; enable + linear framebuffer

    ; 4. 32-bit mode on karo
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

boot_drive db 0

[bits 32]
init_pm:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    mov eax, [0x9000 + 40]   ; framebuffer address
    push eax
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
