#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS    = dvb_demux.o dvbdev.o dvb.o dmxdev.o dvb_net.o dvb_frontend.o
TEST_APPZ = test_audio test_av test_dmx test_front test_video


# 2.4.0 Kernel Option
obj-m = $(M_OBJS)

EXTRA_CFLAGS=-I../include -DEXPORT_SYMTAB -D__DVB_PACK__

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	cp -v $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) $(TEST_APPZ) .*.o.flags *~

include $(KERNEL_LOCATION)/Rules.make


