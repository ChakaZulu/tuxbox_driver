/*
 *   avia.c - AViA x00 driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2001 Felix "tmbinc" Domke (tmbinc@gmx.net)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *   $Log: avia_core.c,v $
 *   Revision 1.3  2001/02/17 11:12:42  gillem
 *   - fix init
 *
 *   Revision 1.2  2001/02/16 20:48:29  gillem
 *   - some avia600 tests
 *
 *   Revision 1.1  2001/02/15 21:55:56  gillem
 *   - change module name to avia.o
 *   - add interrupt for commands
 *   - some rewrites
 *
 *   Revision 1.14  2001/02/15 00:11:20  gillem
 *   - rewrite stuff ... not ready (read source to understand new params)
 *
 *   Revision 1.13  2001/02/13 23:46:30  gillem
 *   -fix the interrupt problem (ppc i like you)
 *
 *   Revision 1.12  2001/02/03 16:39:17  tmbinc
 *   sound fixes
 *
 *   Revision 1.11  2001/02/03 14:48:16  gillem
 *   - more audio fixes :-/
 *
 *   Revision 1.10  2001/02/03 11:29:54  gillem
 *   - fix audio
 *
 *   Revision 1.9  2001/02/02 18:17:18  gillem
 *   - add exports (avia_wait,avia_command)
 *
 *   Revision 1.8  2001/01/31 17:17:46  tmbinc
 *   Cleaned up avia drivers. - tmb
 *
 *   $Revision: 1.3 $
 *
 */

#define __KERNEL_SYSCALLS__

#include <linux/string.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <linux/fcntl.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/malloc.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>

#include "dbox/fp.h"
#include "dbox/avia.h"

/* ---------------------------------------------------------------------- */

#ifdef MODULE
static int debug=0;
static int pal=1;
static char *firmware=0;
#endif

#define dprintk(fmt,args...) if(debug) printk( "AVIA: " fmt,## args)

#ifdef MODULE

void avia_set_pcr(u32 hi, u32 lo);

volatile u8 *aviamem;
int aviarev;

static int dev;
static int run_cmd;

/* interrupt stuff */
#define AVIA_INTERRUPT			SIU_IRQ4
#define AVIA_CMD_TIMEOUT		500

static wait_queue_head_t avia_cmd_wait;

/* finally i got them */
#define UX_MAGIC                        0x00
#define UX_NUM_SECTIONS                 0x01
#define UX_LENGTH_FILE                  0x02
#define UX_FIRST_SECTION_START          0x03
#define UX_SECTION_LENGTH_OFFSET        0x01
#define UX_SECTION_WRITE_ADDR_OFFSET    0x02
#define UX_SECTION_CHECKSUM_OFFSET      0x03
#define UX_SECTION_DATA_OFFSET          0x04
#define UX_SECTION_DATA_GBUS_TABLE      0x02FF
#define UX_SECTION_DATA_IMEM            0x0200
#define UX_SECTION_HEADER_LENGTH        0x04

#define NTSC_16MB_WO_ROM_SRAM   		7
#define NTSC_16MB_PL_ROM_SRAM   		9
#define PAL_16MB_WO_ROM_SRAM    		10
#define PAL_20MB_WO_ROM_SRAM    		12

/* ---------------------------------------------------------------------- */

u32 avia_rd(int mode, int address)
{
	int data=0;

	address&=0x3FFFFF;
	aviamem[6]=((address>>16)|mode)&0xFF;
	aviamem[5]=(address>>8)&0xFF;
	aviamem[4]=address&0xFF;
	data=aviamem[3]<<24;
	data|=aviamem[2]<<16;
	data|=aviamem[1]<<8;
	data|=aviamem[0];

	return data;
}

/* ---------------------------------------------------------------------- */

