DRIVER_TOPDIR = ../../../../..

include $(DRIVER_TOPDIR)/Config.make

obj-m		:= at76c651.o tda8044h.o ves1820.o ves1x93.o

EXTRA_CFLAGS	:= -I$(DRIVER_TOPDIR)/dvb/drivers/media/dvb/dvb-core

include $(DRIVER_TOPDIR)/Rules.make
