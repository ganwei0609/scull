DEBUG=y
ifeq ($(DEBUG),y)
	DEBFLAGS = -O -g -DSCULL_DEBUG
else
	DEBFLAGS = -O2
endif

ccflags-y += $(DEBFLAGS)
#ccflags-y += -D__KERNEL__
TARGET = ganwei
MAKE = make

ifneq ($(KERNELRELEASE),)

ganwei-objs:=sculldelay.o
obj-m:=ganwei.o 

else

KERDIR?=/lib/modules/$(shell uname -r)/build  

PWD :=$(shell pwd)
INSTALLDIR := $(PWD)/out
modules:          
	$(MAKE) -C $(KERDIR) M=$(PWD) modules      

endif

clean:          
	$(MAKE) -C $(KERDIR) SUBDIRS=$(PWD) clean  