void avia_wr(int mode, u32 address, u32 data)
{
	address&=0x3FFFFF;
	aviamem[6]=((address>>16)|mode)&0xFF;
	aviamem[5]=(address>>8)&0xFF;
	aviamem[4]=address&0xFF;
	aviamem[3]=(data>>24)&0xFF;
	aviamem[2]=(data>>16)&0xFF;
	aviamem[1]=(data>>8)&0xFF;
	aviamem[0]=data&0xFF;
}

/* ---------------------------------------------------------------------- */

inline void wIM(u32 addr, u32 data)
{
	wGB(0x36, addr);
	wGB(0x34, data);
}

/* ---------------------------------------------------------------------- */

inline u32 rIM(u32 addr)
{
	wGB(0x3A, 0x0B);
	wGB(0x3B, addr);
	wGB(0x3A, 0x0E);

	return rGB(0x3B);
}

/* ---------------------------------------------------------------------- */

inline unsigned long endian_swap(unsigned long v)
{
	return ((v&0xFF)<<24)|((v&0xFF00)<<8)|((v&0xFF0000)>>8)|((v&0xFF000000)>>24);
}

/* ---------------------------------------------------------------------- */

static void InitialGBus(u32 *microcode)
{
	unsigned long *ptr=((unsigned long*)microcode)+0x306;
	int words=*ptr--, data, addr;

	dprintk(KERN_DEBUG "performing %d initial G-bus Writes. (don't panic! ;)\n", words);

	while (words--)
	{
		addr=*ptr--;
		data=*ptr--;
		wGB(addr, data);
	}
}

/* ---------------------------------------------------------------------- */

static void FinalGBus(u32 *microcode)
{
	unsigned long *ptr=((unsigned long*)microcode)+0x306;
	int words=*ptr--, data, addr;

	*ptr-=words;
	ptr-=words*4;
	words=*ptr--;

	dprintk(KERN_DEBUG "performing %d final G-bus Writes.\n", words);

	while (words--)
	{
		addr=*ptr--;
		data=*ptr--;
		wGB(addr, data);
	}
}

/* ---------------------------------------------------------------------- */

static void dram_memcpyw(u32 dst, u32 *src, int words)
{
	while (words--)
	{
		wDR(dst, *src++);
		dst+=4;
	}
}

/* ---------------------------------------------------------------------- */

static void load_dram_image(u32 *microcode,u32 section_start)
{
	u32 dst, *src, words, errors=0;

	words=endian_swap(microcode[section_start+UX_SECTION_LENGTH_OFFSET])/4;
	dst=endian_swap(microcode[section_start+UX_SECTION_WRITE_ADDR_OFFSET]);
	src=microcode+section_start+UX_SECTION_DATA_OFFSET;

	dram_memcpyw(dst, src, words);

	while (words--)
	{
		if (rDR(dst)!=*src++)
		{
			errors++;
		}

		dst+=4;
	}

	if (errors)
	{
		dprintk(KERN_ERR "ERROR: microcode verify: %d errors.\n", errors);
	}
}

/* ---------------------------------------------------------------------- */

static void load_imem_image(u32 *microcode,u32 data_start)
{
	u32 *src, i, words, errors=0;
	src=microcode+data_start+UX_SECTION_DATA_IMEM;
	words=256;

	for (i=0; i<words; i++)
	{
		wIM(i, src[i]);
	}

	wGB(0x3A, 0xB);               // CPU_RMADR2
	wGB(0x3B, 0);                 // set starting address
	wGB(0x3A, 0xE);               // indirect regs

	for (i=0; i<words; i++)
	{
		if (rGB(0x3B)!=src[i])
		{
			errors++;
		}
	}

	if (errors)
	{
		dprintk(KERN_ERR "ERROR: imem verify: %d errors\n", errors);
	}
} 

/* ---------------------------------------------------------------------- */

static void LoaduCode(u32 *microcode)
{
	int num_sections=endian_swap(microcode[1]);
	int data_offset=3;
  
	while (num_sections--)
	{
		load_dram_image(microcode,data_offset);
		data_offset+=(endian_swap(microcode[data_offset+UX_SECTION_LENGTH_OFFSET])/4)+UX_SECTION_HEADER_LENGTH;
	}
}

