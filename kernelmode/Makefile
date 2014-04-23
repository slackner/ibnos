include config.mk

OBJECTS = $(shell find ./ -iname *.o)
SUBDIRS = $(dir $(wildcard */Makefile))

.PHONY: all
all: ibnos.kernel

ibnos.kernel: $(SUBDIRS) $(OBJECTS) linker.ld
	$(CC) -T linker.ld $(CFLAGS) -o ibnos.kernel $(OBJECTS) $(LDFLAGS)

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@

.PHONY: doc
doc:
	doxygen doxygen.conf

.PHONY: clean
clean:
	rm -f *.kernel
	rm -rf doc/*
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done
