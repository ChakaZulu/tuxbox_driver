/*
 * $Id: avia_av_core.c,v 1.61 2003/04/17 07:29:47 obi Exp $
 * 
 * AViA 500/600 core driver (dbox-II-project)
 *
 * Homepage: http://dbox2.elxsi.de
 *
 * Copyright (C) 2000-2001 Felix "tmbinc" Domke (tmbinc@gmx.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include <linux/slab.h>
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

#include <dbox/fp.h>
#include "avia_av.h"
#include "avia_av_event.h"
#include "avia_av_proc.h"

#include <tuxbox/info_dbox2.h>

/* ---------------------------------------------------------------------- */

TUXBOX_INFO(dbox2_gt);

static int pal = 1;
static char *firmware = NULL;

static int debug = 0;
#define dprintk if (debug) printk

static volatile u8 *aviamem = NULL;
static int aviarev;
static int silirev;

static int dev;

/* interrupt stuff */
#define AVIA_INTERRUPT		  SIU_IRQ4

static spinlock_t avia_lock;
static spinlock_t avia_register_lock;
static wait_queue_head_t avia_cmd_wait;
static wait_queue_head_t avia_cmd_state_wait;
static u8 bypass_mode = 0;
static u8 bypass_mode_changed = 0;
static u16 pid_audio = 0xFFFF;
static u16 pid_video = 0xFFFF;
static u8 play_state_audio = AVIA_AV_PLAY_STATE_STOPPED;
static u8 play_state_video = AVIA_AV_PLAY_STATE_STOPPED;
static u16 sample_rate = 44100;
static u8 stream_type_audio = AVIA_AV_STREAM_TYPE_SPTS;
static u8 stream_type_video = AVIA_AV_STREAM_TYPE_SPTS;
static u8 sync_mode = AVIA_AV_SYNC_MODE_AV;

/* finally i got them */
#define UX_MAGIC			0x00
#define UX_NUM_SECTIONS			0x01
#define UX_LENGTH_FILE			0x02
#define UX_FIRST_SECTION_START		0x03
#define UX_SECTION_LENGTH_OFFSET	0x01
#define UX_SECTION_WRITE_ADDR_OFFSET	0x02
#define UX_SECTION_CHECKSUM_OFFSET	0x03
#define UX_SECTION_DATA_OFFSET		0x04
#define UX_SECTION_DATA_GBUS_TABLE	0x02FF
#define UX_SECTION_DATA_IMEM		0x0200
#define UX_SECTION_HEADER_LENGTH	0x04

#define NTSC_16MB_WO_ROM_SRAM		7
#define NTSC_16MB_PL_ROM_SRAM		9
#define PAL_16MB_WO_ROM_SRAM		10
#define PAL_20MB_WO_ROM_SRAM		12

/* ---------------------------------------------------------------------- */

static int init_avia(void);

/* ---------------------------------------------------------------------- */

u32 avia_av_read(const u8 mode, u32 address)
{
	u32 result;

	spin_lock_irq(&avia_register_lock);

	address &= 0x3FFFFF;

	aviamem[6] = ((address >> 16) | mode) & 0xFF;
	aviamem[5] = (address >>  8) & 0xFF;
	aviamem[4] = address & 0xFF;

	mb();

	result  = aviamem[3] << 24;
	result |= aviamem[2] << 16;
	result |= aviamem[1] << 8;
	result |= aviamem[0];

	spin_unlock_irq(&avia_register_lock);

	return result;
}

void avia_av_write(const u8 mode, u32 address, const u32 data)
{
	spin_lock_irq(&avia_register_lock);

	address &= 0x3FFFFF;
	
	aviamem[6] = ((address >> 16) | mode) & 0xFF;
	aviamem[5] =  (address >>  8) & 0xFF;
	aviamem[4] = address & 0xFF;
	aviamem[3] = (data >> 24) & 0xFF;
	aviamem[2] = (data >> 16) & 0xFF;
	aviamem[1] = (data >>  8) & 0xFF;
	aviamem[0] = data & 0xFF;

	spin_unlock_irq(&avia_register_lock);
}

inline void avia_av_imem_write(const u32 addr, const u32 data)
{
	avia_av_gbus_write(0x36, addr);
	avia_av_gbus_write(0x34, data);
}

#if 0
inline u32 avia_av_imem_read(const u32 addr)
{
	avia_av_gbus_write(0x3A, 0x0B);
	avia_av_gbus_write(0x3B, addr);
	avia_av_gbus_write(0x3A, 0x0E);

	return avia_av_gbus_read(0x3B);
}
#endif

/* ---------------------------------------------------------------------- */

static
void avia_av_gbus_initial(const u32 *microcode)
{
	unsigned long *ptr = ((unsigned long*) microcode) + 0x306;
	int words = *ptr--, data, addr;

	dprintk(KERN_DEBUG "%s: %s: Performing %d initial G-bus Writes. "
		 "(don't panic! ;)\n", __FILE__, __FUNCTION__, words);

	while (words--) {
		addr = *ptr--;
		data = *ptr--;
		avia_av_gbus_write(addr, data);
	}
}

/* ---------------------------------------------------------------------- */

static
void avia_av_gbus_final(const u32 *microcode)
{
	unsigned long *ptr = ((unsigned long*) microcode) + 0x306;
	int words = *ptr--, data, addr;

	*ptr -= words;
	ptr -= words * 4;
	words = *ptr--;

	dprintk(KERN_DEBUG "%s: %s: Performing %d final G-bus Writes.\n",
		 __FILE__, __FUNCTION__, words);

	while (words--) {
		addr = *ptr--;
		data = *ptr--;
		avia_av_gbus_write (addr, data);
	}
}

