# =====================================================================
#  Nexora OS  -  boot.S   (BIOS bootloader, 16-bit -> 32-bit)
#  Sector 0      : stage 1 (reads stage 2)         loaded at 0x7C00
#  Sectors 1..3  : stage 2 (VESA + protected mode) loaded at 0x7E00
#  Sectors 4..   : kernel                          loaded at 0x10000
#  Boot info for the kernel is written at 0x5800.
# =====================================================================
    .intel_syntax noprefix
    .code16
    .text
    .global _start

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    cld
    mov [boot_drive], dl

    mov ah, 0x41                    # BIOS extended disk access available?
    mov bx, 0x55AA
    int 0x13
    jc disk_err
    cmp bx, 0xAA55
    jne disk_err

    mov word ptr [dap+2], 3         # read 3 sectors (stage 2) from LBA 1
    mov word ptr [dap+4], 0x7E00
    mov word ptr [dap+6], 0
    mov dword ptr [dap+8], 1
    mov dword ptr [dap+12], 0
    mov si, offset dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc disk_err
    mov dl, [boot_drive]
    jmp stage2

disk_err:
    mov si, offset msg_disk
    call print
hang:
    cli
    hlt
    jmp hang

print:
    lodsb
    test al, al
    jz 1f
    mov ah, 0x0E
    mov bx, 7
    int 0x10
    jmp print
1:  ret

boot_drive: .byte 0
dap:        .byte 0x10, 0
            .word 0, 0, 0
            .long 0, 0
msg_disk:   .asciz "Nexora: disk read error"

# ---- MBR partition table (one active entry, so strict BIOSes accept the USB) ----
    .org 446
    .byte 0x80, 0x00, 0x02, 0x00    # active, CHS start
    .byte 0xDA, 0xFE, 0xFF, 0xFF    # type, CHS end
    .long 1                         # first LBA
    .long 2879                      # sector count
    .fill 48, 1, 0
    .org 510
    .word 0xAA55

# =====================================================================
#  STAGE 2  (runs at 0x7E00)
# =====================================================================
    .org 512
stage2:
    mov [boot_drive], dl

    # ---------- load the kernel to 0x10000 (small chunks, retries, checked) ----------
    mov dword ptr [cur_lba], 4
    mov word ptr [cur_seg], 0x1000
    mov word ptr [remaining], offset KSECTORS
load_loop:
    mov ax, [remaining]
    test ax, ax
    jz load_done
    cmp ax, 1
    jbe 1f
    mov ax, 1
1:  mov [chunk], ax
    mov byte ptr [retries], 3
read_again:
    mov ax, [chunk]
    mov [dap+2], ax
    mov word ptr [dap+4], 0
    mov bx, [cur_seg]
    mov [dap+6], bx
    mov ecx, [cur_lba]
    mov [dap+8], ecx
    mov dword ptr [dap+12], 0
    mov si, offset dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jnc read_ok
    xor ax, ax                      # reset the disk and try again
    mov dl, [boot_drive]
    int 0x13
    dec byte ptr [retries]
    jnz read_again
    jmp read_fail
read_ok:
    mov ax, [dap+2]                 # sectors really transferred (some BIOSes update it)
    test ax, ax
    jz use_chunk
    cmp ax, [chunk]
    jbe have_count
use_chunk:
    mov ax, [chunk]
have_count:
    sub [remaining], ax
    movzx ecx, ax
    add [cur_lba], ecx
    shl ax, 5                       # sectors * 512 / 16 = paragraphs
    add [cur_seg], ax
    jmp load_loop
load_done:

    # ---------- check the kernel was loaded completely (magic dword at its end) ----------
    mov ax, offset ((0x10000 + KBYTES - 4) >> 4)
    mov fs, ax
    mov si, offset ((0x10000 + KBYTES - 4) & 15)
    mov eax, fs:[si]
    cmp eax, 0x4E455841
    jne kernel_bad

    # ---------- enable A20 ----------
    mov ax, 0x2401
    int 0x15
    in al, 0x92
    or al, 2
    and al, 0xFE
    out 0x92, al

    # ---------- VESA: read controller info ----------
    xor ax, ax
    mov es, ax
    mov dword ptr [0x5000], 0x32454256      # 'VBE2'
    mov di, 0x5000
    mov ax, 0x4F00
    int 0x10
    cmp ax, 0x004F
    jne vbe_fail
    cmp word ptr [0x5004], 0x0200
    jb vbe_fail
    mov ax, [0x500E]
    mov [list_off], ax
    mov ax, [0x5010]
    mov [list_seg], ax
    mov byte ptr [best_score], 0

    # ---------- scan every mode, keep the best one ----------
scan_loop:
    mov si, [list_off]
    mov ax, [list_seg]
    mov fs, ax
    mov cx, fs:[si]
    cmp cx, 0xFFFF
    je scan_done
    add word ptr [list_off], 2
    mov [cur_mode], cx
    xor ax, ax
    mov es, ax
    mov di, 0x5200
    mov ax, 0x4F01
    int 0x10
    cmp ax, 0x004F
    jne scan_loop
    mov ax, [0x5200]                # attributes: supported + graphics + linear fb
    and ax, 0x0091
    cmp ax, 0x0091
    jne scan_loop
    cmp byte ptr [0x521B], 6        # direct colour
    jne scan_loop
    mov al, [0x5219]                # bits per pixel
    mov bl, 0
    cmp al, 32
    jne 1f
    mov bl, 1
    jmp 2f
