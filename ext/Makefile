DRIVER_TOPDIR = ..

include $(DRIVER_TOPDIR)/Config.make

obj-m		:= aviaEXT.o
export-objs	:= aviaEXT.o

EXTRA_CFLAGS	:= -I$(DRIVER_TOPDIR)/dvb/drivers/media/dvb/avia

include $(DRIVER_TOPDIR)/Rules.make
