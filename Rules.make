include $(KERNEL_LOCATION)/Rules.make

.PHONY: _modinst__misc_
_modinst__misc_: dummy
ifneq "$(strip $(ALL_MOBJS))" ""
	mkdir -p $(MODLIB)/misc
	cp $(sort $(ALL_MOBJS)) $(MODLIB)/misc
endif

.PHONY: modules_install_misc
modules_install_misc: _modinst__misc_ $(patsubst %,_modinst_%,$(MOD_DIRS))

