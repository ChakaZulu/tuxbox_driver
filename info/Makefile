DRIVER_TOPDIR = ..

obj-m		:= info.o tuxbox.o
export-objs	:= info.o

TUXBOX		:= tuxbox_core.o tuxbox_hardware_dbox2.o

tuxbox.o: $(TUXBOX)
	$(LD) -r -o $@ $(TUXBOX)

include $(DRIVER_TOPDIR)/Rules.make
