SKELDIRS=boot etc etc/ssh etc/rc.d usr/local/etc/nginx

CFGFILES=skel/boot/loader.conf \
    skel/etc/devfs.rules \
    skel/etc/fstab \
    skel/etc/host.conf \
    skel/etc/motd \
    skel/etc/newsyslog.conf \
    skel/etc/rc.conf \
    skel/etc/resolv.conf \
    skel/etc/resolvconf.conf \
    skel/etc/ssh/xsshd_config \
    skel/etc/sysctl.conf \
    skel/etc/syslog.conf \
    skel/etc/ttys \
    skel/etc/unbound.conf.template \
    skel/usr/local/etc/nginx/mime.types \
    skel/usr/local/etc/nginx/nginx.conf \
    skel/usr/local/etc/nginx/scgi_params


SCRIPTS=skel/etc/rc.d/unbound_cfg \
    skel/etc/rc.d/xsshd

.PHONY: install-skel clean

all: yans.qcow2

clean:
	rm -f template.img template.qcow2 yans.img yans.qcow2

template.img:
	./create-rootfs template.img

template.qcow2: template.img
	./create-image template.img

yans.qcow2: template.img $(CFGFILES) $(SCRIPTS)
	./install-yans template.img yans.img
	./create-image yans.img

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