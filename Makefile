# currently running kernel
CURRENT=2.4.2

LINUXPPC=/LinuxPPC

# where the kernel sources are located
KERNEL_LOCATION=$(LINUXPPC)/usr/src/linux

#where to put the binaries...
INSTALL_PATH=$(LINUXPPC)/rootfs/boot
INSTALL_MOD_PATH=$(LINUXPPC)/rootfs
MODULE_DEST=$(INSTALL_MOD_PATH)/lib/modules/$(CURRENT)/misc
BIN_DEST=$(LINUXPPC)/rootfs/usr/local/bin

export CURRENT KERNEL_LOCATION INSTALL_MOD_PATH MODULE_DEST BIN_DEST


mod-subdirs := nokia avs lcd saa7126 pcm

subdir-y := nokia avs lcd saa7126 pcm

subdir-m := $(subdir-y)

include $(KERNEL_LOCATION)/Rules.make

clean:
	@for dir in $(mod-subdirs) ; do $(MAKE) -C $$dir clean || exit 1 ; done
#	find . \( -name '*.[oas]' -o -name core -o -name '.*.flags' \) -type f -print \
#		| xargs rm -f

install:
	@mkdir -p $(MODULE_DEST)
	@for dir in $(mod-subdirs) ; do $(MAKE) -C $$dir install || exit 1 ; done
