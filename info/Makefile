#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS   = info.o
obj-m    = info.o

EXTRA_CFLAGS=-I../include -I../cvs/driver/include

#remove __ASSEMBLY__ option from kernel makefile
AFLAGS=

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	cp -v $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) .*.o.flags *~

include $(KERNEL_LOCATION)/Rules.make
