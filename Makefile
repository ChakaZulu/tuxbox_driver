include Config.make

subdir-m			:= dvb info
subdir-$(CONFIG_HARDWARE_DBOX2)	+= avs cam event fp i2c lcd saa7126 wdt

include Rules.make
