include $(KERNEL_LOCATION)/.config
include $(KERNEL_LOCATION)/Rules.make

.PHONY: modules_install_misc
modules_install_misc: _modinst_misc_ $(patsubst %,_modinst_misc_%,$(MOD_DIRS))

.PHONY: _modinst_misc_
_modinst_misc_: dummy
ifneq "$(strip $(ALL_MOBJS))" ""
	mkdir -p $(MODLIB)/misc
	cp $(sort $(ALL_MOBJS)) $(MODLIB)/misc
endif

ifneq "$(strip $(MOD_DIRS))" ""
.PHONY: $(patsubst %,_modinst_misc_%,$(MOD_DIRS))
$(patsubst %,_modinst_misc_%,$(MOD_DIRS)) : dummy
	$(MAKE) -C $(patsubst _modinst_misc_%,%,$@) modules_install_misc
endif

