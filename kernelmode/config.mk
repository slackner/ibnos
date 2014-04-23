TOP := $(dir $(lastword $(MAKEFILE_LIST)))

AR 		:= i586-elf-ar
AS 		:= i586-elf-as
CC 		:= i586-elf-gcc
CFLAGS	:= -I$(TOP)kernel/include -D__KERNEL__ -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra -g
LDFLAGS	:= -nostdlib -lgcc