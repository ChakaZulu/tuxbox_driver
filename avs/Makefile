#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS       = cxa2092.o
APPL	     = switch
APPL_SOURCE  = switch.c

# 2.4.0 Kernel Option
obj-m = $(M_OBJS)

EXTRA_CFLAGS=

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)
	cp $(M_OBJS) $(MODULE_DEST)

install:
	cp $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) $(APPL) .*.o.flags *~

appl:
	powerpc-linux-gcc -o $(APPL) $(APPL_SOURCE)

include $(KERNEL_LOCATION)/Rules.make
