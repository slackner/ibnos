SHELL = /bin/bash
SUBDIRS = kernelmode usermode

.PHONY: all
all: ibnos.iso

ibnos.iso: $(SUBDIRS) tmpclean
	cp kernelmode/ibnos.kernel iso/boot/ibnos.kernel
	cp usermode/init.user iso/boot/init.user
	cp usermode/rootfs.tar iso/boot/rootfs.tar
	mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot \
         -boot-load-size 4 -boot-info-table -o ibnos.iso iso

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@

.PHONY: tmpclean
tmpclean:
	rm -f ibnos.iso

.PHONY: doc
doc:
	$(MAKE) -C kernelmode $@

.PHONY: clean
clean: tmpclean
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done
