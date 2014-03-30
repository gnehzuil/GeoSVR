KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
EXTRA_CFLAGS = -g -Wall

obj-m := geosvr.o
geosvr-objs := geosvr_mod.o geosvr_netlink.o geosvr_queue.o geosvr_ipenc.o

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

.PHONY:
	clean

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