/* ---------------------------------------------------------------------- */

void avia_interrupt(int irq, void *vdev, struct pt_regs * regs)
{
	u32 status=rDR(0x2AC);
	u32 sem;

	/* avia cmd stuff */

	if(status&(1<<15) || status&(1<<9))
	{
		dprintk("CMD INTR\n");

		if(run_cmd)
		{
			run_cmd--;
			wake_up_interruptible( &avia_cmd_wait );
		}
		else
		{
			dprintk("CMD UNKN %08X %08X %08X %08X %08X %08X %08X %08X\n",\
				rDR(0x40),rDR(0x44),rDR(0x48),rDR(0x4c),rDR(0x50),rDR(0x54),rDR(0x58),rDR(0x5c));
			dprintk("PROC %08X %08X %08X\n",rDR(0x2a0),rDR(0x2a4),rDR(0x2a8));
		}
	}

	/* INIT INTR */
	if(status&(1<<23))
	{
//		dprintk("INIT RECV !\n");
	}

	/* AUD INTR */
	if(status&(1<<22))
	{
 	    sem = rDR(0x460);

		dprintk("AUD INTR %08X\n",sem);

		// E0 AUDIO_CONFIG
		// E8 AUDIO_DAC_MODE
		// EC AUDIO_CLOCK_SELECTION
		// F0 I"C_958_DELAY

		/* new sample freq. */
		if ( sem&7 )
		{
			switch(sem&7)
			{
				// 44.1
				case 1: wDR(0xEC, (rDR(0xEC)&~(7<<2)) | (1<<2) ); break;
				// 48
				case 2: wDR(0xEC, (rDR(0xEC)&~(7<<2)) ); break;
				// 32
				case 7: wDR(0xEC, (rDR(0xEC)&~(7<<2)) | (2<<2) ); break;
			}

			dprintk("New sample freq. %d.\n",sem&7);
		}

		/* reserved */
		if ( sem&(7<<3) )
		{
			dprintk("Reserved %02X.\n",(sem>>3)&7);
		}

		/* new audio emphasis */
		if ( sem&(3<<6) )
		{
			switch((sem>>6)&3)
			{
				case 1: dprintk("New audio emphasis is off.\n"); break;
				case 2: dprintk("New audio emphasis is on.\n"); break;
			}

		}

		wDR( 0x468, 1 );
	}

	/* buffer full */
	if ( status&(1<<16) ) {
//		printk("BUF-F INTR\n");

		if ( rDR(0x2b4)&2 ) {
//			printk("BUF-F VIDEO\n");
		}

		if ( rDR(0x2b4)&2 ) {
//			printk("BUF-F AUDIO\n");
		}

	}

	/* buffer und. */
	if ( status&(1<<8) ) {
//		printk("UND INTR\n");

		if ( rDR(0x2b8)&1 ) {
//			printk("UND VIDEO\n");
		}

		if ( rDR(0x2b8)&2 ) {
//			printk("UND AUDIO\n");
		}
	}


	/* bitstream error */
	if ( status&1 ) {
//		printk("ERR INTR\n");

		if ( rDR(0x2c4)&(1<<1) ) {
//			printk("ERR SYSTEM BITSTREAM CURR: %d\n",rDR(0x318));
		}

		if ( rDR(0x2c4)&(1<<2) ) {
			dprintk("ERR AUDIO BITSTREAM CURR: %d\n",rDR(0x320));
		}

		if ( rDR(0x2c4)&(1<<3) ) {
//			printk("ERR VIDEO BITSTREAM CURR: %d\n",rDR(0x31C));
		}
	}

    /* intr. ack */
	wGB(0,  ((rGB(0) & (~1))|2));

	/* clear flags */
	wDR(0x2B4, 0);
	wDR(0x2B8, 0);
	wDR(0x2C4, 0);
	wDR(0x2AC, 0);

	return;
}