/* ---------------------------------------------------------------------- */

void avia_av_dram_memcpy32(u32 dst, u32 *src, int dwords)
{
	while (dwords--) {
		avia_av_dram_write(dst, *src++);
		dst += 4;
	}
}

/* ---------------------------------------------------------------------- */

static
void avia_av_load_dram_image(u32 *microcode, const u32 section_start)
{
	u32 dst, *src, words, errors = 0;

	words = __le32_to_cpu(microcode[section_start + UX_SECTION_LENGTH_OFFSET]) / 4;
	dst = __le32_to_cpu(microcode[section_start + UX_SECTION_WRITE_ADDR_OFFSET]);
	src = &microcode[section_start + UX_SECTION_DATA_OFFSET];

	dprintk("%s: %s: Microcode at: %.8x (%.8x)\n",
		 __FILE__, __FUNCTION__, dst, (u32) words * 4);

	avia_av_dram_memcpy32(dst, src, words);

	while (words--) {
		if (avia_av_dram_read(dst) != *src++)
			errors++;
		dst += 4;
	}

	if (errors)
		printk(KERN_ERR "%s: %s: Microcode verify: %d errors.\n",
			__FILE__, __FUNCTION__, errors);
}

/* ---------------------------------------------------------------------- */

static
void avia_av_load_imem_image(u32 *microcode, const u32 data_start)
{
	u32 *src, i, words, errors = 0;

	src = &microcode[data_start + UX_SECTION_DATA_IMEM];
	words = 256;

	for (i = 0; i < words; i++)
		avia_av_imem_write(i, src[i]);

	avia_av_gbus_write(0x3A, 0xB); /* CPU_RMADR2 */
	avia_av_gbus_write(0x3B, 0x0); /* set starting address */
	avia_av_gbus_write(0x3A, 0xE); /* indirect regs */

	for (i = 0; i < words; i++)
		if (avia_av_gbus_read(0x3B) != src[i])
			errors++;

	if (errors)
		printk (KERN_ERR "%s: %s: Imem verify: %d errors\n",
			__FILE__, __FUNCTION__, errors);
}

/* ---------------------------------------------------------------------- */

static
void avia_av_load_microcode(u32 *microcode)
{
	int num_sections = __le32_to_cpu (microcode[1]);
	int data_offset  = 3;

	while (num_sections--) {
		avia_av_load_dram_image(microcode, data_offset);
		data_offset += (__le32_to_cpu (
			 microcode[data_offset+UX_SECTION_LENGTH_OFFSET]) / 4)
			 + UX_SECTION_HEADER_LENGTH;
	}
}

/* ---------------------------------------------------------------------- */

static
void avia_av_htd_interrupt(void)
{
	avia_av_gbus_write(0, avia_av_gbus_read(0) | (1 << 7));
	avia_av_gbus_write(0, avia_av_gbus_read(0) | (1 << 6));
}

/* ---------------------------------------------------------------------- */

static
void avia_av_interrupt(int irq, void *vdev, struct pt_regs *regs)
{
	u32 status = 0;
	u32 sem = 0;

	spin_lock(&avia_lock);

	status = avia_av_dram_read(0x2AC);

	/* avia cmd stuff */
	if ((status & (1 << 15)) || (status & (1 << 9))) {
		dprintk(KERN_INFO "%s: %s: CMD INTR\n", __FILE__,  __FUNCTION__);
		wake_up_interruptible(&avia_cmd_wait);
	}

	/* AUD INTR */
	if (status & (1 << 22)) {
		sem = avia_av_dram_read(0x460);

		dprintk(KERN_DEBUG "%s: %s: AUD INTR %.8x\n",
			 __FILE__, __FUNCTION__, sem);

		/*
		 * E0 AUDIO_CONFIG
		 * E8 AUDIO_DAC_MODE
		 * EC AUDIO_CLOCK_SELECTION
		 * F0 I2C_958_DELAY
		 */

		/* new sample freq. */
		if (sem & 7) {
			switch (sem & 7) {
			case 1: /* 44.1 */
				avia_av_dram_write(0xEC, (avia_av_dram_read(0xEC) & ~(7 << 2)) | (1 << 2));
				sample_rate = 44100;
				break;

			case 2: /* 48.0 */
				avia_av_dram_write(0xEC, (avia_av_dram_read(0xEC) & ~(7 << 2)));
				sample_rate = 48000;
				break;

			case 7: /* 32.0 */
				avia_av_dram_write(0xEC, (avia_av_dram_read(0xEC) & ~(7 << 2)) | (2 << 2));
				sample_rate = 32000;
				break;
			default:
				break;
			}
		}

		/* reserved */
		if (sem & (7 << 3))
			printk(KERN_INFO "%s: %s: Reserved %02X.\n",
				__FILE__, __FUNCTION__, (sem >> 3) & 7);

		/* new audio emphasis */
		if (sem & (3 << 6))
			switch ((sem >> 6) & 3) {
			case 1:
				dprintk(KERN_INFO "%s: %s: New audio emphasis is off.\n",
					__FILE__, __FUNCTION__);
				break;

			case 2:
				dprintk(KERN_INFO "%s: %s: New audio emphasis is on.\n",
					__FILE__, __FUNCTION__);
				break;

			default:
				break;
			}

		if (sem & 0xFF)
			avia_av_dram_write (0x468, 1);
	}

	/* intr. ack */
	avia_av_gbus_write(0, ((avia_av_gbus_read (0) & ~1) | 2));

	/* clear flags */
	avia_av_dram_write(0x2B4, 0);
	avia_av_dram_write(0x2B8, 0);
	avia_av_dram_write(0x2C4, 0);
	avia_av_dram_write(0x2AC, 0);

	spin_unlock(&avia_lock);

	return;

}

