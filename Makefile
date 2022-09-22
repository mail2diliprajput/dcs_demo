KDIR ?= /lib/modules/`uname -r`/build
OVERLAY_DIR ?= /boot/overlays/

.PHONY: clean default defconfig overlay all

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

default:
	$(MAKE) -C $(KDIR) M=$$PWD

defconfig:

overlay: nlcamerapanel.dtbo
	sudo cp nlcamerapanel.dtbo OVERLAY_DIR

all: default

%.dtbo: %.dts
	dtc -I dts -O dtb -o $@ $<