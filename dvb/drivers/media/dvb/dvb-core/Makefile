#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS = dmxdev.o dvb_i2c.o dvb_demux.o dvb_filter.o dvb_frontend.o dvb_net.o dvbdev.o dvb_copy.o

# 2.4.0 Kernel Option
obj-m = $(M_OBJS)

EXTRA_CFLAGS=-I../../../../include -DEXPORT_SYMTAB -D__DVB_PACK__ -MD -Werror

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	cp -v $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) .*.o.flags *~

include $(KERNEL_LOCATION)/Rules.make


