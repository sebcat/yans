SKELDIRS=boot etc etc/ssh etc/rc.d usr/local/etc/nginx

CFGFILES=skel/boot/loader.conf \
    skel/etc/tcpctl.blacklist \
    skel/etc/crontab \
    skel/etc/devd.conf \
    skel/etc/devfs.rules \
    skel/etc/fstab \
    skel/etc/host.conf \
    skel/etc/motd \
    skel/etc/newsyslog.conf \
    skel/etc/nsswitch.conf \
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
    skel/usr/local/etc/nginx/scgi_params \
    skel/usr/local/etc/nginx/disco.sajber.se.conf \
    skel/usr/local/etc/nginx/yans-locations



SCRIPTS=skel/etc/rc.d/xsshd \
    skel/etc/rc.d/xvar

.PHONY: install-skel clean

all: yans.raw

clean:
	rm -f template.rootfs template.raw yans.rootfs fresh.rootfs \
		 yans.raw

template.rootfs:
	./create-rootfs template.rootfs

template.raw: template.rootfs
	./create-image template.rootfs

yans.rootfs: template.rootfs $(CFGFILES) $(SCRIPTS)
	./install-yans template.rootfs yans.rootfs

yans.raw: yans.rootfs
	./create-image yans.rootfs yans.raw

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
