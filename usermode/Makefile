include config.mk

SOURCE		= $(wildcard *.c)
OBJECTS 	= $(SOURCE:.c=.o)
EXECUTABLES = $(shell find . -mindepth 2 -iname "*.exe" -not -path "./libc/*" -not -path "./system/*")
SUBDIRS		= $(dir $(wildcard */Makefile))

.PHONY: all
all: init.user rootfs.tar

init.user: $(SUBDIRS) $(OBJECTS)
	echo $(OBJECTS)
	$(CC) $(CFLAGS) -o init.user $(OBJECTS) $(LDFLAGS)

rootfs.tar: $(SUBDIRS) $(EXECUTABLES)
	tar --transform 's,^\./[^/]*/,/bin/,' -c -f rootfs.tar $(EXECUTABLES)

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -o $@ -c $<

-include $(SOURCE:.c=.d)

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@

.PHONY: clean
clean:
	rm -f *.kernel
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done
