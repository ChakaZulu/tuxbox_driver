# currently running kernel
CURRENT=$(shell uname -r)

# where the kernel sources are located
#KERNEL_LOCATION=/usr/src/kernel/2.0.35
#KERNEL_LOCATION=/usr/src/kernel/$(CURRENT)
#KERNEL_LOCATION=/usr/src/kernel/vger
KERNEL_LOCATION=/LinuxPPC/usr/src/linux

#GCC = $(CROSS_COMPILE)gcc -I/usr/include/linux
#LD =  $(CROSS_COMPILE)ld
MODULE_DEST= /LinuxPPC/lib/modules/2.4.0/misc/

BUILD_DATE=$(shell date)

#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS   = gtx_core.o gtx_fb.o gtx_dmx.o aviam.o
obj-m  = gtx_core.o gtx_fb.o gtx_dmx.o aviam.o
AVIA_OBJS = 500v093.o 600vb017.o avia.o
GTX_FB    = gtx-fb.o fbgen.o
GTX_DMX   = gtx-dmx.o
GTX_CORE  = gtx-core.o ucode.o

#EXTRA_CFLAGS=-DBUILD_DATE='"$(BUILD_DATE)"'
EXTRA_CFLAGS=-I../../include

#remove __ASSEMBLY__ option from kernel makefile
AFLAGS=

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)
	cp $(M_OBJS) $(MODULE_DEST)

install:
	cp $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) $(AVIA_OBJS) $(GTX_FB) $(GTX_DMX) $(GTX_CORE) .*.o.flags *~

gtx_core.o: $(GTX_CORE)
	$(LD) -m elf32ppclinux -r -o $@ $(GTX_CORE)
    
gtx_dmx.o: $(GTX_DMX)
	$(LD) -m elf32ppclinux -r -o $@ $(GTX_DMX)

gtx_fb.o: $(GTX_FB)
	$(LD) -m elf32ppclinux -r -o $@ $(GTX_FB)

aviam.o: $(AVIA_OBJS)
	$(LD) -m elf32ppclinux -r -o $@ $(AVIA_OBJS)

include $(KERNEL_LOCATION)/Rules.make
