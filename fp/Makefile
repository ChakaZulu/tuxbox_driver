#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS  = dbox2_fp.o dbox2_fp_napi.o dbox2_fp_input.o
FP_OBJS	= dbox2_fp_core.o dbox2_fp_sec.o dbox2_fp_timer.o dbox2_fp_tuner.o dbox2_fp_reset.o
INPUT_OBJS	= dbox2_fp_input_core.o dbox2_fp_rc.o dbox2_fp_keyboard.o dbox2_fp_button.o dbox2_fp_mouse.o

# 2.4.0 Kernel Option
obj-m = $(M_OBJS)

EXTRA_CFLAGS=-I../include -I../dvb/include -I../dvb/drivers/media/dvb -I$(KERNEL_LOCATION)/arch/ppc -DEXPORT_SYMTAB -Werror

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	cp -v $(M_OBJS) $(MODULE_DEST)

clean:
	-rm -f $(M_OBJS) $(O_TARGET) $(FP_OBJS) $(INPUT_OBJS) .*.o.flags *~

dbox2_fp.o: $(FP_OBJS)
	$(LD) -m elf32ppclinux -r -o $@ $(FP_OBJS)

dbox2_fp_input.o: $(INPUT_OBJS)
	$(LD) -m elf32ppclinux -r -o $@ $(INPUT_OBJS)

include $(KERNEL_LOCATION)/Rules.make