/* ---------------------------------------------------------------------- */

u32 avia_command(u32 command, ...)
{
	u32 stataddr, tries, i;
	va_list ap;
	va_start(ap, command);

	// TODO: kernel lock (DRINGEND)

	tries=100;

	while (!rDR(0x5C))
	{
		udelay(10000);
		if (! (tries--))
		{
			dprintk("AVIA timeout.\n");
			return -1;
		}
	}
  
	wDR(0x40, command);
  
	for (i=0; i<((command&0x7F00)>>8); i++)
	{
		wDR(0x44+i*4, va_arg(ap, int));
	}

	va_end(ap);

	for (; i<8; i++)
	{
		wDR(0x44+i*4, 0);
	}

	wDR(0x5C, 0);

    // TODO: host-to-decoder interrupt

	tries=100;

	while (!(stataddr=rDR(0x5C)))
	{
		udelay(1000);

		if (! (tries--))
		{
			dprintk("AVIA timeout.\n");
			return -1;
		}
	}

	return stataddr;
}

/* ---------------------------------------------------------------------- */

u32 avia_wait(u32 sa)
{
	int i;
	int tries=2000;

	if (sa==-1)
	{
		return -1;
	}

	dprintk("COMMAND run\n");
	run_cmd++;
	i=interruptible_sleep_on_timeout(&avia_cmd_wait, AVIA_CMD_TIMEOUT);
	dprintk("COMMAND complete: %d.\n",AVIA_CMD_TIMEOUT-i);

	if (i==0)
	{
		dprintk("COMMAND timeout.\n");
		return -1;
	}

	return(rDR(sa));
}

/* ---------------------------------------------------------------------- */

void avia_set_pcr(u32 hi, u32 lo)
{
	u32 data1 = (hi>>16)&0xFFFF;
	u32 data2 = hi&0xFFFF;
	u32 timer_high = ((1<<21))|((data1 & 0xE000L) << 4)
               			| (( 1 << 16)) | ((data1 & 0x1FFFL) << 3)
               			| ((data2 & 0xC000L) >> 13) | ((1L));

	u32 timer_low = ((data2 & 0x03FFFL) << 2)
               			| ((lo & 0x8000L) >> 14) | (( 1L ));

	dprintk("setting avia PCR: %08x:%08x\n", hi, lo);

	wGB(0x02, timer_high);
	wGB(0x03, timer_low);
}

/* ---------------------------------------------------------------------- */

static int wait_audio_sequence(void)
{
	while(rDR(AUDIO_SEQUENCE_ID));
	return 0;
}

static int init_audio_sequence(void)
{
	wDR(AUDIO_SEQUENCE_ID, 0);
	wDR(NEW_AUDIO_SEQUENCE, 1);
	return wait_audio_sequence();
}

static int new_audio_sequence( u32 val )
{
	wDR(AUDIO_SEQUENCE_ID, val);
	wDR(NEW_AUDIO_SEQUENCE, 2);
	return wait_audio_sequence();
}

/* ---------------------------------------------------------------------- */

