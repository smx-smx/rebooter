EXTRA_CFLAGS += 

obj-m += mymod.o
mymod-objs := reboot.o

.PHONY=all clean

KDIR ?= kernel-bcm

all:
	make -C $(KDIR)/ M=$(PWD) KBUILD_EXTMOD=`pwd` modules

clean:
	make -C $(KDIR)/ M=$(PWD) clean
	rm -f *.order *.o

