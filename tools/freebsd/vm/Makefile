SKELDIRS=boot etc etc/ssh etc/rc.d

CFGFILES=skel/boot/loader.conf \
    skel/etc/devfs.rules \
    skel/etc/fstab \
    skel/etc/host.conf \
    skel/etc/motd \
    skel/etc/rc.conf \
    skel/etc/ssh/xsshd_config \
    skel/etc/sysctl.conf \
    skel/etc/ttys

SCRIPTS=skel/etc/rc.d/xsshd

.PHONY: install-skel clean

all: template.qcow2

clean:
	rm -f template.img template.qcow2	

template.img: $(CFGFILES) $(SCRIPTS)
	./create-rootfs template.img

template.qcow2: template.img
	./create-image template.img

install-skel:
	@if [ -z "$(DESTDIR)" ]; then echo "Set DESTDIR";  exit 1; fi
	@for A in $(SKELDIRS); do \
		install -m 0755 -d "$(DESTDIR)/$$A"; \
	done
	@for A in $(CFGFILES); do \
		install -m 0644 "$$A" $(DESTDIR)/$${A#skel/}; \
	done
	@for A in $(SCRIPTS); do \
		install -m 0755 "$$A" $(DESTDIR)/$${A#skel/}; \
	done
