#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS   = info.o tuxbox.o
obj-m    = info.o tuxbox.o

EXTRA_CFLAGS=-I$(KERNEL_LOCATION)/arch/ppc -I../include -I../cvs/driver/include -DEXPORT_SYMTAB

#remove __ASSEMBLY__ option from kernel makefile
AFLAGS=

TUXBOX = tuxbox_core.o tuxbox_hardware_dbox2.o

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	cp -v $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) .*.o.flags *~

tuxbox.o: $(TUXBOX)
	$(LD) -m elf32ppclinux -r -o $@ $(TUXBOX)

include $(KERNEL_LOCATION)/Rules.make