static void avia_audio_init(void)
{
	u32 val;

	/* AUDIO_CONFIG
	 *
	 * 12,11,7,6,5,4 reserved or must be set to 0
     */
	val  = 0;
	val |= (1<<10);		// 1: 64 0: 32/48
	val |= (0<<9);		// 1: I2S 0: other
	val |= (1<<8);		// 1: no clock on da-iec
	val |= (0<<3);		// 0: normal 1:I2S output
	val |= (1<<2);		// 0:off 1:on channels
	val |= (0<<1);		// 0:off 1:on IEC-958
	val |= (0);			// 0:encoded 1:decoded output
	wDR(AUDIO_CONFIG, val);

	/* AUDIO_DAC_MODE
	 * 0 reserved
     */
	val  = 0;
	val |= (0<<8);		//			
	val |= (3<<6);		//
	val |= (0<<4);		//
	val |= (0<<3);		// 0:high 1:low DA-LRCK polarity
	val |= (1<<2);		// 0:0 as MSB in 24 bit mode 1: sign ext. in 24bit
	val |= (0<<1);		// 0:msb 1:lsb first
	wDR(AUDIO_DAC_MODE, val);

	/* AUDIO_CLOCK_SELECTION */
	val = 0;
	val |= (0<<2);
	val |= (1<<1);		// 1:256 0:384 x sampling frequ.
	val |= (1);			// master,slave mode
	wDR(AUDIO_CLOCK_SELECTION, val);

	/* AUDIO_ATTENUATION */
	wDR(AUDIO_ATTENUATION, 0);

	wDR(0xEC, 7);                 // AUDIO CLOCK SELECTION (nokia 3) (br 7)
	wDR(0xE8, 2);                 // AUDIO_DAC_MODE
//	wDR(0xE0, 0x2F);              // nokia: 0xF, br: 0x2D
	wDR(0xE0, 0x70F);
}

/* ---------------------------------------------------------------------- */

void avia_set_default(void)
{
	u32 val;

	val  = 0;
	val |= (0<<2);	// 1: tristate
	val |= (0<<0);	// 0: slave 1: master HSYNC/VSYNC
	val |= (0<<0);	// 0: BT.601 1: BT.656

	wDR(VIDEO_MODE, val);

	/* 0: 4:3 1: 16:9 downs. 2: 16:9 */
	wDR(DISPLAY_ASPECT_RATIO, 0);

	/* 0: disable 1: PAN&SCAN 2: Letterbox */
	wDR(ASPECT_RATIO_MODE, 1);

	/* 2: 4:3 3: 16:9 4: 20:9 */
	wDR(FORCE_CODED_ASPECT_RATIO, 0);

	wDR(PAN_SCAN_SOURCE, 0);

	wDR(PAN_SCAN_HORIZONTAL_OFFSET, 0);

	val = 0x108080;
	wDR(BORDER_COLOR, val);
	wDR(BACKGROUND_COLOR, val);

	/* 0: I_FRAME 1: I_SLICE */
	wDR(I_SLICE, 0);

	/* 0: frame 1: slice based error recovery */
	wDR(ERR_CONCEALMENT_LEVEL, 0);


	/* PLAY MODE PARAMETER */


	/* 0: Demux interface 2: Host Interface */
	wDR(BITSTREAM_SOURCE, 0);

	/* */
	wDR(TM_MODE, 0x0a);

	wDR(AV_SYNC_MODE, 0x06);

	wDR(VIDEO_PTS_DELAY, 0);
	wDR(VIDEO_PTS_SKIP_THRESHOLD, 0xE10);
	wDR(VIDEO_PTS_REPEAT_THRESHOLD, 0xE10);

    wDR(AUDIO_PTS_DELAY, 0xe00);
	wDR(AUDIO_PTS_SKIP_THRESHOLD_1, 0xE10 );
	wDR(AUDIO_PTS_REPEAT_THRESHOLD_1, 0xE10 );
	wDR(AUDIO_PTS_SKIP_THRESHOLD_2, 0x2300 );
	wDR(AUDIO_PTS_REPEAT_THRESHOLD_2, 0x2300 );

	/* */
    wDR(INTERPRET_USER_DATA,0);

	/* osd */
	wDR(DISABLE_OSD, 0 );
	wDR(OSD_BUFFER_START, 0 );
	wDR(OSD_BUFFER_END, 0xFFFF );

	wDR(0x64, 0);
	wDR(DRAM_INFO, 0);
	wDR(UCODE_MEMORY, 0);

	/* set pal or ntsc */
	if (pal)
	{
		wDR(MEMORY_MAP, PAL_16MB_WO_ROM_SRAM);
	} else
	{
		wDR(MEMORY_MAP, NTSC_16MB_WO_ROM_SRAM);
	}
}

