DRIVER_TOPDIR = ../../../../..

include $(DRIVER_TOPDIR)/Config.make

export-objs	:= avia_av_ksyms.o avia_av_napi.o avia_gt_accel.o avia_gt_core.o
obj-m		:= avia_av.o avia_av_napi.o avia_gt.o avia_napi.o

av-core-objs	:= avia_av_core.o avia_av_ksyms.o
av-misc-objs	:= avia_av_event.o avia_av_proc.o

gt-core-objs	:= avia_gt_core.o avia_gt_enx.o avia_gt_gtx.o avia_gt_ucode.o
gt-fb-objs	:= avia_gt_fb_core.o fbgen.o
gt-misc-objs	:= avia_gt_accel.o

include .config

ifeq ($(CONFIG_AVIA_GT_CAPTURE),y)
export-objs	+= avia_gt_capture.o avia_gt_pig.o
gt-misc-objs	+= avia_gt_capture.o avia_gt_pig.o
obj-m		+= avia_gt_v4l2.o
endif
ifeq ($(CONFIG_AVIA_GT_DMX),y)
export-objs	+= avia_gt_dmx.o avia_gt_vbi.o avia_gt_napi.o
gt-misc-objs	+= avia_gt_dmx.o avia_gt_vbi.o
obj-m		+= avia_gt_napi.o
endif
ifeq ($(CONFIG_AVIA_GT_GV),y)
export-objs	+= avia_gt_gv.o fbgen.o
gt-misc-objs	+= avia_gt_gv.o
obj-m		+= avia_gt_fb.o
endif
ifeq ($(CONFIG_AVIA_GT_IR),y)
export-objs	+= avia_gt_ir.o
gt-misc-objs	+= avia_gt_ir.o
obj-m		+= avia_gt_ir_samsung.o avia_gt_lirc.o
endif
ifeq ($(CONFIG_AVIA_GT_PCM),y)
export-objs	+= avia_gt_pcm.o
gt-misc-objs	+= avia_gt_pcm.o
obj-m		+= avia_gt_oss.o
endif

EXTRA_CFLAGS    += -I$(DRIVER_TOPDIR)/dvb/drivers/media/dvb/dvb-core

ifeq ($(CONFIG_AVIA_DEBUG),y)
EXTRA_CFLAGS	+= -DDEBUG
endif

ifeq ($(CONFIG_AVIA_STANDALONE),y)
obj-m		+= $(av-misc-objs) $(gt-misc-objs)
EXTRA_CFLAGS	+= -DSTANDALONE
else
av-core-objs	+= $(av-misc-objs)
gt-core-objs	+= $(gt-misc-objs)
endif

avia_av.o: $(av-core-objs)
	$(LD) -r -o $@ $(av-core-objs)

avia_gt.o: $(gt-core-objs)
	$(LD) -r -o $@ $(gt-core-objs)

avia_gt_fb.o: $(gt-fb-objs)
	$(LD) -r -o $@ $(gt-fb-objs)

fbgen.c: $(KERNEL_LOCATION)/drivers/video/fbgen.c
	ln -s $(KERNEL_LOCATION)/drivers/video/fbgen.c

#
# all modules depend on the configuration file
#
$(obj-m): avia_gt_config.h

#
# create a default makefile configuration if there is no custom .config
#
.config:
	if [ ! -e .config ]; then						\
		echo \# CONFIG_AVIA_DEBUG is not set		>> .config;	\
		echo CONFIG_AVIA_GT_CAPTURE=y			>> .config;	\
		echo CONFIG_AVIA_GT_DMX=y			>> .config;	\
		echo CONFIG_AVIA_GT_GV=y			>> .config;	\
		echo CONFIG_AVIA_GT_IR=y			>> .config;	\
		echo CONFIG_AVIA_GT_PCM=y			>> .config;	\
		echo \# CONFIG_AVIA_STANDALONE is not set	>> .config;	\
	fi

#
# recreate the configuration header every time .config changes
#
avia_gt_config.h: .config
	@echo \#ifndef __avia_gt_config_h__		>  avia_gt_config.h
	@echo \#define __avia_gt_config_h__		>> avia_gt_config.h
	@if [ "$(CONFIG_AVIA_GT_CAPTURE)" = "y" ]; then				\
		echo \#define CONFIG_AVIA_GT_CAPTURE 1	>> avia_gt_config.h;	\
	else									\
		echo \#undef CONFIG_AVIA_GT_CAPTURE	>> avia_gt_config.h;	\
	fi
	@if [ "$(CONFIG_AVIA_GT_DMX)" = "y" ]; then				\
		echo \#define CONFIG_AVIA_GT_DMX 1	>> avia_gt_config.h;	\
	else									\
		echo \#undef CONFIG_AVIA_GT_DMX		>> avia_gt_config.h;	\
	fi
	@if [ "$(CONFIG_AVIA_GT_GV)" = "y" ]; then				\
		echo \#define CONFIG_AVIA_GT_GV 1	>> avia_gt_config.h;	\
	else									\
		echo \#undef CONFIG_AVIA_GT_GV		>> avia_gt_config.h;	\
	fi
	@if [ "$(CONFIG_AVIA_GT_IR)" = "y" ]; then				\
		echo \#define CONFIG_AVIA_GT_IR 1	>> avia_gt_config.h;	\
	else									\
		echo \#undef CONFIG_AVIA_GT_IR		>> avia_gt_config.h;	\
	fi
	@if [ "$(CONFIG_AVIA_GT_PCM)" = "y" ]; then				\
		echo \#define CONFIG_AVIA_GT_PCM 1	>> avia_gt_config.h;	\
	else									\
		echo \#undef CONFIG_AVIA_GT_PCM		>> avia_gt_config.h;	\
	fi
	@echo \#endif					>> avia_gt_config.h

uninstall:
	rm $(MODLIB)/misc/avia*.o

include $(DRIVER_TOPDIR)/Rules.make
