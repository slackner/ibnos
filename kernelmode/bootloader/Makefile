include ../config.mk

.PHONY: all
all: bootloader.o

bootloader.o: bootloader.s
	$(AS) --32 bootloader.s -o bootloader.o

.PHONY: clean
clean:
	rm -f *.o *.a
