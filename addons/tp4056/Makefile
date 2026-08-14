TOOLCHAIN = $(HOME)/Library/Arduino15/packages/esp32/tools/esp-x32/2601/bin

CC = $(TOOLCHAIN)/xtensa-esp32-elf-gcc
OBJCOPY = $(TOOLCHAIN)/xtensa-esp32-elf-objcopy
LDSCRIPT = ../addon.ld
CFLAGS = -Os -fno-pic -ffunction-sections

SRC = tp4056.c

all: tp4056-esp32.bin tp4056-esp32s2.bin

tp4056-esp32.bin: $(SRC) $(LDSCRIPT)
	$(CC) $(CFLAGS) -c $< -o tp4056-esp32.o
	$(CC) -nostdlib -T $(LDSCRIPT) tp4056-esp32.o -o tp4056-esp32.elf
	$(OBJCOPY) -O binary tp4056-esp32.elf $@

tp4056-esp32s2.bin: $(SRC) $(LDSCRIPT)
	$(CC) $(CFLAGS) -DADDON_ESP32S2 -c $< -o tp4056-esp32s2.o
	$(CC) -nostdlib -T $(LDSCRIPT) tp4056-esp32s2.o -o tp4056-esp32s2.elf
	$(OBJCOPY) -O binary tp4056-esp32s2.elf $@

clean:
	rm -f tp4056-esp32.o tp4056-esp32.elf tp4056-esp32.bin tp4056-esp32s2.o tp4056-esp32s2.elf tp4056-esp32s2.bin
