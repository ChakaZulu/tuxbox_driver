#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS       = decode test_dmx test_front test_tbl test_sec
#scan_cable

# 2.4.0 Kernel Option
obj-m = $(M_OBJS)

EXTRA_CFLAGS=-I../include -I$(KERNEL_LOCATION)/include

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)
#	cp $(M_OBJS) $(BIN_DEST)

install:
	cp $(M_OBJS) $(BIN_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) $(APPL) .*.o.flags *~

decode:
	$(CROSS_COMPILE)gcc $(EXTRA_CFLAGS) -o decode decode.c

test_dmx:
	$(CROSS_COMPILE)gcc $(EXTRA_CFLAGS) -o test_dmx test_dmx.c

test_front:
	$(CROSS_COMPILE)gcc $(EXTRA_CFLAGS) -o test_front test_front.c

test_tbl:
	powerpc-linux-gcc $(EXTRA_CFLAGS) -o test_tbl test_tbl.c

scan_cable:
	$(CROSS_COMPILE)gcc $(EXTRA_CFLAGS) -o scan_cable scan_cable.c

test_sec:
	$(CROSS_COMPILE)gcc $(EXTRA_CFLAGS) -o test_sec test_sec.c

include $(KERNEL_LOCATION)/Rules.make
