#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS   = gtx_core.o gtx_fb.o gtx_dmx.o avia.o osd.o enx_core.o
obj-m  = gtx_core.o gtx_fb.o gtx_dmx.o avia.o osd.o enx_core.o
AVIA_OBJS = avia_core.o
AVIA_OSD = avia_osd.o
GTX_FB    = gtx-fb.o fbgen.o
GTX_DMX   = gtx-dmx.o
GTX_CORE  = gtx-core.o
ENX_CORE  = enx-core.o

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
    
enx_core.o: $(ENX_CORE)
	$(LD) -m elf32ppclinux -r -o $@ $(ENX_CORE)

gtx_dmx.o: $(GTX_DMX)
	$(LD) -m elf32ppclinux -r -o $@ $(GTX_DMX)

gtx_fb.o: $(GTX_FB)
	$(LD) -m elf32ppclinux -r -o $@ $(GTX_FB)

avia.o: $(AVIA_OBJS)
	$(LD) -m elf32ppclinux -r -o $@ $(AVIA_OBJS)

osd.o: $(AVIA_OSD)
	$(LD) -m elf32ppclinux -r -o $@ $(AVIA_OSD)

include $(KERNEL_LOCATION)/Rules.make
