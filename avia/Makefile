#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS   = gtx_core.o gtx_dmx.o enx_dmx.o avia.o osd.o enx_core.o gtp_fb.o
obj-m    = gtx_core.o gtx_dmx.o enx_dmx.o avia.o osd.o enx_core.o gtp_fb.o
AVIA_OBJS = avia_core.o
AVIA_OSD  = avia_osd.o
GTP_FB    = gtp-fb.o fbgen.o
GTX_DMX   = gtx-dmx.o crc32.o
ENX_DMX   = enx-dmx.o crc32.o
GTX_CORE  = gtx-core.o
ENX_CORE  = enx-core.o

EXTRA_CFLAGS=-I../include

#remove __ASSEMBLY__ option from kernel makefile
AFLAGS=

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	cp -v $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) $(AVIA_OBJS) $(AVIA_OSD)	\
		$(GTX_DMX) $(GTX_CORE) 		\
		$(ENX_CORE) $(GTP_FB) .*.o.flags *~

gtx_core.o: $(GTX_CORE)
	$(LD) -m elf32ppclinux -r -o $@ $(GTX_CORE)
    
enx_core.o: $(ENX_CORE)
	$(LD) -m elf32ppclinux -r -o $@ $(ENX_CORE)

gtx-dmx.o: gen-dmx.c
enx-dmx.o: gen-dmx.c

gtx_dmx.o: $(GTX_DMX)
	$(LD) -m elf32ppclinux -r -o $@ $(GTX_DMX)

enx_dmx.o: $(ENX_DMX)
	$(LD) -m elf32ppclinux -r -o $@ $(ENX_DMX)

gtp_fb.o: $(GTP_FB)
	$(LD) -m elf32ppclinux -r -o $@ $(GTP_FB)

avia.o: $(AVIA_OBJS)
	$(LD) -m elf32ppclinux -r -o $@ $(AVIA_OBJS)

osd.o: $(AVIA_OSD)
	$(LD) -m elf32ppclinux -r -o $@ $(AVIA_OSD)

include $(KERNEL_LOCATION)/Rules.make
