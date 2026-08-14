TOOLCHAIN = $(HOME)/Library/Arduino15/packages/esp32/tools/esp-x32/2601/bin

CC = $(TOOLCHAIN)/xtensa-esp32-elf-gcc
OBJCOPY = $(TOOLCHAIN)/xtensa-esp32-elf-objcopy
LDSCRIPT = ../addon.ld
CFLAGS = -Os -fno-pic -ffunction-sections

SRC = file.c

all: file-esp32.bin file-esp32s2.bin

file-esp32.bin: $(SRC) $(LDSCRIPT)
	$(CC) $(CFLAGS) -c $< -o file-esp32.o
	$(CC) -nostdlib -T $(LDSCRIPT) file-esp32.o -o file-esp32.elf
	$(OBJCOPY) -O binary file-esp32.elf $@

file-esp32s2.bin: $(SRC) $(LDSCRIPT)
	$(CC) $(CFLAGS) -DADDON_ESP32S2 -c $< -o file-esp32s2.o
	$(CC) -nostdlib -T $(LDSCRIPT) file-esp32s2.o -o file-esp32s2.elf
	$(OBJCOPY) -O binary file-esp32s2.elf $@

clean:
	rm -f file-esp32.o file-esp32.elf file-esp32.bin file-esp32s2.o file-esp32s2.elf file-esp32s2.bin
