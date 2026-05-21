# Makefile - JJ4x4 USB-MIDI controller firmware
#
# Build : make            -> produces main.hex and prints the size
# Flash : make flash      -> bootloadHID -r main.hex
#         (hold key K11 while plugging in USB to enter the bootloader first)
# Clean : make clean
#
# Requires the AVR toolchain (avr-gcc, avr-objcopy, avr-size) and the
# bootloadHID command-line flasher on PATH. See README.md.

DEVICE  = atmega32
F_CPU   = 12000000

CC      = avr-gcc
OBJCOPY = avr-objcopy
SIZE    = avr-size

CFLAGS  = -Wall -Os -std=gnu99 -DF_CPU=$(F_CPU) -I. -Iusbdrv -mmcu=$(DEVICE)

OBJECTS = usbdrv/usbdrv.o usbdrv/usbdrvasm.o usbdrv/oddebug.o \
          main.o midi.o matrix.o modes.o leds.o

HEADERS = usbconfig.h board.h midi.h matrix.h modes.h leds.h

all: main.hex

main.elf: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o main.elf

main.hex: main.elf
	$(OBJCOPY) -j .text -j .data -O ihex main.elf main.hex
	$(SIZE) --format=berkeley main.elf

flash: main.hex
	bootloadHID -r main.hex

size: main.elf
	$(SIZE) --format=berkeley main.elf

clean:
	rm -f *.o *.elf *.hex usbdrv/*.o

# Application objects depend on all firmware headers (rebuild on any change).
main.o midi.o matrix.o modes.o leds.o: $(HEADERS)
# V-USB objects depend on usbconfig.h.
$(OBJECTS): usbconfig.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

usbdrv/%.o: usbdrv/%.c
	$(CC) $(CFLAGS) -c $< -o $@

usbdrv/usbdrvasm.o: usbdrv/usbdrvasm.S
	$(CC) $(CFLAGS) -x assembler-with-cpp -c usbdrv/usbdrvasm.S -o usbdrv/usbdrvasm.o