/* ---------------------------------------------------------------------- */

int avia_av_standby(const int state)
{
	if (state == 0)
	{
		if (init_avia())
			printk("AVIA: wakeup ... error\n");
		else
			printk("AVIA: wakeup ... ok\n");
	}
	else
	{
		avia_av_event_exit();

		/* disable interrupts */
		avia_av_dram_write(0x200, 0);

		free_irq(AVIA_INTERRUPT, &dev);

		/* enable host access */
		avia_av_gbus_write(0, 0x1000);
		/* cpu reset, 3 state mode */
		avia_av_gbus_write(0x39, 0xF00000);
		/* cpu reset by fp */
		dbox2_fp_reset(0xBF & ~2);
	}

	return 0;
}

/* ---------------------------------------------------------------------- */

static
u32 avia_cmd_status_get(const u32 status_addr, const u8 wait_for_completion)
{

	int waittime = 350;

	if (!status_addr)
		return 0;

	dprintk("SA: 0x%X -> run\n", status_addr);

	while ((waittime > 0) && wait_for_completion && (avia_av_dram_read(status_addr) < 0x03)) {

		waittime = interruptible_sleep_on_timeout(&avia_cmd_wait, waittime);

		if ((!waittime) && (avia_av_dram_read(status_addr) < 0x03)) {

			printk(KERN_ERR "avia_av: timeout error while fetching command status\n");

			return 0;

		}

		dprintk("SA: 0x%X -> complete (time: %d, status: %d)\n", status_addr, waittime, avia_av_dram_read(status_addr));

	}

	dprintk("SA: 0x%X -> end -> S: 0x%X\n", status_addr, avia_av_dram_read(status_addr));

	if (avia_av_dram_read(status_addr) >= 0x05)
		printk("avia_av: warning - command @ 0x%X failed\n", status_addr);

	return avia_av_dram_read(status_addr);

}

static
u32 avia_cmd_status_get_addr(void)
{

	long timeout = jiffies + HZ * 2;

	while ((!avia_av_dram_read(0x5C)) && (time_before(jiffies, timeout)))
		schedule();

	return avia_av_dram_read(0x5C);
	
}

u32 avia_av_cmd(u32 command, ...)
{

	u32	i;
	va_list ap;
	u32 status_addr;


#if 0
	va_start(ap, command);
	printk(KERN_INFO "Avia-Command: 0x%04X ",command);
	for (i = 0; i < ((command & 0x7F00) >> 8); i++)
		printk("0x%04X ",va_arg(ap, int));
	va_end(ap);
	printk("\n");
#endif

	if (!avia_cmd_status_get_addr()) {

		printk(KERN_ERR "avia_av: status timeout - chip not ready for new command\n");

		avia_av_standby(1);
		avia_av_standby(0);

		return 0;

	}

	dprintk("C: 0x%X -> PROC: 0x%X\n", command, avia_av_dram_read(0x2A0));

	va_start(ap, command);

	spin_lock_irq(&avia_lock);

	avia_av_dram_write(0x40, command);

	for (i = 0; i < ((command & 0x7F00) >> 8); i++)
		avia_av_dram_write(0x44 + i * 4, va_arg(ap, int));

	for (; i < 8; i++)
		avia_av_dram_write(0x44 + i * 4, 0);

	/* RUN */
	avia_av_dram_write(0x5C, 0);

	/* host-to-decoder interrupt */
	avia_av_htd_interrupt();

	spin_unlock_irq(&avia_lock);

	va_end(ap);

	if (!(status_addr = avia_cmd_status_get_addr())) {

		printk(KERN_ERR "avia_av: status timeout - chip didn't accept command 0x%X\n", command);

		avia_av_standby(1);
		avia_av_standby(0);

		return 0;

	}

	dprintk("C: 0x%X -> SA: 0x%X PROC: 0x%X\n", command, status_addr, avia_av_dram_read(0x2A0));

	if (command & 0x8000)
		avia_cmd_status_get(status_addr, 1);

	return status_addr;

}

void avia_av_set_pcr(const u32 hi, const u32 lo)
{
	u32 data1 = (hi >> 16) & 0xFFFF;
	u32 data2 = hi & 0xFFFF;
	u32 timer_high = ((1 << 21))|((data1 & 0xE000L) << 4)
				| (1 << 16) | ((data1 & 0x1FFFL) << 3)
				| ((data2 & 0xC000L) >> 13) | 1L;

	u32 timer_low = ((data2 & 0x03FFFL) << 2)
				| ((lo & 0x8000L) >> 14) | 1L;

	dprintk(KERN_INFO "AVIA: Setting PCR: %08x:%08x\n", hi, lo);

	avia_av_gbus_write(0x02, timer_high);
	avia_av_gbus_write(0x03, timer_low);

	if (sync_mode != AVIA_AV_SYNC_MODE_NONE)
		avia_av_dram_write(AV_SYNC_MODE, sync_mode);

}

