KERNELRELEASE 	:= \
	$(shell \
	for TAG in VERSION PATCHLEVEL SUBLEVEL EXTRAVERSION FLAVOUR ; do \
		eval `sed -ne "/^$$TAG/s/[   ]//gp" $(KERNEL_LOCATION)/Makefile` ; \
	done ; \
	echo $$VERSION.$$PATCHLEVEL.$$SUBLEVEL$$EXTRAVERSION$${FLAVOUR:+-$$FLAVOUR})

CONFIG_SHELL 	:= $(shell if [ -x "$$BASH" ]; then echo $$BASH; \
		else if [ -x /bin/bash ]; then echo /bin/bash; \
		else echo sh; fi ; fi)
TOPDIR		:= $(KERNEL_LOCATION)

HPATH		:= $(shell pwd)/include

CROSS_COMPILE =

AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CC) -E
AR		= $(CROSS_COMPILE)ar
NM		= $(CROSS_COMPILE)nm
STRIP		= $(CROSS_COMPILE)strip
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump
MODFLAGS	= -DMODULE

CPPFLAGS	:= -D__KERNEL__ -I$(shell pwd)/include -I$(shell pwd)/dvb/include -I$(KERNEL_LOCATION)/include -I$(KERNEL_LOCATION)/arch/ppc
CFLAGS		:= $(CPPFLAGS) -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fomit-frame-pointer
AFLAGS		:= -D__ASSEMBLY__ $(CPPFLAGS)

MODLIB		:= $(INSTALL_MOD_PATH)/lib/modules/$(KERNELRELEASE)

ifeq ($(HARDWARE),dbox2)
CONFIG_HARDWARE_DBOX2		:= m
else
ifeq ($(HARDWARE),dreambox)
CONFIG_HARDWARE_DREAMBOX	:= m
else
CONFIG_HARDWARE_DBOX2		:= m
endif
endif

export	VERSION PATCHLEVEL SUBLEVEL EXTRAVERSION KERNELRELEASE ARCH \
	CONFIG_SHELL TOPDIR HPATH CROSS_COMPILE AS LD CC \
	CPP AR NM \
	CPPFLAGS CFLAGS CFLAGS_KERNEL AFLAGS AFLAGS_KERNEL \
	MODLIB CONFIG_HARDWARE_DBOX2 CONFIG_HARDWARE_DREAMBOX

subdir-m			:= dvb info
subdir-$(CONFIG_HARDWARE_DBOX2)	+= avs cam event fp i2c lcd saa7126 wdt

ifeq (.depend,$(wildcard .depend))
include .depend
all: modules
else
all: depend modules
endif

.PHONY: modules
modules: $(patsubst %, _mod_%, $(SUBDIRS))

.PHONY: $(patsubst %, _mod_%, $(SUBDIRS))
$(patsubst %, _mod_%, $(SUBDIRS)):
	$(MAKE) -C $(patsubst _mod_%, %, $@) CFLAGS="$(CFLAGS) $(MODFLAGS)" MAKING_MODULES=1 modules

depend dep: $(KERNEL_LOCATION)/scripts/mkdep
	touch .depend
	$(KERNEL_LOCATION)/scripts/mkdep -- `find . \( -name CVS -o -name .svn \) -prune -o -follow -name \*.h -print` > .hdepend
	$(MAKE) $(patsubst %,_sfdep_%,$(SUBDIRS)) _FASTDEP_ALL_SUB_DIRS="$(SUBDIRS)" TOPDIR=$(KERNEL_LOCATION)

clean:
	find . \( -name '*.[oas]' -o -name '.*.flags' \) -type f -print | xargs rm -f

mrproper: clean
	find . \( -size 0 -o -name .depend \) -type f -print | xargs rm -f

distclean: mrproper
	rm -f `find . \( -not -type d \) -and \
		\( -name '*.orig' -o -name '*.rej' -o -name '*~' \
		-o -name '*.bak' -o -name '#*#' -o -name '.*.orig' \
		-o -name '.*.rej' -o -name '.SUMS' -o -size 0 \) -type f -print`

.PHONY: install
install: modules_install_misc

include Rules.make
