#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS       = demux.o dvbdev.o dvb.o dmxdev.o
TEST = test_audio test_av test_dmx test_front test_video


# 2.4.0 Kernel Option
obj-m = $(M_OBJS)

EXTRA_CFLAGS=-I../include

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)
	cp $(M_OBJS) $(MODULE_DEST)

install:
	cp $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) $(APPZ) .*.o.flags *~

include $(KERNEL_LOCATION)/Rules.make


