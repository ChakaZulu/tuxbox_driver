DRIVER_TOPDIR = ../../../../..

include $(DRIVER_TOPDIR)/Config.make

obj-m		:= dvb-core.o dvb_i2c_bridge.o
export-objs	:= dvb_ksyms.o compat.o dvb_i2c_bridge.o

DVB_CORE	:= dvbdev.o dmxdev.o dvb_demux.o dvb_filter.o \
		   dvb_frontend.o dvb_i2c.o dvb_net.o dvb_ksyms.o compat.o

dvb-core.o: $(DVB_CORE)
	$(LD) -r -o $@ $(DVB_CORE)

include $(DRIVER_TOPDIR)/Rules.make
