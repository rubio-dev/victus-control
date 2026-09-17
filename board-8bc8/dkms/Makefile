# Builds the stock hp-wmi driver of the running kernel series with board 8BC8
# added. The source is the kernel's own, so no cross-version compatibility
# shims are needed here: it is built against the headers it was written for.

obj-m += hp-wmi.o

TARGET_KERNEL_RELEASE := $(if $(KERNELRELEASE),$(KERNELRELEASE),$(shell uname -r))
KDIR := /lib/modules/$(TARGET_KERNEL_RELEASE)/build
PWD := $(shell pwd)

PKGNAME := $(shell grep -oP 'PACKAGE_NAME="\K[^"]+' dkms.conf)
VERSION := $(shell grep -oP 'PACKAGE_VERSION="\K[^"]+' dkms.conf)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install-dkms:
	cp -r $(PWD) /usr/src/$(PKGNAME)-$(VERSION)
	dkms add -m $(PKGNAME) -v $(VERSION)
	dkms build -m $(PKGNAME) -v $(VERSION)
	dkms install -m $(PKGNAME) -v $(VERSION)

uninstall-dkms:
	dkms remove -m $(PKGNAME) -v $(VERSION) --all
	rm -rf /usr/src/$(PKGNAME)-$(VERSION)

.PHONY: all clean install-dkms uninstall-dkms
