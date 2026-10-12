[org 0x7c00]
[bits 16]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti
    cld                         ; FIX 5

    mov ah, 0x00                ; clear screen
    mov al, 0x03
    int 0x10

    mov ah, 0x01                ; cursor shape
    mov ch, 0x06
    mov cl, 0x07
    int 0x10

    mov si, prompt
    call print_string
    mov di, buffer

main_loop:
    mov ah, 0x00
    int 0x16

    cmp al, 13
    je .enter
    cmp al, 8
    je .backspace

    cmp al, 32                  ; FIX 3: ignore special/control keys
    jb main_loop
    cmp di, buffer + 63         ; FIX 2: buffer full, ignore
    jae main_loop

    mov ah, 0x0E
    mov bl, 0x0F
    int 0x10
    mov [di], al
    inc di
    jmp main_loop

.enter:
    mov byte [di], 0

    mov ah, 0x0E
    mov al, 13
    int 0x10
    mov al, 10
    int 0x10

    cmp di, buffer              ; FIX 4: empty line, skip command check
    je .skip
    call check_command
.skip:
    mov si, prompt
    call print_string
    mov di, buffer
    jmp main_loop

.backspace:
    cmp di, buffer              ; FIX 1: check FIRST
    je main_loop
    dec di
    mov ah, 0x0E
    mov al, 8
    int 0x10
    mov al, ' '
    int 0x10
    mov al, 8
    int 0x10
    jmp main_loop

print_string:
    mov ah, 0x0E
    mov bl, 0x0F
.loop:
    lodsb
    cmp al, 0
    je .done
    int 0x10
    jmp .loop
.done:
    ret

check_command:
    mov si, cmd_help
    mov di, buffer
    call compare
    je .do_help

    mov si, cmd_clear
    mov di, buffer
    call compare
    je .do_clear

    mov si, cmd_about
    mov di, buffer
    call compare
    je .do_about

    mov si, msg_notfound
    call print_string
    ret

.do_help:
    mov si, msg_help
    call print_string
    ret

.do_clear:
    mov ah, 0x00
    mov al, 0x03
    int 0x10
    ret

.do_about:
    mov si, msg_about
    call print_string
    ret

compare:
.loop:
    mov al, [si]
    mov bl, [di]
    cmp al, bl
    jne .not_equal
    cmp al, 0
    je .equal
    inc si
    inc di
    jmp .loop
.not_equal:
    mov al, 1
    cmp al, 0                   ; ZF = 0
    ret
.equal:
    xor al, al                  ; ZF = 1
    ret

prompt:       db "Nexora> ", 0
cmd_help:     db "help", 0
cmd_clear:    db "clear", 0
cmd_about:    db "about", 0

msg_help:     db "Commands: help, clear, about", 13, 10, 0
msg_about:    db "Nexora OS v0.1 - Made by Nexora CEO", 13, 10, 0
msg_notfound: db "Command not found!", 13, 10, 0

buffer: times 64 db 0

times 510-($-$$) db 0
dw 0xAA55
