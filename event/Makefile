#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS       = event.o

# 2.4.0 Kernel Option
obj-m        = $(M_OBJS)

EXTRA_CFLAGS=-I../include -I$(KERNEL_LOCATION)/arch/ppc/8xx_io -I$(KERNEL_LOCATION)/include/asm-ppc -DEXPORT_SYMTAB

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	cp -v $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) .*.o.flags *~

include $(KERNEL_LOCATION)/Rules.make

