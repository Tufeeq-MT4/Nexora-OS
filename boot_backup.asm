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
    cld
    mov ah, 0x00
    mov al, 0x03
    int 0x10
    mov ah, 0x01
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
    cmp al, 32
    jb main_loop
    cmp di, buffer + 63
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
    cmp di, buffer
    je .skip
    call check_command
.skip:
    mov di, buffer
    mov cx, 64
.clear:
    mov byte [di], 0
    inc di
    loop .clear
    mov di, buffer
    mov si, prompt
    call print_string
    jmp main_loop
.backspace:
    cmp di, buffer
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
    mov si, cmd_echo
    mov di, buffer
    call compare_cmd
    je .do_echo
    mov si, cmd_reboot
    mov di, buffer
    call compare_cmd
    je .do_reboot
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
.do_echo:
    mov ah, 0x0E
    mov al, 13
    int 0x10
    mov al, 10
    int 0x10
    cmp byte [buffer + 4], 0
    je .echo_newline
    mov si, buffer
    add si, 5
    call print_string
.echo_newline:
    mov ah, 0x0E
    mov al, 13
    int 0x10
    mov al, 10
    int 0x10
    ret
.do_reboot:
   mov si, msg_reboot
   call print_string
   mov ah,0x00
   int 0x06
   jmp 0xFFFF :0x0000
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
    cmp al, 0
    ret
.equal:
    xor al, al
    ret
compare_cmd:
.loop:
    mov al, [si]
    cmp al, 0
    je .cmd_end
    cmp al, [di]
    jne .not_equal
    inc si
    inc di
    jmp .loop
.cmd_end:
    mov al, [di]
    cmp al, 0
    je .equal
    cmp al, ' '
    je .equal
    jmp .not_equal
.not_equal:
    mov al, 1
    cmp al, 0
    ret
.equal:
    xor al, al
    ret
prompt:       db "Nexora> ", 0
cmd_help:     db "help", 0
cmd_clear:    db "clear", 0
cmd_about:    db "about", 0
cmd_echo:     db "echo", 0
cmd_reboot:   db "reboot",0
msg_help:     db "Commands: help, clear, about, echo", 13, 10, 0
msg_about:    db "Nexora OS v0.1 - Made by Nexora CEO", 13, 10, 0
msg_reboot: db "System Rebooting......",13,10,0
msg_notfound: db "Command not found!", 13, 10, 0
buffer: times 64 db 0
times 510-($-$$) db 0
dw 0xAA55
