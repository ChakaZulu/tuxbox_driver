#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS = dvb-core.o

DVB_CORE = dvbdev.o dmxdev.o dvb_demux.o dvb_filter.o \
	   dvb_frontend.o dvb_i2c.o dvb_net.o dvb_ksyms.o

# 2.4.0 Kernel Option
obj-m = $(M_OBJS)

EXTRA_CFLAGS=-I../../../../include -I$(KERNEL_LOCATION)/arch/ppc -DEXPORT_SYMTAB -MD -Werror

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	cp -v $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(DVB_CORE) $(O_TARGET) .*.o.flags *~ *.d

dvb-core.o: $(DVB_CORE)
	$(LD) -m elf32ppclinux -r -o $@ $(DVB_CORE)

include $(KERNEL_LOCATION)/Rules.make
-include $(wildcard *.d) dummy
