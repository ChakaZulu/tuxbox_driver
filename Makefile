# currently running kernel
CURRENT=$(shell uname -r)

# where the kernel sources are located
#KERNEL_LOCATION=/usr/src/kernel/2.0.35
#KERNEL_LOCATION=/usr/src/kernel/$(CURRENT)
#KERNEL_LOCATION=/usr/src/kernel/vger
KERNEL_LOCATION=/LinuxPPC/usr/src/linux

MODULE_DEST= /LinuxPPC/lib/modules/2.4.1/misc/

export CURRENT KERNEL_LOCATION MODULE_DEST

#################################################

mod-subdirs := nokia cxa2092 cxa2126 lcd saa7126 pcm

subdir-y := nokia cxa2092 cxa2126 lcd saa7126 pcm

subdir-m :=	$(subdir-y)

include $(KERNEL_LOCATION)/Rules.make

clean:
	find . \( -name '*.[oas]' -o -name core -o -name '.*.flags' \) -type f -print \
		| xargs rm -f