1:  cmp al, 24
    jne scan_loop
2:  mov dx, [0x5212]                # width
    mov ax, [0x5214]                # height
    cmp dx, 800
    jb scan_loop
    mov si, offset rank_table
3:  mov cx, [si]
    test cx, cx
    jz 5f
    cmp cx, dx
    jne 4f
    mov cx, [si+2]
    cmp cx, ax
    jne 4f
    mov cl, [si+4]
    jmp 6f
4:  add si, 5
    jmp 3b
5:  cmp dx, 1920                    # unknown size: accept only up to 1920x1200
    ja scan_loop
    cmp ax, 1200
    ja scan_loop
    mov cl, 1
6:  shl cl, 1
    add cl, bl
    cmp cl, [best_score]
    jbe scan_loop
    mov [best_score], cl
    mov cx, [cur_mode]
    mov [best_mode], cx
    mov si, 0x5200                  # keep a copy of this mode's info
    mov di, 0x5400
    mov cx, 128
    rep movsw
    jmp scan_loop

scan_done:
    cmp byte ptr [best_score], 0
    je vbe_fail

    # ---------- QEMU / Bochs / VirtualBox: set 1920x1080x32 directly ----------
    mov dx, 0x01CE
    xor ax, ax
    out dx, ax
    mov dx, 0x01CF
    in ax, dx
    and ax, 0xFFF0
    cmp ax, 0xB0C0
    jne set_vbe
    .macro bga idx, val
    mov dx, 0x01CE
    mov ax, \idx
    out dx, ax
    mov dx, 0x01CF
    mov ax, \val
    out dx, ax
    .endm
    bga 4, 0
    bga 1, 1920
    bga 2, 1080
    bga 3, 32
    bga 4, 0x41
    mov word ptr [0x5410], 7680
    mov word ptr [0x5412], 1920
    mov word ptr [0x5414], 1080
    mov byte ptr [0x5419], 32
    mov byte ptr [0x5420], 16
    mov word ptr [0x5432], 0
    jmp mode_done

set_vbe:
    mov ax, 0x4F02
    mov bx, [best_mode]
    or bx, 0x4000                   # use linear framebuffer
    int 0x10
    cmp ax, 0x004F
    jne vbe_fail
mode_done:

    # ---------- boot info for the kernel (at 0x5800) ----------
    mov eax, [0x5428]               # framebuffer address
    mov [0x5800], eax
    movzx eax, word ptr [0x5410]    # bytes per line
    cmp word ptr [0x5004], 0x0300
    jb 1f
    cmp word ptr [0x5432], 0
    je 1f
    movzx eax, word ptr [0x5432]
1:  mov [0x5804], eax
    movzx eax, word ptr [0x5412]
    mov [0x5808], eax               # width
    movzx eax, word ptr [0x5414]
    mov [0x580C], eax               # height
    movzx eax, byte ptr [0x5419]
    mov [0x5810], eax               # bits per pixel
    movzx eax, byte ptr [0x5420]
    mov [0x5814], eax               # red field position

    # ---------- enter protected mode ----------
    cli
    lgdt [gdt_desc]
    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp 0x08:pm_entry

vbe_fail:
    mov si, offset msg_vbe
    call print
    jmp hang

read_fail:
    mov si, offset msg_disk
    call print
    mov si, offset msg_lba
    call print
    mov ax, [cur_lba]
    call print_hex16
    jmp hang

kernel_bad:
    mov si, offset msg_bad
    call print
    mov si, offset msg_lba
    call print
    mov ax, [cur_lba]
    call print_hex16
    mov si, offset msg_left
    call print
    mov ax, [remaining]
    call print_hex16
    jmp hang

print_hex16:                        # print AX as 4 hex digits
    mov cx, 4
1:  rol ax, 4
    push ax
    and al, 0x0F
    add al, 0x30
    cmp al, 0x39
    jbe 2f
    add al, 7
2:  mov ah, 0x0E
    mov bx, 7
    int 0x10
    pop ax
    loop 1b
    ret

    .code32
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x9F000
    mov eax, 0x10000
    jmp eax

    .code16
    .balign 8
gdt_start:
    .quad 0
    .quad 0x00CF9A000000FFFF        # code
    .quad 0x00CF92000000FFFF        # data
gdt_desc:
    .word gdt_desc - gdt_start - 1
    .long gdt_start

cur_lba:    .long 0
chunk:      .word 0
retries:    .byte 0
cur_seg:    .word 0
remaining:  .word 0
list_off:   .word 0
list_seg:   .word 0
cur_mode:   .word 0
best_mode:  .word 0
best_score: .byte 0
msg_bad:    .asciz "Nexora: kernel not fully loaded from USB (read error)"
msg_lba:    .asciz " lba=0x"
msg_left:   .asciz " left=0x"
msg_vbe:    .asciz "Nexora: VESA graphics mode not supported on this PC"

#   width, height, rank   (higher rank = preferred)
rank_table:
    .word 1920, 1080
    .byte 9
    .word 1600, 900
    .byte 8
    .word 1366, 768
    .byte 7
    .word 1280, 720
    .byte 6
    .word 1280, 800
    .byte 5
    .word 1280, 1024
    .byte 5
    .word 1024, 768
    .byte 4
    .word 1920, 1200
    .byte 3
    .word 0

    .org 2048                       # boot.bin = exactly 4 sectors
