# currently running kernel
CURRENT=$(shell uname -r)

# where the kernel sources are located
#KERNEL_LOCATION=/usr/src/kernel/2.0.35
#KERNEL_LOCATION=/usr/src/kernel/$(CURRENT)
#KERNEL_LOCATION=/usr/src/kernel/vger
KERNEL_LOCATION=/LinuxPPC/usr/src/linux

#GCC = $(CROSS_COMPILE)gcc -I/usr/include/linux
#LD =  $(CROSS_COMPILE)ld
MODULE_DEST= /LinuxPPC/lib/modules/2.4.1/misc/

#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS    = saa7126.o
SAA_OBJS  = saa7126_core.o saa7126_calc.o

# 2.4.0 Kernel Option
obj-m = $(M_OBJS)

EXTRA_CFLAGS=

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)
	cp $(M_OBJS) $(MODULE_DEST)

install:
	cp $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) $(SAA_OBJS) .*.o.flags *~

saa7126.o: $(SAA_OBJS)
	$(LD) -m elf32ppclinux -r -o $@ $(SAA_OBJS)

include $(KERNEL_LOCATION)/Rules.make