/*
static int wait_audio_sequence(void)
{
	while(avia_av_dram_read(AUDIO_SEQUENCE_ID));
	return 0;
}

static int init_audio_sequence(void)
{
	avia_av_dram_write(AUDIO_SEQUENCE_ID, 0);
	avia_av_dram_write(NEW_AUDIO_SEQUENCE, 1);
	return wait_audio_sequence();
}

static int new_audio_sequence( u32 val )
{
	avia_av_dram_write(AUDIO_SEQUENCE_ID, val);
	avia_av_dram_write(NEW_AUDIO_SEQUENCE, 2);
	return wait_audio_sequence();
}
*/

static
void avia_av_audio_init(void)
{
	u32 val;

	/* 
	 * AUDIO_CONFIG
	 * 12,11,7,6,5,4 reserved or must be set to 0
	 */
	val  = 0;
	val |= (0<<10);	/* 1: 64 0: 32/48 */
	val |= (0<<9);	/* 1: I2S 0: other */
	val |= (0<<8);	/* 1: no clock on da-iec */
	val |= (1<<3);	/* 0: normal 1:I2S output */
	val |= (1<<2);	/* 0:off 1:on channels */
	val |= (1<<1);	/* 0:off 1:on IEC-958 */
	val |= (1);	/* 0:encoded 1:decoded output */
	avia_av_dram_write(AUDIO_CONFIG, val);

	/*
	 * AUDIO_DAC_MODE
	 * 0 reserved
	 */
	val  = 0;
	val |= (0<<8);
	val |= (3<<6);
	val |= (0<<4);
	val |= (0<<3);	/* 0:high 1:low DA-LRCK polarity */
	val |= (1<<2);	/* 0:0 as MSB in 24 bit mode 1: sign ext. in 24bit */
	val |= (0<<1);	/* 0:msb 1:lsb first */
	avia_av_dram_write(AUDIO_DAC_MODE, val);

	/* AUDIO_CLOCK_SELECTION */
	val = 0;
	val |= (1<<2);

	/* 500/600 test */
	if ((aviarev == 0x00) && (silirev == 0x80))
		val |= (0<<1);	/* 1:256 0:384 x sampling frequ. */
	else
		val |= (1<<1);	/* 1:256 0:384 x sampling frequ. */

	val |= (1);	/* master, slave mode */
	avia_av_dram_write(AUDIO_CLOCK_SELECTION, val);

	/* AUDIO_ATTENUATION */
	avia_av_dram_write(AUDIO_ATTENUATION, 0);

	/* SET SCMS */
	avia_av_dram_write(IEC_958_CHANNEL_STATUS_BITS, avia_av_dram_read(IEC_958_CHANNEL_STATUS_BITS) | 5);

	sample_rate = 44100;
}

/* ---------------------------------------------------------------------- */

static
void avia_av_set_default(void)
{
	u32 val = 0;

#if 0
	val |= (0 << 2);	/* 1: tristate */
	val |= (0 << 0);	/* 0: slave 1: master HSYNC/VSYNC */
	val |= (0 << 0);	/* 0: BT.601 1: BT.656 */
#endif

	avia_av_dram_write(VIDEO_MODE, val);

	/* 0: 4:3 1: 16:9 downs. 2: 16:9 */
	avia_av_dram_write(DISPLAY_ASPECT_RATIO, 0);

	/* 0: disable 1: PAN&SCAN 2: Letterbox */
	avia_av_dram_write(ASPECT_RATIO_MODE, 2);

	/* 2: 4:3 3: 16:9 4: 20:9 */
	avia_av_dram_write(FORCE_CODED_ASPECT_RATIO, 0);

	avia_av_dram_write(PAN_SCAN_SOURCE, 0);
	avia_av_dram_write(PAN_SCAN_HORIZONTAL_OFFSET, 0);

	val = 0x108080;
	avia_av_dram_write(BORDER_COLOR, val);
	avia_av_dram_write(BACKGROUND_COLOR, val);

	/* 0: I_FRAME 1: I_SLICE */
	avia_av_dram_write(I_SLICE, 0);

	/* 0: frame 1: slice based error recovery */
	avia_av_dram_write(ERR_CONCEALMENT_LEVEL, 0);


	/* PLAY MODE PARAMETER */


	/* 0: Demux interface 2: Host Interface */
	avia_av_dram_write(BITSTREAM_SOURCE, 0);

	switch (tuxbox_dbox2_gt) {
	case TUXBOX_DBOX2_GT_GTX:
		avia_av_dram_write(TM_MODE, 0x0a); /* GTX */
		break;
	case TUXBOX_DBOX2_GT_ENX:
		avia_av_dram_write(TM_MODE, 0x18); /* eNX */
		break;
	}

	avia_av_dram_write(AV_SYNC_MODE, AVIA_AV_SYNC_MODE_NONE);

	avia_av_dram_write(VIDEO_PTS_DELAY, 0);
	avia_av_dram_write(VIDEO_PTS_SKIP_THRESHOLD, 0xE10);
	avia_av_dram_write(VIDEO_PTS_REPEAT_THRESHOLD, 0xE10);

	avia_av_dram_write(AUDIO_PTS_DELAY, 0xe00);
	avia_av_dram_write(AUDIO_PTS_SKIP_THRESHOLD_1, 0xE10);
	avia_av_dram_write(AUDIO_PTS_REPEAT_THRESHOLD_1, 0xE10);
	avia_av_dram_write(AUDIO_PTS_SKIP_THRESHOLD_2, 0x2300);
	avia_av_dram_write(AUDIO_PTS_REPEAT_THRESHOLD_2, 0x2300);

	/* */
	avia_av_dram_write(INTERPRET_USER_DATA, 0);
	avia_av_dram_write(INTERPRET_USER_DATA_MASK, 0);

	/* osd */
	avia_av_dram_write(DISABLE_OSD, 0);

	/* disable osd */
	avia_av_dram_write(OSD_EVEN_FIELD, 0);
	avia_av_dram_write(OSD_ODD_FIELD, 0);

	avia_av_dram_write(0x64, 0);
	avia_av_dram_write(DRAM_INFO, 0);
	avia_av_dram_write(UCODE_MEMORY, 0);

	/* set pal or ntsc */
	if (pal)
		avia_av_dram_write(MEMORY_MAP, PAL_16MB_WO_ROM_SRAM);
	else
		avia_av_dram_write(MEMORY_MAP, NTSC_16MB_WO_ROM_SRAM);
}

