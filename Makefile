KERNELRELEASE = \
	$(shell \
	for TAG in VERSION PATCHLEVEL SUBLEVEL EXTRAVERSION FLAVOUR ; do \
		eval `sed -ne "/^$$TAG/s/[   ]//gp" $(KERNEL_LOCATION)/Makefile` ; \
	done ; \
	echo $$VERSION.$$PATCHLEVEL.$$SUBLEVEL$$EXTRAVERSION$${FLAVOUR:+-$$FLAVOUR})

INSTALL_MOD_PATH =
MODULE_DEST := $(INSTALL_MOD_PATH)/lib/modules/$(KERNELRELEASE)/misc

export KERNELRELEASE KERNEL_LOCATION INSTALL_MOD_PATH MODULE_DEST

mod-subdirs := avs cam dvb event fp i2c info lcd saa7126 wdt

subdir-y := $(mod-subdirs)

subdir-m := $(subdir-y)

include $(KERNEL_LOCATION)/Rules.make

clean:
	@for dir in $(mod-subdirs) ; do $(MAKE) -C $$dir clean || exit 1 ; done

install:
	@mkdir -p $(MODULE_DEST)
	@for dir in $(mod-subdirs) ; do $(MAKE) -C $$dir install || exit 1 ; done
