CFLAGS= -I/LinuxPPC/usr/src/linux/include -Wall -Wstrict-prototypes        \
        -O2 -fomit-frame-pointer -fno-strict-aliasing -D__powerpc__ -fsigned-char      \
        -msoft-float -pipe -ffixed-r2 -Wno-uninitialized -mmultiple -mstring -mcpu=860 \
        -I../8xx_io -I/LinuxPPC/home/tmbinc/nokia/include -Wall

MFLAGS=-D__KERNEL__ -DMODULE

MODPATH=/LinuxPPC/lib/modules/2.4.0/misc
CROSS_COMPILE = ppc_8xx-
CC = $(CROSS_COMPILE)gcc
KD = $(CROSS_COMPILE)ld

MODULES = audio.o demux.o dmx.o dvbdev.o frontend.o sec.o video.o dvb.o dmxdev.o
TEST = test_audio test_av test_dmx test_front test_video

all: $(MODULES) $(TEST)

.c.o: 
	$(CC) $(CFLAGS) $(MFLAGS) -c $*.c -o $@

copy: $(MODULES)
	cp $(MODULES) $(MODPATH)