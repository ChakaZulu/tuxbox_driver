#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS       = decode test_dmx test_front
#scan_cable

# 2.4.0 Kernel Option
obj-m = $(M_OBJS)

EXTRA_CFLAGS=-I../include

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)
	cp $(M_OBJS) $(MODULE_DEST)

install:
	cp $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) $(APPL) .*.o.flags *~

decode:
	powerpc-linux-gcc $(EXTRA_CFLAGS) -o decode decode.c

test_dmx:
	powerpc-linux-gcc $(EXTRA_CFLAGS) -o test_dmx test_dmx.c

test_front:
	powerpc-linux-gcc $(EXTRA_CFLAGS) -o test_front test_front.c

scan_cable:
	powerpc-linux-gcc $(EXTRA_CFLAGS) -o scan_cable scan_cable.c

include $(KERNEL_LOCATION)/Rules.make
