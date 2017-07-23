# Kernel module compilation
KDIR = /lib/modules/$(shell uname -r)/build
obj-m += kbench_mod.o
kbench_mod.ko: kbench_mod.c
	make -C $(KDIR) M=$(PWD) modules
