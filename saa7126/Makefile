#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS    = saa7126.o
SAA_OBJS  = saa7126_core.o

# 2.4.0 Kernel Option
obj-m = $(M_OBJS)

EXTRA_CFLAGS=-I$(KERNEL_LOCATION)/arch/ppc -I../include -DEXPORT_SYMTAB

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	cp -v $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) $(SAA_OBJS) .*.o.flags *~

saa7126.o: $(SAA_OBJS)
	$(LD) -m elf32ppclinux -r -o $@ $(SAA_OBJS)

include $(KERNEL_LOCATION)/Rules.make
