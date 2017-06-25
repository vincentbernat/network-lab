# Kernel module compilation
obj-m += kbench_mod.o
kbench_mod.ko: kbench_mod.c
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
