TOP := $(dir $(lastword $(MAKEFILE_LIST)))

CC		:= i586-elf-gcc
CFLAGS	:= -m32 -std=gnu99 -Wall -Wextra -Wno-old-style-declaration -g -I../$(TOP)kernelmode/kernel/include -I $(TOP)system/i586-elf-ibnos/include -B $(TOP)system/i586-elf-ibnos/lib
LDFLAGS := -L $(TOP)system/i586-elf-ibnos/lib
