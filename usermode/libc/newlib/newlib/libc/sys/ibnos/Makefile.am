## Process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS = cygnus

INCLUDES = $(NEWLIB_CFLAGS) $(CROSS_CFLAGS) $(TARGET_CFLAGS)

AM_CCASFLAGS = $(INCLUDES)

noinst_LIBRARIES = lib.a libdummy.a

lib_a_SOURCES = chown.c close.c execve.c fork.c fstat.c \
	getpid.c gettod.c isatty.c kill.c link.c lseek.c open.c \
	read.c readlink.c malloc.c stat.c symlink.c times.c unlink.c \
	wait.c write.c liballoc.c reent.c _exit.c helper.c dup.c pipe.c \
	getdents.c
lib_a_CCASFLAGS = $(AM_CCASFLAGS)
lib_a_CFLAGS = $(AM_CFLAGS)

libdummy_a_SOURCES = crt0.S
libdummy_a_CCASFLAGS = $(AM_CCASFLAGS)
libdummy_a_CFLAGS = $(AM_CFLAGS)

all-local: crt0.o

ACLOCAL_AMFLAGS = -I ../../.. -I ../../../..
CONFIG_STATUS_DEPENDENCIES = $(newlib_basedir)/configure.host
