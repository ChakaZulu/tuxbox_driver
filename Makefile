KERNELRELEASE = \
	$(shell \
	for TAG in VERSION PATCHLEVEL SUBLEVEL EXTRAVERSION FLAVOUR ; do \
		eval `sed -ne "/^$$TAG/s/[   ]//gp" $(KERNEL_LOCATION)/Makefile` ; \
	done ; \
	echo $$VERSION.$$PATCHLEVEL.$$SUBLEVEL$$EXTRAVERSION$${FLAVOUR:+-$$FLAVOUR})

INSTALL_MOD_PATH =
MODULE_DEST := $(INSTALL_MOD_PATH)/lib/modules/$(KERNELRELEASE)/misc

export KERNELRELEASE KERNEL_LOCATION INSTALL_MOD_PATH MODULE_DEST

mod-subdirs := avia avs cam event fp i2c info lcd ost saa7126 ves

subdir-y := avia avs cam event fp i2c info lcd ost saa7126 ves

subdir-m := $(subdir-y)

include $(KERNEL_LOCATION)/Rules.make

clean:
	@for dir in $(mod-subdirs) ; do $(MAKE) -C $$dir clean || exit 1 ; done
#	find . \( -name '*.[oas]' -o -name core -o -name '.*.flags' \) -type f -print \
#		| xargs rm -f

install:
	@mkdir -p $(MODULE_DEST)
	@for dir in $(mod-subdirs) ; do $(MAKE) -C $$dir install || exit 1 ; done
