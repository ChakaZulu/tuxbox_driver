#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS       = cam.o

obj-m = $(M_OBJS)

EXTRA_CFLAGS=-I../../include

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)
	cp $(M_OBJS) $(MODULE_DEST)

install:
	cp $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) $(APPL) .*.o.flags *~

include $(KERNEL_LOCATION)/Rules.make
