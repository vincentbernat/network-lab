# Kernel module compilation
KDIR = /lib/modules/$(shell uname -r)/build
obj-m += livepatch-teamd-ab.o
livepatch-teamd-ab.ko: livepatch-teamd-ab.c
	make -C $(KDIR) M=$(PWD) modules
