#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS   = ves1820.o ves1893.o at76c651.o
obj-m  = $(M_OBJS)

#remove __ASSEMBLY__ option from kernel makefile
AFLAGS=
EXTRA_CFLAGS=-I../../include

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	cp -v $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) .*.o.flags *~

include $(KERNEL_LOCATION)/Rules.make
