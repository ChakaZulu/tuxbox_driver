#################################################
# some magic for using linux kernel settings
# when compiling module(s)

STANDALONE = 1

M_OBJS   = gtx_dmx.o avia_gt.o avia_gt_fb.o avia_gt_oss.o enx_dmx.o avia.o osd.o gtx_fb.o enx_vbi.o gtx_vbi.o gtx_capture.o gtx_pig.o
obj-m    = gtx_dmx.o avia_gt.o avia_gt_fb.o avia_gt_oss.o enx_dmx.o avia.o osd.o gtx_fb.o enx_vbi.o gtx_vbi.o gtx_capture.o gtx_pig.o

AVIA_OBJS = avia_core.o
AVIA_OSD  = avia_osd.o

AVIA_GT_MODS = avia_gt_gv.o avia_gt_pcm.o avia_gt_capture.o avia_gt_pig.o
AVIA_GT      = avia_gt_core.o avia_gt_enx.o avia_gt_gtx.o
AVIA_GT_FB   = avia_gt_fb_core.o fbgen.o

GTX_DMX   = gtx-dmx.o crc32.o
GTX_FB    = gtx-fb.o fbgen.o

ENX_DMX   = enx-dmx.o crc32.o

EXTRA_CFLAGS=-I../include -DEXPORT_SYMTAB

ifeq ($(STANDALONE), 1)
M_OBJS       += $(AVIA_GT_MODS)
obj-m        += $(AVIA_GT_MODS)
EXTRA_CFLAGS += -DSTANDALONE
else
AVIA_GT      += $(AVIA_GT_MODS)
endif

#remove __ASSEMBLY__ option from kernel makefile
AFLAGS=

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	cp -v $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) $(AVIA_OBJS) $(AVIA_OSD)	\
		$(GTX_DMX) $(GTX_FB) 		\
		$(ENX_DMX) $(AVIA_GT) $(AVIA_GT_FB) .*.o.flags *~

gtx-fb.o: gen-fb.c
gtx-dmx.o: gen-dmx.c
enx-dmx.o: gen-dmx.c

avia.o: $(AVIA_OBJS)
	$(LD) -m elf32ppclinux -r -o $@ $(AVIA_OBJS)

osd.o: $(AVIA_OSD)
	$(LD) -m elf32ppclinux -r -o $@ $(AVIA_OSD)

gtx_dmx.o: $(GTX_DMX)
	$(LD) -m elf32ppclinux -r -o $@ $(GTX_DMX)

gtx_fb.o: $(GTX_FB)
	$(LD) -m elf32ppclinux -r -o $@ $(GTX_FB)

avia_gt.o: $(AVIA_GT)
	$(LD) -m elf32ppclinux -r -o $@ $(AVIA_GT)

avia_gt_fb.o: $(AVIA_GT_FB)
	$(LD) -m elf32ppclinux -r -o $@ $(AVIA_GT_FB)

enx_dmx.o: $(ENX_DMX)
	$(LD) -m elf32ppclinux -r -o $@ $(ENX_DMX)

include $(KERNEL_LOCATION)/Rules.make
