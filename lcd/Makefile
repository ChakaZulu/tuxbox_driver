#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS	     = lcd.o
LCD_OBJS     = lcd-console.o lcd-font.o lcd-ks0713.o

# 2.4.X Kernel Option
obj-m = $(M_OBJS)

EXTRA_CFLAGS=-I../include -I$(KERNEL_LOCATION)/arch/ppc/8xx_io -I$(KERNEL_LOCATION)/include -DEXPORT_SYMTAB

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	cp -v $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) $(LCD_OBJS) .*.o.flags *~

lcd.o: $(LCD_OBJS)
	$(LD) -m elf32ppclinux -r -o $@ $(LCD_OBJS)

include $(KERNEL_LOCATION)/Rules.make
