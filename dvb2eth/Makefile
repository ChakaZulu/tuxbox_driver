DRIVER_TOPDIR = ..

include $(DRIVER_TOPDIR)/Config.make

obj-m		:= dvb2eth.o
export-objs	:= dvb2eth.o

EXTRA_CFLAGS	:= -I$(DRIVER_TOPDIR)/dvb/drivers/media/dvb

include $(DRIVER_TOPDIR)/Rules.make
