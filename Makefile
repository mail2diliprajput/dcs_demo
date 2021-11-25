KDIR ?= /lib/modules/`uname -r`/build

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

default:
	$(MAKE) -C $(KDIR) M=$$PWD

defconfig:

# Module specific targets
all: default