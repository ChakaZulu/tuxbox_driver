# currently running kernel
CURRENT=$(shell uname -r)

# where the kernel sources are located
#KERNEL_LOCATION=/usr/src/kernel/2.0.35
#KERNEL_LOCATION=/usr/src/kernel/$(CURRENT)
#KERNEL_LOCATION=/usr/src/kernel/vger
KERNEL_LOCATION=/LinuxPPC/usr/src/linux

#GCC = $(CROSS_COMPILE)gcc -I/usr/include/linux
#LD =  $(CROSS_COMPILE)ld
MODULE_DEST= /LinuxPPC/lib/modules/2.4.0-test12/misc/

BUILD_DATE=$(shell date)

#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS       = cxa2092.o

EXTRA_CFLAGS=-DBUILD_DATE='"$(BUILD_DATE)"'

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)
	cp $(M_OBJS) $(MODULE_DEST)

install:
	cp $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) .*.o.flags *~

include $(KERNEL_LOCATION)/Rules.make
