#################################################
# some magic for using linux kernel settings
# when compiling module(s)

DEBUG = 0
STANDALONE = 0

M_OBJS   = avia_av.o avia_gt.o avia_gt_napi.o avia_gt_fb.o avia_gt_oss.o avia_gt_vbi.o avia_gt_lirc.o avia_gt_dvr.o avia.o osd.o
obj-m    = avia_av.o avia_gt.o avia_gt_napi.o avia_gt_fb.o avia_gt_oss.o avia_gt_vbi.o avia_gt_lirc.o avia_gt_dvr.o avia.o osd.o

AVIA_OBJS = avia_core.o
AVIA_OSD  = avia_osd.o

FB_GEN = $(KERNEL_LOCATION)/drivers/video/fbgen.o

#AVIA_AV_MODS = avia_av_osd.o 
AVIA_AV_MODS = avia_av_event.o avia_av_proc.o
AVIA_AV      = avia_av_core.o 

AVIA_GT_MODS = avia_gt_accel.o avia_gt_dmx.o avia_gt_gv.o avia_gt_pcm.o avia_gt_capture.o avia_gt_pig.o avia_gt_ir.o
AVIA_GT      = avia_gt_core.o avia_gt_enx.o avia_gt_gtx.o
AVIA_GT_FB   = avia_gt_fb_core.o $(FB_GEN)

EXTRA_CFLAGS = -I../include -DEXPORT_SYMTAB -Werror

ifeq ($(DEBUG), 1)
EXTRA_CFLAGS	+= -DDEBUG
endif

ifeq ($(STANDALONE), 1)
M_OBJS			+= $(AVIA_AV_MODS) $(AVIA_GT_MODS)
obj-m			+= $(AVIA_AV_MODS) $(AVIA_GT_MODS)
EXTRA_CFLAGS	+= -DSTANDALONE
else
AVIA_AV			+= $(AVIA_AV_MODS)
AVIA_GT			+= $(AVIA_GT_MODS)
endif

#remove __ASSEMBLY__ option from kernel makefile
AFLAGS=

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	cp -v $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) $(AVIA_OBJS) $(AVIA_OSD)	\
		$(AVIA_GT) $(AVIA_GT_FB) .*.o.flags *~

avia.o: $(AVIA_OBJS) 
	$(LD) -m elf32ppclinux -r -o $@ $(AVIA_OBJS)

osd.o: $(AVIA_OSD)
	$(LD) -m elf32ppclinux -r -o $@ $(AVIA_OSD)

avia_av.o: $(AVIA_AV)
	$(LD) -m elf32ppclinux -r -o $@ $(AVIA_AV)

avia_gt.o: $(AVIA_GT)
	$(LD) -m elf32ppclinux -r -o $@ $(AVIA_GT)

avia_gt_fb.o: $(AVIA_GT_FB)
	$(LD) -m elf32ppclinux -r -o $@ $(AVIA_GT_FB)

include $(KERNEL_LOCATION)/Rules.make
