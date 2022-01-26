KDIR ?= /lib/modules/`uname -r`/build

.PHONY: clean default defconfig overlay all

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

default:
	$(MAKE) -C $(KDIR) M=$$PWD

defconfig:

overlay: nlcamerapanel.dtbo
	sudo cp nlcamerapanel.dtbo /boot/overlays/

all: default

%.dtbo: %.dts
	dtc -I dts -O dtb -o $@ $<