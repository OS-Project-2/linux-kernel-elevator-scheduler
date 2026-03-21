obj-m += syscalls.o
KDIR = /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(PWD) modules
	make -C src

clean:
	make -C $(KDIR) M=$(PWD) clean
	make -C src clean
