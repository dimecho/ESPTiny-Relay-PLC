TOOLCHAIN = $(HOME)/Library/Arduino15/packages/esp32/tools/esp-x32/2601/bin

CC = $(TOOLCHAIN)/xtensa-esp32-elf-gcc
OBJCOPY = $(TOOLCHAIN)/xtensa-esp32-elf-objcopy
LDSCRIPT = ../addon.ld
CFLAGS = -Os -fno-pic -ffunction-sections

SRC = test.c

all: test-esp32.bin test-esp32s2.bin

test-esp32.bin: $(SRC) $(LDSCRIPT)
	$(CC) $(CFLAGS) -c $< -o test-esp32.o
	$(CC) -nostdlib -T $(LDSCRIPT) test-esp32.o -o test-esp32.elf
	$(OBJCOPY) -O binary test-esp32.elf $@

test-esp32s2.bin: $(SRC) $(LDSCRIPT)
	$(CC) $(CFLAGS) -DADDON_ESP32S2 -c $< -o test-esp32s2.o
	$(CC) -nostdlib -T $(LDSCRIPT) test-esp32s2.o -o test-esp32s2.elf
	$(OBJCOPY) -O binary test-esp32s2.elf $@

clean:
	rm -f test-esp32.o test-esp32.elf test-esp32.bin test-esp32s2.o test-esp32s2.elf test-esp32s2.bin
