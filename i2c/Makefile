# currently running kernel
CURRENT=$(shell uname -r)

# where the kernel sources are located
#KERNEL_LOCATION=/usr/src/kernel/2.0.35
#KERNEL_LOCATION=/usr/src/kernel/$(CURRENT)
#KERNEL_LOCATION=/usr/src/kernel/vger
KERNEL_LOCATION=/usr/src/linux

#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS       = i2c-8xx.o
GCC = powerpc-linux-gcc -I/usr/include/linux
LD =  powerpc-linux-ld
here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)
#	$(LD) -m elf32ppclinux -r -o $(O_TARGET) $(M_OBJS)
	cp $(M_OBJS) /LinuxPPC/lib/modules/2.4.0-test12/misc/
#install:
#	su -c "cp -v $(M_OBJS) $(MX_OBJS) /lib/modules/$(CURRENT)/misc"

clean:
	-rm -f $(M_OBJS) $(O_TARGET) .*.o.flags *~

include $(KERNEL_LOCATION)/Rules.make
