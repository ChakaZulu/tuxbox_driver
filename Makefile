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

CPPFLAGS := -D__KERNEL__ -I$(shell pwd)/include -I$(shell pwd)/dvb/include -I$(KERNEL_LOCATION)/include -I$(KERNEL_LOCATION)/arch/ppc
CFLAGS := $(CPPFLAGS) -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fomit-frame-pointer
AFLAGS := -D__ASSEMBLY__ $(CPPFLAGS)

HPATH = $(shell pwd)/include

KERNELRELEASE := \
	$(shell \
	for TAG in VERSION PATCHLEVEL SUBLEVEL EXTRAVERSION FLAVOUR ; do \
		eval `sed -ne "/^$$TAG/s/[   ]//gp" $(KERNEL_LOCATION)/Makefile` ; \
	done ; \
	echo $$VERSION.$$PATCHLEVEL.$$SUBLEVEL$$EXTRAVERSION$${FLAVOUR:+-$$FLAVOUR})

TOPDIR := $(KERNEL_LOCATION)

MODLIB := $(INSTALL_MOD_PATH)/lib/modules/$(KERNELRELEASE)

CONFIG_MODULES	:= y

export AS LD CC CPP AR NM CFLAGS AFLAGS HPATH KERNELRELEASE TOPDIR MODLIB CONFIG_MODULES

SUBDIRS		:= avs cam dvb event fp i2c info lcd saa7126 wdt

all: do-it-all

ifeq (.depend,$(wildcard .depend))
include .depend
do-it-all: modules
else
do-it-all: depend
endif

.PHONY: modules
modules: $(patsubst %, _mod_%, $(SUBDIRS))

.PHONY: $(patsubst %, _mod_%, $(SUBDIRS))
$(patsubst %, _mod_%, $(SUBDIRS)):
	$(MAKE) -C $(patsubst _mod_%, %, $@) CFLAGS="$(CFLAGS) $(MODFLAGS)" MAKING_MODULES=1 modules

depend dep: dep-files

dep-files: $(KERNEL_LOCATION)/scripts/mkdep
	touch .depend
	$(KERNEL_LOCATION)/scripts/mkdep -- `find . \( -name CVS -o -name .svn \) -prune -o -follow -name \*.h -print` > .hdepend
	$(MAKE) $(patsubst %,_sfdep_%,$(SUBDIRS)) _FASTDEP_ALL_SUB_DIRS="$(SUBDIRS)" TOPDIR=$(KERNEL_LOCATION)

clean:
	find . \( -name '*.[oas]' -o -name '.*.flags' \) -type f -print | xargs rm -f

mrproper: clean
	find . \( -size 0 -o -name .depend \) -type f -print | xargs rm -f

distclean: mrproper
	rm -f core `find . \( -not -type d \) -and \
		\( -name '*.orig' -o -name '*.rej' -o -name '*~' \
		-o -name '*.bak' -o -name '#*#' -o -name '.*.orig' \
		-o -name '.*.rej' -o -name '.SUMS' -o -size 0 \) -type f -print`

.PHONY: modules_install
modules_install: _modinst_ $(patsubst %, _modinst_%, $(SUBDIRS))

.PHONY: _modinst_
_modinst_:
	@mkdir -p $(MODLIB)/misc

.PHONY: $(patsubst %, _modinst_%, $(SUBDIRS))
$(patsubst %, _modinst_%, $(SUBDIRS)):
	$(MAKE) -C $(patsubst _modinst_%, %, $@) modules_install_misc

include Rules.make
