#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS       = cam.o

obj-m = $(M_OBJS) camd

EXTRA_CFLAGS=-I../../include -I$(KERNEL_LOCATION)/include

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)
	cp $(M_OBJS) $(MODULE_DEST)

install:
	cp $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) $(APPL) .*.o.flags *~

camd:
	$(CROSS_COMPILE)gcc $(EXTRA_CFLAGS) -o camd camd.c

include $(KERNEL_LOCATION)/Rules.make