/* ---------------------------------------------------------------------- */

static 
int avia_av_set_ppc_siumcr(void)
{
	immap_t *immap = NULL;
	sysconf8xx_t *sys_conf = NULL;

	immap = (immap_t *)IMAP_ADDR;

	if (!immap)
	{
		dprintk(KERN_ERR "AVIA: Get immap failed.\n");
		return -1;
	}

	sys_conf = (sysconf8xx_t *)&immap->im_siu_conf;

	if (!sys_conf)
	{
		dprintk(KERN_ERR "AVIA: Get sys_conf failed.\n");
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
/* shamelessly stolen from sound_firmware.c */
static int errno;

static
int avia_av_firmware_read(const char *fn, char **fp)
{
	int fd = 0;
	loff_t l = 0;
	char *dp = NULL;
	char firmwarePath[100] = { "\0" };

	if (!fn)
		return 0;

	strncpy(firmwarePath, fn, sizeof(firmwarePath) - sizeof("/avia600.ux") - 1);
	firmwarePath[sizeof(firmwarePath) - sizeof("/avia600.ux") - 2] = 0;

	if (!aviarev)
		strcat(firmwarePath, "/avia600.ux");
	else
		strcat(firmwarePath, "/avia500.ux");

	if ((fd = open(firmwarePath, 0, 0)) < 0) {
		printk (KERN_ERR "%s: %s: Unable to load '%s'.\n",
			__FILE__, __FUNCTION__, firmwarePath);
		return 0;
	}

	l = lseek(fd, 0L, 2);

	if ((l <= 0) || (l >= 128 * 1024)) {
		printk(KERN_ERR "%s: %s: Firmware wrong size '%s'.\n",
			__FILE__, __FUNCTION__, firmwarePath);
		sys_close(fd);
		return 0;
	}

	lseek(fd, 0L, 0);
	dp = vmalloc(l);

	if (dp == NULL) {
		printk(KERN_ERR "%s: %s: Out of memory loading '%s'.\n",
			__FILE__, __FUNCTION__, firmwarePath);
		sys_close(fd);
		return 0;
	}

	if (read(fd, dp, l) != l) {
		printk(KERN_ERR "%s: %s: Failed to read '%s'.\n",
			__FILE__, __FUNCTION__, firmwarePath);
		vfree(dp);
		sys_close(fd);
		return 0;
	}

	close(fd);
	*fp = dp;
	return l;
}

/* ---------------------------------------------------------------------- */

static int init_avia(void)
{
	u32 *microcode = NULL;
	u32 val = 0;
	int tries = 0;
	mm_segment_t fs;
	
	/* remap avia memory */
	if (!aviamem)
		aviamem = (unsigned char*)ioremap(0xA000000, 0x200);

	if (!aviamem)
	{
		printk(KERN_ERR "AVIA: Failed to remap memory.\n");
		return -ENOMEM;
	}

	(void)aviamem[0];

	/* read revision */
	aviarev = (avia_av_gbus_read(0) >> 16) & 3;

	fs = get_fs();
	set_fs(get_ds());

	/* read firmware */
	if (avia_av_firmware_read(firmware, (char**) &microcode) == 0)
	{
		set_fs(fs);
		iounmap((void*)aviamem);
		return -EIO;
	}

	set_fs(fs);

	/* set siumcr for interrupt */
	if (avia_av_set_ppc_siumcr() < 0)
	{
		vfree(microcode);
		iounmap((void*)aviamem);
		return -EIO;
	}

	/* request avia interrupt */
	if (request_8xxirq(AVIA_INTERRUPT, avia_av_interrupt, 0, "avia", &dev) != 0)
	{
		printk(KERN_ERR "AVIA: Failed to get interrupt.\n");
		vfree(microcode);
		iounmap((void*)aviamem);
		return -EIO;
	}

	/* init queue */
	init_waitqueue_head(&avia_cmd_wait);

	init_waitqueue_head(&avia_cmd_state_wait);

	/* enable host access */
	avia_av_gbus_write(0, 0x1000);
	/* cpu reset */
	avia_av_gbus_write(0x39, 0xF00000);
	/* cpu reset by fp */
	dbox2_fp_reset(0xBF & ~2);
	/* enable host access */
	avia_av_gbus_write(0, 0x1000);
	/* cpu reset */
	avia_av_gbus_write(0x39, 0xF00000);

	avia_av_gbus_write(0, 0x1000);

	/*aviarev=(avia_av_gbus_read(0)>>16)&3;*/
	silirev = ((avia_av_gbus_read(0x22) >> 8) & 0xFF);

	dprintk(KERN_INFO "AVIA: AVIA REV: %02X SILICON REV: %02X\n",aviarev,silirev);

	/*
	 * AR SR CHIP FILE
	 * 00 80 600L 600...
	 * 03 00 600L 500... ???
	 * 03 00 500  500
	*/

	switch (aviarev) {
	case 0:
		dprintk(KERN_INFO "AVIA: AVIA 600L found.\n");
		break;
	case 1:
		dprintk(KERN_INFO "AVIA: AVIA 500 LB3 found. (no microcode)\n");
		break;
#if 0
	case 3:
		dprintk("AVIA 600L found. (no support yet)\n");
		break;
#endif
	default:
		dprintk(KERN_INFO "AVIA: AVIA 500 LB4 found. (nice)\n");
		break;
	}

	/* D.4.3 - Initialize the SDRAM DMA Controller */
	switch (aviarev) {
	case 0:
	/* case 3: */
		avia_av_gbus_write(0x22, 0x10000);
		avia_av_gbus_write(0x23, 0x5FBE);
		avia_av_gbus_write(0x22, 0x12);
		avia_av_gbus_write(0x23, 0x3a1800);
		break;
	default:
		avia_av_gbus_write(0x22, 0xF);
		val = avia_av_gbus_read(0x23) | 0x14EC;
		avia_av_gbus_write(0x22, 0xF);
		avia_av_gbus_write(0x23, val);
		avia_av_gbus_read(0x23);
		avia_av_gbus_write(0x22, 0x11);
		val = (avia_av_gbus_read(0x23) & 0x00FFFFFF) | 1;
		avia_av_gbus_write(0x22, 0x11);
		avia_av_gbus_write(0x23, val);
		avia_av_gbus_read(0x23);
		break;
	}

	avia_av_gbus_initial(microcode);
	avia_av_load_microcode(microcode);
	avia_av_load_imem_image(microcode, UX_FIRST_SECTION_START + UX_SECTION_DATA_OFFSET);
	avia_av_set_default();
	avia_av_audio_init();
	/* init_audio_sequence(); */
	avia_av_gbus_final(microcode);

	vfree(microcode);

	/* set cpu running mode */
	avia_av_gbus_write(0x39, 0x900000);

	/* enable decoder/host interrupt */
	avia_av_gbus_write(0, avia_av_gbus_read(0) | (1 << 7));
	avia_av_gbus_write(0, avia_av_gbus_read(0) & ~(1 << 6));
	avia_av_gbus_write(0, avia_av_gbus_read(0) | (1 << 1));
	avia_av_gbus_write(0, avia_av_gbus_read(0) & ~1);

	/* clear int. flags */
	avia_av_dram_write(0x2B4, 0);
	avia_av_dram_write(0x2B8, 0);
	avia_av_dram_write(0x2C4, 0);
	avia_av_dram_write(0x2AC, 0);

	/* enable interrupts */
	avia_av_dram_write(0x200, ((1 << 22) | (1 << 15) | (1 << 9)));

	tries = 20;

	while ((avia_av_dram_read(0x2A0) != 0x2))
	{
		if (!--tries)
			break;
		udelay(10*1000);
		schedule();
	}

	if (!tries)
	{
		dprintk(KERN_ERR "AVIA: Timeout waiting for decoder initcomplete. (%08X)\n",avia_av_dram_read(0x2A0));
		iounmap((void*)aviamem);
		free_irq(AVIA_INTERRUPT, &dev);
		return -EIO;
	}

	/* new audio config */
	avia_av_dram_write(0x468, 0xFFFF);

	tries = 20;

	while (avia_av_dram_read(0x468))
	{
		if (!--tries)
			break;
		udelay(10*1000);
		schedule();
	}

	if (!tries)
	{
		dprintk(KERN_ERR "AVIA: New_audio_config timeout\n");
		iounmap((void*)aviamem);
		free_irq(AVIA_INTERRUPT, &dev);
		return -EIO;
	}

	avia_av_event_init();

	/*
	avia_av_dram_write(OSD_BUFFER_START, 0x1f0000);
	avia_av_dram_write(OSD_BUFFER_END,   0x200000);
	*/

	avia_av_dram_write(OSD_BUFFER_START, 0x022000);
	avia_av_dram_write(OSD_BUFFER_END, 0x03a000);

	avia_av_cmd(Reset);
	udelay(1000);
	avia_av_cmd(SelectStream, 0x00, 0xFFFF);
	avia_av_cmd(SelectStream, 0x03, 0xFFFF);
	avia_av_cmd(Play, 0x00, 0xFFFF, 0xFFFF);

	dprintk(KERN_INFO "AVIA: Using avia firmware revision %c%c%c%c\n", avia_av_dram_read(0x330)>>24, avia_av_dram_read(0x330)>>16, avia_av_dram_read(0x330)>>8, avia_av_dram_read(0x330));
	dprintk(KERN_INFO "AVIA: %x %x %x %x %x\n", avia_av_dram_read(0x2C8), avia_av_dram_read(0x2CC), avia_av_dram_read(0x2B4), avia_av_dram_read(0x2B8), avia_av_dram_read(0x2C4));

	return 0;
}

/* ---------------------------------------------------------------------- */

u16 avia_av_get_sample_rate(void)
{
	return sample_rate;
}

void avia_av_bypass_mode_set(const u8 enable)
{
	if (bypass_mode == !!enable)
		return;

	/*
	 * AVIA 500 cannot change bypass-mode in play mode.
	 */

	if (aviarev)
		avia_av_cmd(Abort, 0x00);

	if (enable)
		avia_av_dram_write(AUDIO_CONFIG, avia_av_dram_read(AUDIO_CONFIG) & ~1);
	else
		avia_av_dram_write(AUDIO_CONFIG, avia_av_dram_read(AUDIO_CONFIG) | 1);

	avia_av_dram_write(NEW_AUDIO_CONFIG, 1);

	bypass_mode = !!enable;
	bypass_mode_changed = 1;
}

int avia_av_pid_set(const u8 type, const u16 pid)
{
	if ((pid > 0x1FFF) && (pid != 0xFFFF))
		return -EINVAL;

	switch (type) {
	case AVIA_AV_TYPE_AUDIO:
		if (pid_audio == pid)
			return 0;
		pid_audio = pid;
		break;

	case AVIA_AV_TYPE_VIDEO:
		if (pid_video == pid)
			return 0;
		pid_video = pid;
		break;

	default:
		printk("avia_av: invalid pid type\n");
		return -EINVAL;
	}

	return 0;
}

int avia_av_play_state_set_audio(const u8 new_play_state)
{
	switch (new_play_state) {
	case AVIA_AV_PLAY_STATE_PAUSED:
		if (play_state_audio != AVIA_AV_PLAY_STATE_PLAYING)
			return -EINVAL;

		dprintk("avia_av: pausing audio decoder\n");
		avia_av_cmd(SelectStream, 0x03 - bypass_mode, 0xFFFF);
		break;

	case AVIA_AV_PLAY_STATE_PLAYING:
		printk("avia_av: audio play (apid=0x%04X)\n", pid_audio);
		avia_av_cmd(SelectStream, 0x03 - bypass_mode, pid_audio);
		if (aviarev && bypass_mode_changed)
		{
			avia_av_cmd(SelectStream,0x00,(play_state_video == AVIA_AV_PLAY_STATE_PLAYING) ? pid_video : 0xFFFF);
			avia_av_cmd(Play,0x00,(play_state_video == AVIA_AV_PLAY_STATE_PLAYING) ? pid_video : 0xFFFF,pid_audio);
		}
		bypass_mode_changed = 0;
		break;

	case AVIA_AV_PLAY_STATE_STOPPED:
		if ((play_state_audio != AVIA_AV_PLAY_STATE_PAUSED) &&
			(play_state_audio != AVIA_AV_PLAY_STATE_PLAYING))
				return -EINVAL;

		dprintk("avia_av: stopping audio decoder\n");
		avia_av_cmd(SelectStream, 0x03 - bypass_mode, 0xFFFF);

		if (play_state_video == AVIA_AV_PLAY_STATE_STOPPED)
		{
			avia_av_dram_write(AV_SYNC_MODE, AVIA_AV_SYNC_MODE_NONE);
			if (aviarev)
			{
				avia_av_cmd(Play,0x00,0xFFFF,0xFFFF);
			}
			else
			{
				avia_av_cmd(NewChannel,0x00,0xFFFF,0xFFFF);
			}
		}
		break;

	default:
		printk("avia_av: invalid audio play state\n");
		return -EINVAL;
	}

	play_state_audio = new_play_state;
	return 0;
}

int avia_av_play_state_set_video(const u8 new_play_state)
{
	switch (new_play_state) {
	case AVIA_AV_PLAY_STATE_PAUSED:
		if (play_state_video != AVIA_AV_PLAY_STATE_PLAYING)
			return -EINVAL;
		avia_av_cmd(Freeze, 1);
		break;

	case AVIA_AV_PLAY_STATE_PLAYING:
		if (play_state_video == AVIA_AV_PLAY_STATE_PLAYING)
			return -EINVAL;
		else if (play_state_video == AVIA_AV_PLAY_STATE_PAUSED)
			avia_av_cmd(Resume);
		else {
			printk("avia_av: video play (vpid=0x%04X)\n", pid_video);
			avia_av_cmd(SelectStream, 0x00, pid_video);
		}
		break;

	case AVIA_AV_PLAY_STATE_STOPPED:
		if ((play_state_video != AVIA_AV_PLAY_STATE_PAUSED) &&
			(play_state_video != AVIA_AV_PLAY_STATE_PLAYING))
				return -EINVAL;

		dprintk("avia_av: stopping video decoder\n");
		avia_av_cmd(SelectStream, 0x00, 0xFFFF);

		if (play_state_audio == AVIA_AV_PLAY_STATE_STOPPED)
		{
			avia_av_dram_write(AV_SYNC_MODE, AVIA_AV_SYNC_MODE_NONE);
			if (aviarev)
			{
				avia_av_cmd(Play,0x00,0xFFFF,0xFFFF);
			}
			else
			{
				avia_av_cmd(NewChannel,0x00,0xFFFF,0xFFFF);
			}
		}
		break;

	default:
		printk("avia_av: invalid video play state\n");
		return -EINVAL;
	}

	play_state_video = new_play_state;
	return 0;
}

int avia_av_stream_type_set(const u8 new_stream_type_video, const u8 new_stream_type_audio)
{

	if ((play_state_video != AVIA_AV_PLAY_STATE_STOPPED) || (play_state_audio != AVIA_AV_PLAY_STATE_STOPPED))
		return -EBUSY;
		
	if ((new_stream_type_video == stream_type_video) && (new_stream_type_audio == stream_type_audio))
		return 0;

	dprintk("avia_av: setting stream type %d/%d\n", new_stream_type_video, new_stream_type_audio);

	switch (new_stream_type_video) {
	case AVIA_AV_STREAM_TYPE_ES:
		switch (new_stream_type_audio) {
		case AVIA_AV_STREAM_TYPE_ES:
			avia_av_cmd(SetStreamType, 0x08, 0x0000);
			break;
			
		case AVIA_AV_STREAM_TYPE_PES:
			avia_av_cmd(SetStreamType, 0x0A, 0x0000);
			break;
			
		case AVIA_AV_STREAM_TYPE_SPTS:
			printk("avia_av: video ES with audio SPTS stream type is not supported\n");
			return -EINVAL;
				
		default:
			printk("avia_av: invalid audio stream type\n");
			return -EINVAL;
		}	
		break;
	
	case AVIA_AV_STREAM_TYPE_PES:
		switch (new_stream_type_audio) {
		case AVIA_AV_STREAM_TYPE_ES:
			avia_av_cmd(SetStreamType, 0x09, 0x0000);
			break;
			
		case AVIA_AV_STREAM_TYPE_PES:
			avia_av_cmd(SetStreamType, 0x0B, 0x0000);
			break;
			
		case AVIA_AV_STREAM_TYPE_SPTS:
			printk("avia_av: video PES with audio SPTS stream type is not supported\n");
			return -EINVAL;
				
		default:
			printk("avia_av: invalid audio stream type\n");
			return -EINVAL;
		}
		break;
		
	case AVIA_AV_STREAM_TYPE_SPTS:
		switch (new_stream_type_audio) {
		case AVIA_AV_STREAM_TYPE_ES:
			printk("avia_av: video SPTS with audio ES stream type is not supported\n");
			return -EINVAL;

		case AVIA_AV_STREAM_TYPE_PES:
			printk("avia_av: video SPTS with audio PES stream type is not supported\n");
			return -EINVAL;
					
		case AVIA_AV_STREAM_TYPE_SPTS:
			/*
			 * AViA 500 doesn't support SetStreamType 0x10/0x11
			 * So we Reset the AViA 500 back to SPTS mode
			 */
			if (aviarev) {
				avia_av_cmd(Reset);
			} else {
				avia_av_cmd(SetStreamType, 0x10, pid_audio);
				avia_av_cmd(SetStreamType, 0x11, pid_video);
			}
			break;

		default:
			printk("avia_av: invalid audio stream type\n");
			return -EINVAL;
		}
		break;

	default:
		printk("avia_av: invalid video stream type\n");
		return -EINVAL;
	}

	stream_type_audio = new_stream_type_audio;
	stream_type_video = new_stream_type_video;
	return 0;

}

int avia_av_sync_mode_set(const u8 new_sync_mode)
{
	if ((new_sync_mode != AVIA_AV_SYNC_MODE_NONE) &&
		(new_sync_mode != AVIA_AV_SYNC_MODE_AUDIO) &&
		(new_sync_mode != AVIA_AV_SYNC_MODE_VIDEO) &&
		(new_sync_mode != AVIA_AV_SYNC_MODE_AV))
		return -EINVAL;

	if ((play_state_video != AVIA_AV_PLAY_STATE_STOPPED) || (play_state_audio != AVIA_AV_PLAY_STATE_STOPPED))
		avia_av_dram_write(AV_SYNC_MODE, new_sync_mode);

	sync_mode = new_sync_mode;
	return 0;
}

void avia_av_set_audio_attenuation(const u8 att)
{
	avia_av_dram_write(AUDIO_ATTENUATION, att);
}

/* ---------------------------------------------------------------------- */

int __init avia_av_core_init(void)
{

	int err;

	printk("avia_av: $Id: avia_av_core.c,v 1.61 2003/04/17 07:29:47 obi Exp $\n");

	if (!(err = init_avia()))
		avia_av_proc_init();

	return err;
	
}

void __exit avia_av_core_exit(void)
{

	avia_av_proc_exit();

	avia_av_standby(1);

	if (aviamem)
		iounmap((void*)aviamem);

}

module_init(avia_av_core_init);
module_exit(avia_av_core_exit);

MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("AViA 500/600 driver");
MODULE_LICENSE("GPL");
MODULE_PARM(debug,"i");
MODULE_PARM(pal,"i");
MODULE_PARM(firmware,"s");
MODULE_PARM_DESC(debug, "1: enable debug messages");
MODULE_PARM_DESC(pal, "0: ntsc, 1: pal");
MODULE_PARM_DESC(firmware, "path to AViA500/600 microcode");

EXPORT_SYMBOL(avia_av_read);
EXPORT_SYMBOL(avia_av_write);
EXPORT_SYMBOL(avia_av_dram_memcpy32);
EXPORT_SYMBOL(avia_av_cmd);
EXPORT_SYMBOL(avia_av_set_pcr);
EXPORT_SYMBOL(avia_av_set_audio_attenuation);
EXPORT_SYMBOL(avia_av_standby);
EXPORT_SYMBOL(avia_av_get_sample_rate);

EXPORT_SYMBOL(avia_av_bypass_mode_set);
EXPORT_SYMBOL(avia_av_pid_set);
EXPORT_SYMBOL(avia_av_play_state_set_audio);
EXPORT_SYMBOL(avia_av_play_state_set_video);
EXPORT_SYMBOL(avia_av_stream_type_set);
EXPORT_SYMBOL(avia_av_sync_mode_set);

