#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS   = gtx_core.o gtx_dmx.o enx_core.o enx_dmx.o avia.o osd.o enx_fb.o gtx_fb.o gtx-capture.o enx_vbi.o gtx_vbi.o enx_capture.o gtx_capture.o enx_pig.o gtx_pig.o
obj-m    = gtx_core.o gtx_dmx.o enx_core.o enx_dmx.o avia.o osd.o enx_fb.o gtx_fb.o gtx-capture.o enx_vbi.o gtx_vbi.o enx_capture.o gtx_capture.o enx_pig.o gtx_pig.o

AVIA_OBJS = avia_core.o
AVIA_OSD  = avia_osd.o

GTX_CORE  = gtx-core.o
GTX_DMX   = gtx-dmx.o crc32.o
GTX_FB    = gtx-fb.o fbgen.o

ENX_CORE  = enx-core.o
ENX_DMX   = enx-dmx.o crc32.o
ENX_FB    = enx-fb.o fbgen.o

EXTRA_CFLAGS=-I../include -DEXPORT_SYMTAB

#remove __ASSEMBLY__ option from kernel makefile
AFLAGS=

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	cp -v $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) $(AVIA_OBJS) $(AVIA_OSD)	\
		$(GTX_DMX) $(GTX_CORE) $(GTX_FB) 		\
		$(ENX_DMX) $(ENX_CORE) $(ENX_FB) .*.o.flags *~

gtx-fb.o: gen-fb.c
enx-fb.o: gen-fb.c
gtx-dmx.o: gen-dmx.c
enx-dmx.o: gen-dmx.c

avia.o: $(AVIA_OBJS)
	$(LD) -m elf32ppclinux -r -o $@ $(AVIA_OBJS)

osd.o: $(AVIA_OSD)
	$(LD) -m elf32ppclinux -r -o $@ $(AVIA_OSD)

gtx_core.o: $(GTX_CORE)
	$(LD) -m elf32ppclinux -r -o $@ $(GTX_CORE)

gtx_dmx.o: $(GTX_DMX)
	$(LD) -m elf32ppclinux -r -o $@ $(GTX_DMX)

gtx_fb.o: $(GTX_FB)
	$(LD) -m elf32ppclinux -r -o $@ $(GTX_FB)

enx_core.o: $(ENX_CORE)
	$(LD) -m elf32ppclinux -r -o $@ $(ENX_CORE)

enx_dmx.o: $(ENX_DMX)
	$(LD) -m elf32ppclinux -r -o $@ $(ENX_DMX)

enx_fb.o: $(ENX_FB)
	$(LD) -m elf32ppclinux -r -o $@ $(ENX_FB)

include $(KERNEL_LOCATION)/Rules.make
