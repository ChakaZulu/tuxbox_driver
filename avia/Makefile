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

#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS   = gtx.o aviam.o

AVIA_OBJS = 500v093.o 600vb017.o avia.o
GTX_OBJS  = gtx-core.o gtx-dmx.o gtx-fb.o ucode.o fbgen.o 

#remove __ASSEMBLY__ option from kernel makefile
AFLAGS=

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)
	cp $(M_OBJS) $(MODULE_DEST)

install:
	cp $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) $(AVIA_OBJS) $(GTX_OBJS) .*.o.flags *~

gtx.o: $(GTX_OBJS)
	$(LD) -m elf32ppclinux -r -o $@ $(GTX_OBJS)
    
aviam.o: $(AVIA_OBJS)
	$(LD) -m elf32ppclinux -r -o $@ $(AVIA_OBJS)

include $(KERNEL_LOCATION)/Rules.make
