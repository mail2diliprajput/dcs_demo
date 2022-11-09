KDIR ?= /lib/modules/`uname -r`/build
OVERLAY_DIR ?= /boot/overlays/

.PHONY: clean default defconfig overlay all

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

default: nlcamerapanel.dtbo
	$(MAKE) -C $(KDIR) M=$$PWD

defconfig:

overlay: nlcamerapanel.dtbo
	sudo cp nlcamerapanel.dtbo OVERLAY_DIR

install: all
	sudo rm /lib/modules/`uname -r`/kernel/drivers/gpu/drm/panel/nlcamerapanel.ko.xz
	sudo cp nlcamerapanel.ko /lib/modules/`uname -r`/kernel/drivers/gpu/drm/panel/
	sudo xz /lib/modules/`uname -r`/kernel/drivers/gpu/drm/panel/nlcamerapanel.ko

all: default

%.dtbo: %.dts
	dtc -I dts -O dtb -o $@ $<