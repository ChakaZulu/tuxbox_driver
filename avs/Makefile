#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS       = avs.o
AVS_OBJS     = cxa2092.o cxa2126.o avs_core.o

# 2.4.0 Kernel Option
obj-m = $(M_OBJS)

EXTRA_CFLAGS=

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	cp -v $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) $(AVS_OBJS) .*.o.flags *~

avs.o:	$(AVS_OBJS)
	$(LD) -m elf32ppclinux -r -o $@ $(AVS_OBJS)

include $(KERNEL_LOCATION)/Rules.make
