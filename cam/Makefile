#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS       = cam.o cam_napi.o

obj-m = $(M_OBJS)

EXTRA_CFLAGS=-I../include -I../dvb/include -I../dvb/drivers/media/dvb -I$(KERNEL_LOCATION)/arch/ppc -DEXPORT_SYMTAB

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	cp -v $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) .*.o.flags *~

include $(KERNEL_LOCATION)/Rules.make
