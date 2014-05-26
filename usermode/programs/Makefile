include ../config.mk

SOURCE		= $(wildcard *.c)
EXECUTABLES	= $(SOURCE:.c=.exe)
SUBDIRS		= $(dir $(wildcard */Makefile))

.PHONY: all
all: $(SUBDIRS) $(EXECUTABLES)

%.exe: %.c
	$(CC) $(CFLAGS) -MMD -MP -o $@ $< $(LDFLAGS)

-include $(SOURCE:.c=.d)

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@

.PHONY: clean
clean:
	rm -f *.a *.o *.d *.exe
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done
