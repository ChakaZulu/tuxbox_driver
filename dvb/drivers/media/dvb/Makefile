
mod-subdirs := avia dvb-core frontends

subdir-y := avia dvb-core frontends

subdir-m := $(subdir-y)

include $(KERNEL_LOCATION)/Rules.make

clean:
	@for dir in $(mod-subdirs) ; do $(MAKE) -C $$dir clean || exit 1 ; done

install:
	@mkdir -p $(MODULE_DEST)
	@for dir in $(mod-subdirs) ; do $(MAKE) -C $$dir install || exit 1 ; done
