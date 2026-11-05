
all:
	nasm -f bin boot.asm -o boot.bin
	nasm -f elf32 kernel_entry.asm -o kernel_entry.o
	gcc -m32 -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-builtin -c kernel.c -o kernel.o
	ld -m elf_i386 -Ttext 0x1000 -e _start -o kernel.elf kernel_entry.o kernel.o
	objcopy -O binary kernel.elf kernel.bin
	dd if=/dev/zero of=os.img bs=512 count=2880
	dd if=boot.bin of=os.img conv=notrunc
	dd if=kernel.bin of=os.img bs=512 seek=1 conv=notrunc

run:
	qemu-system-i386 -fda os.img -vga std -display gtk,zoom-to-fit=on
