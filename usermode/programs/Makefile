include ../config.mk

SOURCE		= $(wildcard *.c)
OBJECTS		= $(SOURCE:.c=.o)
SUBDIRS		= $(dir $(wildcard */Makefile))

.PHONY: all
all: $(SUBDIRS) $(OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -o $@ -c $<

-include $(SOURCE:.c=.d)

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@

.PHONY: clean
clean:
	rm -f *.a *.o *.d
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done
