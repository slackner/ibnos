include config.mk

SOURCE		= $(wildcard *.c)
DIR_OBJECTS = $(SOURCE:.c=.o)
OBJECTS		= $(shell find . -mindepth 2 -iname "*.o" -not -path "./libc/*" -not -path "./system/*")
SUBDIRS		= $(dir $(wildcard */Makefile))

.PHONY: all
all: init.user

init.user: $(SUBDIRS) $(OBJECTS) $(DIR_OBJECTS)
	echo $(OBJECTS)
	$(CC) $(CFLAGS) -o init.user $(OBJECTS) $(DIR_OBJECTS) $(LDFLAGS)

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
