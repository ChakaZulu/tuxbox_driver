DRIVER_TOPDIR = ..

obj-m		:= dbox2_fp.o dbox2_fp_napi.o dbox2_fp_input.o
export-objs	:= dbox2_fp_core.o dbox2_fp_sec.o dbox2_fp_tuner.o dbox2_fp_reset.o

FP_OBJS		:= dbox2_fp_core.o dbox2_fp_sec.o dbox2_fp_timer.o dbox2_fp_tuner.o dbox2_fp_reset.o
INPUT_OBJS	:= dbox2_fp_input_core.o dbox2_fp_rc.o dbox2_fp_keyboard.o dbox2_fp_button.o dbox2_fp_mouse.o

EXTRA_CFLAGS	:= -I$(DRIVER_TOPDIR)/dvb/drivers/media/dvb -Werror

dbox2_fp.o: $(FP_OBJS)
	$(LD) -r -o $@ $(FP_OBJS)

dbox2_fp_input.o: $(INPUT_OBJS)
	$(LD) -r -o $@ $(INPUT_OBJS)

include $(DRIVER_TOPDIR)/Rules.make
