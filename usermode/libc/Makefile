# Nasty hack to get around the problem that newlib wants the OS name in the compiler name
CC_FOR_TARGET	= i586-elf-gcc
CXX_FOR_TARGET	= i586-elf-c++
AR_FOR_TARGET	= i586-elf-ar
AS_FOR_TARGET	= i586-elf-as
LD_FOR_TARGET	= i586-elf-ld
NM_FOR_TARGET	= i586-elf-nm
OBJDUMP_FOR_TARGET	= i586-elf-objdump
RANLIB_FOR_TARGET	= i586-elf-ranlib
READELF_FOR_TARGET	= i586-elf-readelf
STRIP_FOR_TARGET	= i586-elf-strip
CFLAGS= -g
export

install_prefix = $(shell readlink -f ../system)

.PHONY: all
all: ../system/i586-elf-ibnos/lib/libc.a

newlib/Makefile: newlib/configure
	cd newlib; ./configure --target=i586-elf-ibnos --prefix=$(install_prefix)

newlib/i586-elf-ibnos/newlib/libc.a: newlib/Makefile
	make -C newlib

../system/i586-elf-ibnos/lib/libc.a: newlib/i586-elf-ibnos/newlib/libc.a
	make -C newlib install

.PHONY: clean
clean:
	if [ -f newlib/Makefile ]; then \
		rm -rf newlib/i586-elf-ibnos/; \
		make -C newlib distclean; \
	fi
