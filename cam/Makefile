DRIVER_TOPDIR = ..

include $(DRIVER_TOPDIR)/Config.make

obj-m		:= cam.o cam_napi.o
export-objs	:= cam.o

EXTRA_CFLAGS	:= -I$(DRIVER_TOPDIR)/dvb/drivers/media/dvb

include $(DRIVER_TOPDIR)/Rules.make
