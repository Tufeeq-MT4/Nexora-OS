CFLAGS = -m32 -O2 -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-builtin \
         -mno-sse -mno-mmx -mno-80387 -fno-asynchronous-unwind-tables -Wall -Wno-misleading-indentation

all: os.img

kernel_entry.o: kernel_entry.S
	gcc -m32 -c kernel_entry.S -o kernel_entry.o

kernel.o: kernel.c
	gcc $(CFLAGS) -c kernel.c -o kernel.o

assets.o: assets.S logo.bin font0.bin font1.bin font2.bin
	gcc -m32 -c assets.S -o assets.o

kernel.elf: kernel_entry.o kernel.o assets.o kernel.ld
	ld -m elf_i386 -T kernel.ld -o kernel.elf kernel_entry.o kernel.o assets.o

kernel.bin: kernel.elf
	objcopy -O binary kernel.elf kernel.bin

boot.bin: boot.S kernel.bin
	SZ=$$(stat -c%s kernel.bin); KS=$$(( (SZ + 511) / 512 )); \
	echo "kernel bytes: $$SZ  sectors: $$KS"; \
	as --32 --defsym KSECTORS=$$KS --defsym KBYTES=$$SZ boot.S -o boot.o && \
	ld -m elf_i386 -Ttext 0x7C00 -e _start -o boot.elf boot.o && \
	objcopy -O binary boot.elf boot.bin

os.img: boot.bin kernel.bin
	@if [ -f os.img ]; then dd if=os.img of=.settings.sec bs=512 skip=2800 count=1 status=none 2>/dev/null || true; fi
	dd if=/dev/zero of=os.img bs=512 count=2880 status=none
	dd if=boot.bin of=os.img conv=notrunc status=none
	dd if=kernel.bin of=os.img bs=512 seek=4 conv=notrunc status=none
	@if [ -f walls.bin ]; then \
	  dd if=walls.bin of=os.img bs=512 seek=4096 conv=notrunc status=none; \
	  echo "OK: wallpapers added from walls.bin"; \
	else echo "NOTE: walls.bin not found - photo wallpapers skipped"; fi
	@if [ -s .settings.sec ]; then dd if=.settings.sec of=os.img bs=512 seek=2800 conv=notrunc status=none; echo "OK: your saved settings were kept"; fi
	@echo "OK: os.img ready"

run: os.img
	# accel=kvm:tcg = use the CPU hardware (fast, smooth) when /dev/kvm exists, otherwise fall back to slow software mode
	qemu-system-i386 -machine pc,accel=kvm:tcg -m 128 -drive file=os.img,format=raw -vga std -rtc base=localtime -display gtk,zoom-to-fit=on

clean:
	@if [ -f os.img ]; then dd if=os.img of=.settings.sec bs=512 skip=2800 count=1 status=none 2>/dev/null || true; fi
	rm -f *.o *.elf boot.bin kernel.bin os.img

# erase the saved settings too (password goes back to 11223344)
reset-settings:
	rm -f .settings.sec os.img
	@echo "Settings erased. Run: make run"

.PHONY: all run clean reset-settings
