DRIVER_TOPDIR = ../../../../..

include $(DRIVER_TOPDIR)/Config.make

obj-m		:= dvb-core.o dvb_i2c_bridge.o
export-objs	:= dvb_compat.o dvb_functions.o dvb_ksyms.o dvb_ringbuffer.o dvb_i2c_bridge.o

dvb-core-objs = dvbdev.o dmxdev.o dvb_demux.o dvb_filter.o \
		dvb_compat.o dvb_functions.o dvb_frontend.o dvb_i2c.o dvb_net.o dvb_ksyms.o dvb_ringbuffer.o

dvb-core.o: $(dvb-core-objs)
	$(LD) -r -o $@ $(dvb-core-objs)

include $(DRIVER_TOPDIR)/Rules.make