/* ---------------------------------------------------------------------- */

static int ppc_set_siumcr(void)
{
	immap_t *immap;
	sysconf8xx_t * sys_conf;

	immap = (immap_t *)IMAP_ADDR;

	if (!immap)
	{
		dprintk("Get immap failed.\n");
		return -1;
	}

	sys_conf = (sysconf8xx_t *)&immap->im_siu_conf;

	if (!sys_conf)
	{
		dprintk("Get sys_conf failed.\n");
		return -1;
	}

	if ( sys_conf->sc_siumcr & (3<<10) )
	{
		cli();
		sys_conf->sc_siumcr &= ~(3<<10);
		sti();
	}

	return 0;
}

/* ---------------------------------------------------------------------- */

static int errno;

static int do_firmread(const char *fn, char **fp)
{
	/* shameless stolen from sound_firmware.c */

	int fd;
    long l;
    char *dp;

	fd = open(fn,0,0);

	if (fd == -1)
	{
		dprintk("Unable to load '%s'.\n", firmware);
		return 0;
	}

	l = lseek(fd, 0L, 2);

	if (l<=0)
	{
		dprintk("Firmware wrong size '%s'.\n", firmware);
		sys_close(fd);
		return 0;
	}

	lseek(fd, 0L, 0);

	dp = vmalloc(l);

	if (dp == NULL)
	{
		dprintk("Out of memory loading '%s'.\n", firmware);
		sys_close(fd);
		return 0;
	}

	if (read(fd, dp, l) != l)
	{
		dprintk("Failed to read '%s'.%d\n", firmware,errno);
		vfree(dp);
		sys_close(fd);
		return 0;
	}

	close(fd);

	*fp = dp;

	return (int) l;
}

/* ---------------------------------------------------------------------- */

