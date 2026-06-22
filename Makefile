obj-m += main.o

KDIR ?= /home/vikram/buildroot-2025.02.14/output/build/linux-custom
CROSS_COMPILE ?= /home/vikram/buildroot-2025.02.14/output/host/bin/arm-buildroot-linux-gnueabihf-

all:
	$(MAKE) ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) clean

