#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS       = dbox2_i2c.o dbox2_i2c_napi.o

# 2.4.x Kernel Option
obj-m        = $(M_OBJS)

EXTRA_CFLAGS=-I$(KERNEL_LOCATION)/arch/ppc/8xx_io -I$(KERNEL_LOCATION)/arch/ppc -I$(KERNEL_LOCATION)/include -I../include -DEXPORT_SYMTAB

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	cp -v $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) .*.o.flags *~

include $(KERNEL_LOCATION)/Rules.make

