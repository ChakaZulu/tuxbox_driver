#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS       = i2c-8xx.o

# 2.4.0 Kernel Option
obj-m        = $(M_OBJS)

EXTRA_CFLAGS=-I$(KERNEL_LOCATION)/arch/ppc/8xx_io

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)
	cp $(M_OBJS) $(MODULE_DEST)

install:
	cp $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) .*.o.flags *~

include $(KERNEL_LOCATION)/Rules.make