static int init_avia(void)
{
	u32 *microcode;
	u32 val;
	int tries;
   	mm_segment_t fs;

	run_cmd = 0;

	fs = get_fs();

	set_fs(get_ds());

	/* read firmware */
	if (do_firmread(firmware, (char**)&microcode) == 0)
	{
		set_fs(fs);
		return -EIO;
	}

	set_fs(fs);

	/* remap avia memory */
	aviamem=(unsigned char*)ioremap(0xA000000, 0x200);

	if (!aviamem)
	{
		dprintk("Failed to remap memory.\n");
		vfree(microcode);
		return -ENOMEM;
	}

	(void)aviamem[0];

	/* set siumcr for interrupt */
	if ( ppc_set_siumcr() < 0 )
	{
		vfree(microcode);
		iounmap((void*)aviamem);
		return -EIO;
	}

	/* request avia interrupt */
	if (request_8xxirq(AVIA_INTERRUPT, avia_interrupt, 0, "avia", &dev) != 0)
	{
		dprintk("Failed to get interrupt.\n");
		vfree(microcode);
		iounmap((void*)aviamem);
		return -EIO;
	}

	/* init queue */
	init_waitqueue_head(&avia_cmd_wait);

	/* enable host access */
	wGB(0, 0x1000);
	/* cpu reset */
	wGB(0x39, 0xF00000);
	/* cpu reset by fp */
	fp_do_reset(0xBF & ~ (2));
	/* enable host access */
	wGB(0, 0x1000);
	/* cpu reset */
	wGB(0x39, 0xF00000);

	wGB(0, 0x1000);

	aviarev=(rGB(0)>>16)&3;

	switch (aviarev)
	{
		case 0:
				dprintk("AVIA 600 found. (no support yet)\n");
				break;
		case 1:
    			dprintk("AVIA 500 LB3 found. (no microcode)\n");
				break;
//		case 3:
//				dprintk("AVIA 600L found. (no support yet)\n");
//				break;
  		default:
    			dprintk("AVIA 500 LB4 found. (nice)\n");
    			break;
	}

	dprintk("SILICON REVISION: %02X\n",((rGB(0x22)>>8)&0xFF));

	/* TODO: AVIA 600 INIT !!! */

    /* D.4.3 - Initialize the SDRAM DMA Controller */
	switch (aviarev)
	{
		case 0:
//		case 3:
			wGB(0x22, 0x10000);
			wGB(0x23, 0x5FBE);
			wGB(0x22, 0x12);
			wGB(0x23, 0x3a1800);
			break;
		default:
		    wGB(0x22, 0xF);
			val = rGB(0x23) | 0x14EC;
		    wGB(0x22, 0xF);
		    wGB(0x23, val);
			rGB(0x23);

		    wGB(0x22, 0x11);
			val = (rGB(0x23) & 0x00FFFFFF) | 1;
		    wGB(0x22, 0x11);
		    wGB(0x23, val);
			rGB(0x23);
			break;
	}

	InitialGBus(microcode);

	LoaduCode(microcode);

	load_imem_image(microcode,UX_FIRST_SECTION_START+UX_SECTION_DATA_OFFSET);

	avia_set_default();

	avia_audio_init();

//	init_audio_sequence();

	FinalGBus(microcode);

	vfree(microcode);

	/* set cpu running mode */
	wGB(0x39, 0x900000);

	/* enable decoder/host interrupt */
	wGB(0, rGB(0)|(1<<7));
	wGB(0, rGB(0)&~(1<<6));
	wGB(0, rGB(0)|(1<<1));
	wGB(0, rGB(0)&~1);

	/* clear int. flags */
	wDR(0x2B4, 0);
	wDR(0x2B8, 0);
	wDR(0x2C4, 0);
	wDR(0x2AC, 0);

	/* enable interrupts */
	wDR(0x200, (1<<23)|(1<<22)|(1<<16)|(1<<8)|(1) );

	tries=20;

	while ((rDR(0x2A0)!=0x2))
	{
		if (!--tries)
			break;
		udelay(10*1000);
		schedule();
	}

	if (!tries)
	{
		dprintk("Timeout waiting for decoder initcomplete. (%08X)\n",rDR(0x2A0));
		return -EIO;
	}

	/* new audio config */
	wDR(0x468, 0xFFFF);

	tries=20;

	while (rDR(0x468))
	{
		if (!--tries)
			break;
		udelay(10*1000);
		schedule();
	}

	if (!tries)
	{
		dprintk("New_audio_config timeout\n");
		return -EIO;
	}

	/* TODO: better handling */
	if (avia_wait(avia_command(SetStreamType, 0xB))==-1)
	{
		return 0;
	}

	avia_wait(avia_command(SelectStream, 0, 0xFF));
//	avia_wait(avia_command(SelectStream, 2, 0x100));
//	avia_wait(avia_command(SelectStream, 3, 0x100));
	avia_command(Play, 0, 0, 0);

	dprintk("Using avia firmware revision %c%c%c%c\n", rDR(0x330)>>24, rDR(0x330)>>16, rDR(0x330)>>8, rDR(0x330));
	dprintk("%x %x %x %x %x\n", rDR(0x2C8), rDR(0x2CC), rDR(0x2B4), rDR(0x2B8), rDR(0x2C4));

	return 0;
}

/* ---------------------------------------------------------------------- */

EXPORT_SYMBOL(avia_wr);
EXPORT_SYMBOL(avia_rd);
EXPORT_SYMBOL(avia_wait);
EXPORT_SYMBOL(avia_command);
EXPORT_SYMBOL(avia_set_pcr);

/* ---------------------------------------------------------------------- */

MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("Avia x00 driver");
MODULE_PARM(debug,"i");
MODULE_PARM(pal,"i");
MODULE_PARM(firmware,"s");

int init_module(void)
{
	return init_avia();
}

void cleanup_module(void)
{
	free_irq(AVIA_INTERRUPT, &dev);

	if (aviamem)
	{
		iounmap((void*)aviamem);
	}
}
#endif
