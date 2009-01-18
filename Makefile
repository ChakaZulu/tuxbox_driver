DRIVER_TOPDIR = .

include Config.make

subdir-m			:= dvb info
subdir-$(CONFIG_HARDWARE_DBOX2)	+= avs cam event ext fp i2c lcd saa7126 dvb2eth ds1307
subdir-$(CONFIG_IDE) 		+= ide
subdir-$(CONFIG_DBOX2_MMC)	+= mmc

# catch if IDE is built-in
ifeq ($(CONFIG_IDE),y)
subdir-m			+= ide
endif
ifeq ($(CONFIG_DBOX2_MMC),y)
subdir-m			+= mmc
endif

include Rules.make
