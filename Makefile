KERNELRELEASE = \
	$(shell \
	for TAG in VERSION PATCHLEVEL SUBLEVEL EXTRAVERSION FLAVOUR ; do \
		eval `sed -ne "/^$$TAG/s/[   ]//gp" $(KERNEL_LOCATION)/Makefile` ; \
	done ; \
	echo $$VERSION.$$PATCHLEVEL.$$SUBLEVEL$$EXTRAVERSION$${FLAVOUR:+-$$FLAVOUR})

INSTALL_MOD_PATH =
MODULE_DEST := $(INSTALL_MOD_PATH)/lib/modules/$(KERNELRELEASE)/misc
BIN_DEST := $(INSTALL_MOD_PATH)/bin

export KERNELRELEASE KERNEL_LOCATION INSTALL_MOD_PATH MODULE_DEST BIN_DEST

mod-subdirs := avs lcd saa7126 pcm avia fp i2c ves cam ost test info core event

subdir-y := avs lcd saa7126 pcm avia fp i2c ves cam ost test info core event

subdir-m := $(subdir-y)

include $(KERNEL_LOCATION)/Rules.make

clean:
	@for dir in $(mod-subdirs) ; do $(MAKE) -C $$dir clean || exit 1 ; done
#	find . \( -name '*.[oas]' -o -name core -o -name '.*.flags' \) -type f -print \
#		| xargs rm -f

install:
	@mkdir -p $(MODULE_DEST)
	@for dir in $(mod-subdirs) ; do $(MAKE) -C $$dir install || exit 1 ; done
