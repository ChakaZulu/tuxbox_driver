DRIVER_TOPDIR = ..

include $(DRIVER_TOPDIR)/Config.make

obj-m		:= dboxide.o

IDE_OBJS	:= main.o transfer.o trace.o

dboxide.o: $(IDE_OBJS)
	$(LD) -r -o $@ $(IDE_OBJS)

include $(DRIVER_TOPDIR)/Rules.make
