/*
 *   saa7126_core.c - pal driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2001 Gillem (htoa@gmx.net)
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
 *   $Log: saa7126_core.c,v $
 *   Revision 1.3  2001/02/01 19:56:38  gillem
 *   - 2.4.1 support
 *
 *   Revision 1.2  2001/01/06 10:06:55  gillem
 *   cvs check
 *
 *   $Revision: 1.3 $
 *
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/malloc.h>
#include <linux/mm.h>
#include <linux/pci.h>
#include <linux/signal.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <linux/sched.h>
#include <asm/segment.h>
#include <linux/types.h>
#include <linux/wrapper.h>

#include <linux/proc_fs.h>

#include <linux/videodev.h>
#include <linux/version.h>
#include <asm/uaccess.h>

#include <linux/i2c.h>
#include <linux/video_encoder.h>

#define DEBUG(x)   x		/* Debug driver */

///////////////////////////////////////////////////////////////////////////////

static unsigned char PAL_SAA_NOKIA[] =
{
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x21, 0x1D, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x1A, 0x1A,
/* COLORBAR=0x80 */
0x03,

0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
0x00, 0x00, 0x68, 0x7D, 0xAF, 0x33, 0x35, 0x35,
0x00, 0x06, 0x2F, 0xCB, 0x8A, 0x09, 0x2A, 0x00,
0x00, 0x00, 0x00,
/* 0x6B */
0x52,
0x28, 0x01, 0x20, 0x31,
0x7D, 0xBC, 0x60, 0x41, 0x05, 0x00, 0x06, 0x16,
0x06, 0x16, 0x16, 0x36, 0x60, 0x00, 0x00, 0x00
};

static unsigned char PAL_SAA_NOKIA_CONFIG[] =
{
0x00, 0x1b, 0x00, 0x22, 0x2b, 0x08, 0x74, 0x55,
0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x22, 0x15, 0x60, 0x00, 0x07, 0x7e, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x3b, 0x72, 0x00, 0x00, 0x02, 0x54, 0x00, 0x00,
0x21, 0x1d, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00,
0x2c, 0x04, 0x00, 0xfe, 0x00, 0x00, 0x7e, 0x00,
0x1a, 0x1a, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
0x00, 0x00, 0x68, 0x7d, 0xaf, 0x33, 0x35, 0x35,
0x00, 0x06, 0x2f, 0xcb, 0x8a, 0x09, 0x2a, 0x00,
0x00, 0x00, 0x00, 0x52, 0x28, 0x01, 0x20, 0x31,
0x7d, 0xbc, 0x60, 0x41, 0x05, 0x00, 0x06, 0x16,
0x06, 0x16, 0x16, 0x36, 0x60, 0x00, 0x00, 0x00
};

static unsigned char PAL_SAA_PHILIPS[] =
{
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x22, 0x1E, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x1C, 0x1C, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x70, 0x78, 0xAB, 0x1B, 0x26, 0x26,
0x00, 0x16, 0x2E, 0xCB, 0x8A, 0x09, 0x2A, 0x00,
0x00, 0x00, 0x00, 0x72, 0x00, 0x00, 0xA0, 0x31,
0x7D, 0xBB, 0x60, 0x40, 0x07, 0x00, 0x06, 0x16,
0x06, 0x16, 0x16, 0x36, 0x60, 0x00, 0x00, 0x00
};

static unsigned char PAL_SAA_SAGEM[] =
{
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x21, 0x1D, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x1E, 0x1E, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x54, 0xF2, 0x90,
0x00, 0x00, 0x70, 0x75, 0xA5, 0x37, 0x39, 0x39,
0x00, 0x06, 0x2C, 0xCB, 0x8A, 0x09, 0x2A, 0x00,
0x00, 0x00, 0x00, 0x52, 0x6F, 0x00, 0xA0, 0x31,
0x7D, 0xBF, 0x60, 0x40, 0x07, 0x00, 0x06, 0x16,
0x06, 0x16, 0x16, 0x36, 0x60, 0x00, 0x00, 0x00
};

///////////////////////////////////////////////////////////////////////////////

typedef struct s_saa_data {
	unsigned char version   : 3;
	unsigned char ccrdo     : 1;
	unsigned char ccrde     : 1;
	unsigned char res01     : 1;
	unsigned char fseq      : 1;
	unsigned char o_2       : 1;

	unsigned char res02[0x25-0x01];	// NULL

	unsigned char wss_7_0   : 8;

	unsigned char wsson     : 1;
	unsigned char res03     : 1;
	unsigned char wss_13_8  : 6;

	unsigned char deccol    : 1;
	unsigned char decfis    : 1;
	unsigned char bs        : 6;

	unsigned char sres      : 1;
	unsigned char res04     : 1;
	unsigned char be        : 6;


	unsigned char cg_7_0    : 8;

	unsigned char cg_15_8   : 8;

	unsigned char cgen      : 1;
	unsigned char res05     : 3;
	unsigned char cg_19_16  : 4;

	unsigned char vbsen     : 2;
	unsigned char cvbsen    : 1;
	unsigned char cen       : 1;
	unsigned char cvbstri   : 1;
	unsigned char rtri      : 1;
	unsigned char gtri      : 1;
	unsigned char btri      : 1;

	unsigned char res06[0x37-0x2e];	// NULL

	unsigned char res07     : 3;
	unsigned char gy        : 5;

	unsigned char res08     : 3;
	unsigned char gcd       : 5;

	unsigned char vbenb     : 1;
	unsigned char res09     : 2;
	unsigned char symp      : 1;
	unsigned char demoff    : 1;
	unsigned char csync     : 1;
	unsigned char mp2c      : 2;

	unsigned char vpsen     : 1;
	unsigned char ccirs     : 1;
	unsigned char res10     : 4;
	unsigned char edge      : 2;

	unsigned char vps5      : 8;
	unsigned char vps11     : 8;
	unsigned char vps12     : 8;
	unsigned char vps13     : 8;
	unsigned char vps14     : 8;

	unsigned char chps      : 8;
	unsigned char gainu_7_0 : 8;
	unsigned char gainv_7_0 : 8;

	unsigned char gainu_8   : 1;
	unsigned char decoe     : 1;
	unsigned char blckl     : 6;

	unsigned char gainv_8   : 1;
	unsigned char decph     : 1;
	unsigned char blnnl     : 6;

	unsigned char ccrs      : 2;
	unsigned char blnvb     : 6;

	unsigned char res11     : 8; // NULL

	unsigned char downb     : 1;
	unsigned char downa     : 1;
	unsigned char inpi      : 1;
	unsigned char ygs       : 1;
	unsigned char res12     : 1;
	unsigned char scbw      : 1;
	unsigned char apl       : 1;
	unsigned char fise      : 1;

	// 62h
	unsigned char rtce      : 1;
	unsigned char bsta      : 7;

	unsigned char fsc0      : 8;
	unsigned char fsc1      : 8;
	unsigned char fsc2      : 8;
	unsigned char fsc3      : 8;

	unsigned char l21o0     : 8;
	unsigned char l21o1     : 8;
	unsigned char l21e0     : 8;
	unsigned char l21e1     : 8;

	unsigned char srcv0     : 1;
	unsigned char srcv1     : 1;
	unsigned char trcv2     : 1;
	unsigned char orcv1     : 1;
	unsigned char prcv1     : 1;
	unsigned char cblf      : 1;
	unsigned char orcv2     : 1;
	unsigned char prcv2     : 1;

	// 6ch
	unsigned char htrig0    : 8;
	unsigned char htrig1    : 8;

	unsigned char sblbn     : 1;
	unsigned char blckon    : 1;
	unsigned char phres     : 2;
	unsigned char ldel      : 2;
	unsigned char flc       : 2;

	unsigned char ccen      : 2;
	unsigned char ttxen     : 1;
	unsigned char sccln     : 5;

	unsigned char rcv2s_lsb : 8;
	unsigned char rcv2e_lsb : 8;

	unsigned char res13     : 1;
	unsigned char rvce_mbs  : 3;
	unsigned char res14     : 1;
	unsigned char rvcs_mbs  : 3;

	unsigned char ttxhs     : 8;
	unsigned char ttxhl     : 4;
	unsigned char ttxhd     : 4;

	unsigned char csynca    : 5;
	unsigned char vss       : 3;

	unsigned char ttxovs    : 8;
	unsigned char ttxove    : 8;
	unsigned char ttxevs    : 8;
	unsigned char ttxeve    : 8;

	// 7ah
	unsigned char fal       : 8;
	unsigned char lal       : 8;

	unsigned char ttx60     : 1;
	unsigned char lal8      : 1;
	unsigned char ttx0      : 1;
	unsigned char fal8      : 1;
	unsigned char ttxeve8   : 1;
	unsigned char ttxove8   : 1;
	unsigned char ttxevs8   : 1;
	unsigned char ttxovs8   : 1;

	unsigned char res15     : 8;

	unsigned char ttxl12    : 1;
	unsigned char ttxl11    : 1;
	unsigned char ttxl10    : 1;
	unsigned char ttxl9     : 1;
	unsigned char ttxl8     : 1;
	unsigned char ttxl7     : 1;
	unsigned char ttxl6     : 1;
	unsigned char ttxl5     : 1;
	unsigned char ttxl20    : 1;
	unsigned char ttxl19    : 1;
	unsigned char ttxl18    : 1;
	unsigned char ttxl17    : 1;
	unsigned char ttxl16    : 1;
	unsigned char ttxl15    : 1;
	unsigned char ttxl14    : 1;
	unsigned char ttxl13    : 1;
} s_saa_data;

#define SAA_DATA_SIZE sizeof(s_saa_data)

static struct s_saa_data saa_data;	/* current settings */

///////////////////////////////////////////////////////////////////////////////

/* Addresses to scan */
static unsigned short normal_i2c[] 				= {I2C_CLIENT_END};
static unsigned short normal_i2c_range[] 	= { 0x88>>1,0x88>>1,I2C_CLIENT_END};
static unsigned short probe[2]        		= { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short probe_range[2]  		= { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short ignore[2]       		= { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short ignore_range[2] 		= { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short force[2]        		= { I2C_CLIENT_END, I2C_CLIENT_END };

static struct i2c_client_address_data addr_data = {
	normal_i2c, normal_i2c_range,
	probe, probe_range,
	ignore, ignore_range,
	force
};

/* ---------------------------------------------------
 * /proc entry declarations
 *----------------------------------------------------
 */

#ifdef CONFIG_PROC_FS

static int saa7126_proc_init(void);
static int saa7126_proc_cleanup(void);

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,3,27))
static void monitor_bus_saa7126(struct inode *inode, int fill);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,58)) */

static int read_bus_saa7126(char *buf, char **start, off_t offset, int len,
                           int *eof , void *private);

static int saa7126_proc_initialized;

#else /* undef CONFIG_PROC_FS */

#define saa7126_proc_init() 0
#define saa7126_proc_cleanup() 0

#endif /* CONFIG_PROC_FS */

int read_bus_saa7126(char *buf, char **start, off_t offset, int len, int *eof,
                 void *private);

static int saa7126_ioctl (struct inode *inode, struct file *file,
                         unsigned int cmd, unsigned long arg);
static int saa7126_open (struct inode *inode, struct file *file);

///////////////////////////////////////////////////////////////////////////////

static struct file_operations saa7126_fops = {
	owner:		THIS_MODULE,
	ioctl:		saa7126_ioctl,
	open:			saa7126_open,
};

///////////////////////////////////////////////////////////////////////////////

#define SAA7126S 							1
#define I2C_DRIVERID_SAA7126	1
#define SAA7126_MAJOR 				200

///////////////////////////////////////////////////////////////////////////////

static int debug =  0; /* insmod parameter */
static int addr  =  0;

#if LINUX_VERSION_CODE > 0x020100
MODULE_PARM(debug,"i");
MODULE_PARM(addr,"i");
#endif

///////////////////////////////////////////////////////////////////////////////

static int this_adap;

#define dprintk     if (debug) printk

#if LINUX_VERSION_CODE < 0x02017f
void schedule_timeout(int j)
{
	current->state   = TASK_INTERRUPTIBLE;
	current->timeout = jiffies + j;
	schedule();
}
#endif

struct saa7126
{
	int addr;
	unsigned char reg[128];

	int norm;
	int enable;
	int bright;
	int contrast;
	int hue;
	int sat;
};

static struct i2c_driver driver;
static struct i2c_client client_template;

struct saa7126type
{
	char *name;
	u8 Vendor;
	u8 Type;
};

static struct saa7126type saa7126[] = {
	{"SAA7126", 0, 0 }
};

/* ------------------------------------------------------------------------- */

int saa7126_proc_init(void)
{
	struct proc_dir_entry *proc_bus_saa7126;

	saa7126_proc_initialized = 0;

	if (! proc_bus) {
		printk("saa7126.o: /proc/bus/ does not exist");
		saa7126_proc_cleanup();
		return -ENOENT;
 	}

	proc_bus_saa7126 = create_proc_entry("saa7126",0,proc_bus);

	if (!proc_bus_saa7126) {
		printk("saa7126.o: Could not create /proc/bus/saa7126");
		saa7126_proc_cleanup();
		return -ENOENT;
 	}

	proc_bus_saa7126->read_proc = &read_bus_saa7126;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,27))
	proc_bus_saa7126->owner = THIS_MODULE;
#else
	proc_bus_saa7126->fill_inode = &monitor_bus_saa7126;
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,27)) */
	saa7126_proc_initialized += 2;
	return 0;
}

/* ----------------------------------------------------
 * The /proc functions
 * ----------------------------------------------------
 */

#ifdef CONFIG_PROC_FS

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,3,27))
/* Monitor access to /proc/bus/i2c*; make unloading i2c-proc impossible
   if some process still uses it or some file in it */
void monitor_bus_saa7126(struct inode *inode, int fill)
{
	if (fill)
		MOD_INC_USE_COUNT;
	else
		MOD_DEC_USE_COUNT;
}
#endif /* (LINUX_VERSION_CODE <= KERNEL_VERSION(2,3,37)) */

/* This function generates the output for /proc/bus/i2c */
int read_bus_saa7126(char *buf, char **start, off_t offset, int len, int *eof,
                 void *private)
{
	unsigned char reg[0x80];
	unsigned char tmp[0x02];
	int i;
	int nr = 0;

	for(i=0;i<0x80;i++)
	{
		buf[0] = i;
		if (1 != i2c_master_recv(&client_template,tmp,1))
			return 0;

		reg[i] = tmp[0];
	}

	nr = sprintf(buf,"SAA7126 configuration:\n");

	nr+= sprintf(buf+nr,"VERSION: %d\n", (reg[0]>>5)&0x0F);

	nr+= sprintf(buf+nr,"CCRDO: %d ", (reg[0]>>4)&0x01);
	nr+= sprintf(buf+nr,"CCRDE: %d ", (reg[0]>>3)&0x01);
	nr+= sprintf(buf+nr,"FSEQ:  %d ", (reg[0]>>1)&0x01);
	nr+= sprintf(buf+nr,"O_E:   %d\n", (reg[0])&0x01);

	nr+= sprintf(buf+nr,"WSS ON:   %d\n", (reg[0x27]>>7)&0x01);
	nr+= sprintf(buf+nr,"WSS BITS: %d\n", (reg[0x26]|((reg[0x27]&0x3F)<<8)));

	nr+= sprintf(buf+nr,"DECCOL: %d\n", (reg[0x28]>>7)&0x01);
	nr+= sprintf(buf+nr,"DECFIS: %d\n", (reg[0x28]>>6)&0x01);

	nr+= sprintf(buf+nr,"BURST START: %d (PAL:33 NTSC:25)n", (reg[0x28]&0x3F));
	nr+= sprintf(buf+nr,"BURST END:   %d (PAL/NTSC:29)\n", (reg[0x29]&0x3F));

	nr+= sprintf(buf+nr,"SRES: %d\n", (reg[0x29]>>7)&0x01);

	nr+= sprintf(buf+nr,"CGON: %d\n", (reg[0x2C]>>7)&0x01);
	nr+= sprintf(buf+nr,"CG: %d\n", (reg[0x2A]|(reg[0x2B]<<8)|((reg[0x2C]&0x0F)<<16)));

	nr+= sprintf(buf+nr,"VBSEN1: %d\n", (reg[0x2D]>>7)&0x01);
	nr+= sprintf(buf+nr,"VBSEN0: %d\n", (reg[0x2D]>>6)&0x01);
	nr+= sprintf(buf+nr,"CVBSEN: %d\n", (reg[0x2D]>>5)&0x01);
	nr+= sprintf(buf+nr,"CEN: %d\n", (reg[0x2D]>>4)&0x01);
	nr+= sprintf(buf+nr,"CVBSTRI: %d\n", (reg[0x2D]>>3)&0x01);
	nr+= sprintf(buf+nr,"RTRI: %d\n", (reg[0x2D]>>2)&0x01);
	nr+= sprintf(buf+nr,"GTRI: %d\n", (reg[0x2D]>>1)&0x01);
	nr+= sprintf(buf+nr,"BTRI: %d\n", (reg[0x2D])&0x01);

	nr+= sprintf(buf+nr,"GY: %d\n", (reg[0x38])&0x0F);
	nr+= sprintf(buf+nr,"GCD: %d\n", (reg[0x39])&0x0F);

	nr+= sprintf(buf+nr,"CBENB: %d\n", (reg[0x3A]>>7)&0x01);
	nr+= sprintf(buf+nr,"SYMB: %d\n", (reg[0x3A]>>4)&0x01);
	nr+= sprintf(buf+nr,"DEMOFF: %d\n", (reg[0x3A]>>3)&0x01);
	nr+= sprintf(buf+nr,"CSYNC: %d\n", (reg[0x3A]>>2)&0x01);
	nr+= sprintf(buf+nr,"MP2C2: %d\n", (reg[0x3A]>>1)&0x01);
	nr+= sprintf(buf+nr,"MP2C1: %d\n", (reg[0x3A])&0x01);

	nr+= sprintf(buf+nr,"VPSEN: %d\n", (reg[0x3B]>>7)&0x01);
	nr+= sprintf(buf+nr,"CCIRS: %d\n", (reg[0x3B]>>6)&0x01);

	nr+= sprintf(buf+nr,"EDGE2: %d\n", (reg[0x54]>>1)&0x01);
	nr+= sprintf(buf+nr,"EDGE1: %d\n", (reg[0x54])&0x01);

	nr+= sprintf(buf+nr,"VPS 05: %d\n", (reg[0x55])&0xFF);
	nr+= sprintf(buf+nr,"VPS 11: %d\n", (reg[0x56])&0xFF);
	nr+= sprintf(buf+nr,"VPS 12: %d\n", (reg[0x57])&0xFF);
	nr+= sprintf(buf+nr,"VPS 13: %d\n", (reg[0x58])&0xFF);
	nr+= sprintf(buf+nr,"VPS 14: %d\n", (reg[0x59])&0xFF);

	nr+= sprintf(buf+nr,"CHPS: %d\n", (reg[0x5A])&0xFF);
	nr+= sprintf(buf+nr,"GAINU: %d\n", (reg[0x5B] | ((reg[0x5D]&0x80)<<1) ));
	nr+= sprintf(buf+nr,"GAINV: %d\n", (reg[0x5C] | ((reg[0x5E]&0x80)<<1) ));

	nr+= sprintf(buf+nr,"DECOE: %d\n", (reg[0x5D]>>6)&0x01);
	nr+= sprintf(buf+nr,"BLCKL: %d\n", (reg[0x5D])&0x3F);

	nr+= sprintf(buf+nr,"DECPH: %d\n", (reg[0x5E]>>6)&0x01);
	nr+= sprintf(buf+nr,"BLNNL: %d\n", (reg[0x5E])&0x3F);

	nr+= sprintf(buf+nr,"CCR: %d\n", (reg[0x5F]>>6)&0x03);
	nr+= sprintf(buf+nr,"BLNVB: %d\n", (reg[0x5F])&0x3F);

	nr+= sprintf(buf+nr,"DOWNB: %d\n", (reg[0x61]>>7)&0x01);
	nr+= sprintf(buf+nr,"DOWNA: %d\n", (reg[0x61]>>6)&0x01);
	nr+= sprintf(buf+nr,"INPI: %d\n", (reg[0x61]>>5)&0x01);
	nr+= sprintf(buf+nr,"YGS: %d\n", (reg[0x61]>>4)&0x01);
	nr+= sprintf(buf+nr,"SCBW: %d\n", (reg[0x61]>>3)&0x01);
	nr+= sprintf(buf+nr,"PAL: %d\n", (reg[0x61]>>2)&0x01);
	nr+= sprintf(buf+nr,"FISE: %d\n", (reg[0x61])&0x01);

	nr+= sprintf(buf+nr,"RTCE: %d\n", (reg[0x62]>>7)&0x01);
	nr+= sprintf(buf+nr,"BSTA: %d\n", (reg[0x62])&0x7F);

	nr+= sprintf(buf+nr,"FSC 0: %d\n", (reg[0x63])&0xFF);
	nr+= sprintf(buf+nr,"FSC 1: %d\n", (reg[0x64])&0xFF);
	nr+= sprintf(buf+nr,"FSC 2: %d\n", (reg[0x65])&0xFF);
	nr+= sprintf(buf+nr,"FSC 3: %d\n", (reg[0x66])&0xFF);

	nr+= sprintf(buf+nr,"L21O 0: %d\n", (reg[0x67])&0xFF);
	nr+= sprintf(buf+nr,"L21O 1: %d\n", (reg[0x68])&0xFF);
	nr+= sprintf(buf+nr,"L21E 0: %d\n", (reg[0x69])&0xFF);
	nr+= sprintf(buf+nr,"L21E 1: %d\n", (reg[0x6A])&0xFF);

	nr+= sprintf(buf+nr,"SRCV11: %d\n", (reg[0x6B]>>7)&0x01);
	nr+= sprintf(buf+nr,"SRCV10: %d\n", (reg[0x6B]>>6)&0x01);
	nr+= sprintf(buf+nr,"TRCV2: %d\n", (reg[0x6B]>>5)&0x01);
	nr+= sprintf(buf+nr,"ORCV1: %d\n", (reg[0x6B]>>4)&0x01);
	nr+= sprintf(buf+nr,"PRCV1: %d\n", (reg[0x6B]>>3)&0x01);
	nr+= sprintf(buf+nr,"CBLF: %d\n", (reg[0x6B]>>2)&0x01);
	nr+= sprintf(buf+nr,"ORCV2: %d\n", (reg[0x6B]>>1)&0x01);
	nr+= sprintf(buf+nr,"PRCV2: %d\n", (reg[0x6B])&0x01);

	nr+= sprintf(buf+nr,"HTRIG: %d\n", (reg[0x6C] | (reg[0x6d]<<8)));

	nr+= sprintf(buf+nr,"SBLBN: %d\n", (reg[0x6E]>>7)&0x01);
	nr+= sprintf(buf+nr,"BLCKON: %d\n", (reg[0x6E]>>6)&0x01);
	nr+= sprintf(buf+nr,"PHRES1: %d\n", (reg[0x6E]>>5)&0x01);
	nr+= sprintf(buf+nr,"PHRES0: %d\n", (reg[0x6E]>>4)&0x01);
	nr+= sprintf(buf+nr,"LDEL1: %d\n", (reg[0x6E]>>3)&0x01);
	nr+= sprintf(buf+nr,"LDEL0: %d\n", (reg[0x6E]>>2)&0x01);
	nr+= sprintf(buf+nr,"FLC1: %d\n", (reg[0x6E]>>1)&0x01);
	nr+= sprintf(buf+nr,"FLC0: %d\n", (reg[0x6E])&0x01);

	nr+= sprintf(buf+nr,"CCEN1: %d\n", (reg[0x6f]>>7)&0x01);
	nr+= sprintf(buf+nr,"CCEN0: %d\n", (reg[0x6f]>>6)&0x01);
	nr+= sprintf(buf+nr,"TTXEN: %d\n", (reg[0x6f]>>5)&0x01);
	nr+= sprintf(buf+nr,"SCCLN: %d\n", (reg[0x6f])&0x1F);

	nr+= sprintf(buf+nr,"RCV2S: %d\n", (reg[0x70] | ((reg[0x72]&0x07)<<8)));
	nr+= sprintf(buf+nr,"RCV2E: %d\n", (reg[0x71] | ((reg[0x72]&0x70)<<4)));

	nr+= sprintf(buf+nr,"TTXHS: %d\n", (reg[0x73])&0xff);
	nr+= sprintf(buf+nr,"TTXHL: %d\n", (reg[0x74]>>4)&0x0f);
	nr+= sprintf(buf+nr,"TTXHD: %d\n", (reg[0x74])&0x0f);

	nr+= sprintf(buf+nr,"CSYNCA: %d\n", (reg[0x75]>>3)&0x0f);
	nr+= sprintf(buf+nr,"VS_S: %d\n", (reg[0x75])&0x07);

	nr+= sprintf(buf+nr,"TTXOVS: %03X ", reg[0x76] | ((reg[0x7c]&0x01)<<8));
	nr+= sprintf(buf+nr,"TTXOVE: %03X ", reg[0x77] | ((reg[0x7c]&0x02)<<7));
	nr+= sprintf(buf+nr,"TTXEVS: %03X ", reg[0x78] | ((reg[0x7c]&0x04)<<6));
	nr+= sprintf(buf+nr,"TTXEVE: %03X\n", reg[0x79] | ((reg[0x7c]&0x08)<<5));

	nr+= sprintf(buf+nr,"FAL: %d\n", (reg[0x7A] | ((reg[0x7C]&0x10)<<4)));
	nr+= sprintf(buf+nr,"LAL: %d\n", (reg[0x7B] | ((reg[0x7C]&0x40)<<2)));

	nr+= sprintf(buf+nr,"TTX60: %d\n", (reg[0x7E]&0x80)>>7);
	nr+= sprintf(buf+nr,"TTXO: %d\n", (reg[0x7E]&0x20)>>5);

	nr+= sprintf(buf+nr,"LINE: %d\n", (reg[0x7E])&0xFF);
	nr+= sprintf(buf+nr,"LINE: %d\n", (reg[0x7F])&0xFF);
/*
	for(i=0;i<0x80;i++)
		nr += sprintf(buf+nr,"%02X\n",reg[i]&0xff);
	nr += sprintf(buf+nr,"\n");
*/
	return nr;
}

int saa7126_proc_cleanup(void)
{

	if (saa7126_proc_initialized >= 1) {
		remove_proc_entry("saa7126",proc_bus);
		saa7126_proc_initialized -= 2;
	}
	return 0;
}

#endif /* def CONFIG_PROC_FS */

/* ----------------------------------------------------
 * the functional interface to the i2c busses.
 * ----------------------------------------------------
 */

///////////////////////////////////////////////////////////////////////////////

static int saa7126_getstatus(struct i2c_client *c)
{
	unsigned char byte;

	if (1 != i2c_master_recv(c,&byte,1))
		return 0;
	return byte;
}

static int saa7126_attach(struct i2c_adapter *adap, int addr,
			unsigned short flags, int kind)
{
	int i;
	char buf[2];
	struct saa7126 *t;
	struct i2c_client *client;

	dprintk("[SAA7126]: attach\n");

	if (this_adap > 0)
		return -1;
	this_adap++;
	
        client_template.adapter = adap;
        client_template.addr = addr;

        dprintk("saa7126: chip found @ 0x%x\n",addr);

        if (NULL == (client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
                return -ENOMEM;
        memcpy(client,&client_template,sizeof(struct i2c_client));
        client->data = t = kmalloc(sizeof(struct saa7126),GFP_KERNEL);
        if (NULL == t) {
                kfree(client);
                return -ENOMEM;
        }
        memset(t,0,sizeof(struct saa7126));
/*
	if (type >= 0 && type < SAA7126S) {
		t->type = type;
	strncpy(client->name, saa7126[t->type].name, sizeof(client->name));
	} else {
		t->type = -1;
	}
*/
	i2c_attach_client(client);
	//MOD_INC_USE_COUNT;

	// upload data
	for(i=0;i<0x80;i++)
	{
		buf[0] = i;
		buf[1] = PAL_SAA_NOKIA[i];
//		buf[1] = PAL_SAA_PHILIPS[i];
		i2c_master_send( client, buf, 2 );
	}

	for(i=0;i<0x80;i++)
	{
		buf[0] = i;
		buf[1] = PAL_SAA_NOKIA_CONFIG[i];
//		buf[1] = PAL_SAA_PHILIPS[i];
		i2c_master_send( client, buf, 2 );
	}

	memcpy( t->reg, PAL_SAA_NOKIA_CONFIG, 0x80 );

	return 0;
}

static int saa7126_probe(struct i2c_adapter *adap)
{
	int ret;

	ret = 0;

	dprintk("[saa7126]: probe\n");

	if (0 != addr) {
		normal_i2c_range[0] = addr;
		normal_i2c_range[1] = addr;
	}

	this_adap = 0;
	
	//if (adap->id == (I2C_ALGO_BIT | I2C_HW_B_BT848))

	if (1)
		ret = i2c_probe(adap, &addr_data, saa7126_attach );

	dprintk("[saa7126]: probe end %d\n",ret);

	return ret;
}

static int saa7126_detach(struct i2c_client *client)
{
	struct saa7126 *t = (struct saa7126*)client->data;

	dprintk("[Ssaa7126]: detach\n");

	i2c_detach_client(client);
	kfree(t);
	kfree(client);
	return 0;
}

static int saa7126_command(struct i2c_client *client, unsigned int cmd, void *arg )
{
	struct saa7126 *encoder = client->data;

	dprintk("[saa7126]: command\n");

	switch (cmd)	
	{
		case ENCODER_GET_CAPABILITIES:
			{
			struct video_encoder_capability *cap = arg;

			cap->flags
			    = VIDEO_ENCODER_PAL
			    | VIDEO_ENCODER_NTSC
			    | VIDEO_ENCODER_SECAM
			    | VIDEO_ENCODER_CCIR;
			cap->inputs = 1;
			cap->outputs = 1;
		}
		break;

	case ENCODER_SET_NORM:
		{
			int *iarg = arg;

			switch (*iarg) {

			case VIDEO_MODE_NTSC:
//				saa7185_write_block(encoder, init_ntsc, sizeof(init_ntsc));
				break;

			case VIDEO_MODE_PAL:
//				saa7185_write_block(encoder, init_pal, sizeof(init_pal));
				break;

			case VIDEO_MODE_SECAM:
			default:
				return -EINVAL;

			}
			encoder->norm = *iarg;
		}
		break;

	case ENCODER_SET_INPUT:
		{
			int *iarg = arg;

#if 0
			/* not much choice of inputs */
			if (*iarg != 0) {
				return -EINVAL;
			}
#else
			/* RJ: *iarg = 0: input is from SA7111
			   *iarg = 1: input is from ZR36060 */

			switch (*iarg) {

			case 0:
				/* Switch RTCE to 1 */
	//			saa7185_write(encoder, 0x61, (encoder->reg[0x61] & 0xf7) | 0x08);
				break;

			case 1:
				/* Switch RTCE to 0 */
//				saa7185_write(encoder, 0x61, (encoder->reg[0x61] & 0xf7) | 0x00);
				break;

			default:
				return -EINVAL;

			}
#endif
		}
		break;

	case ENCODER_SET_OUTPUT:
		{
			int *iarg = arg;

			/* not much choice of outputs */
			if (*iarg != 0) {
				return -EINVAL;
			}
		}
		break;

	case ENCODER_ENABLE_OUTPUT:
		{
			int *iarg = arg;

			encoder->enable = !!*iarg;
//			saa7185_write(encoder, 0x61, (encoder->reg[0x61] & 0xbf) | (encoder->enable ? 0x00 : 0x40));
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////

int saa7126_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
                  unsigned long arg)
{
	dprintk("[SAA7126]: IOCTL\n");
	
	switch (cmd)
	{
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////

int saa7126_open (struct inode *inode, struct file *file)
{
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

void inc_use (struct i2c_client *client)
{
#ifdef MODULE
        MOD_INC_USE_COUNT;
#endif
}

void dec_use (struct i2c_client *client)
{
#ifdef MODULE
        MOD_DEC_USE_COUNT;
#endif
}
static struct i2c_driver driver = {
        "i2c saa7126 driver",
        I2C_DRIVERID_SAA7126,
        I2C_DF_NOTIFY,
        saa7126_probe,
        saa7126_detach,
        saa7126_command,
        inc_use,
        dec_use,
};

static struct i2c_client client_template =
{
        "i2c saa7126 chip",          /* name       */
        I2C_DRIVERID_SAA7126,           /* ID         */
        0,
				0, /* interpret as 7Bit-Adr */
        NULL,
        &driver
};

EXPORT_NO_SYMBOLS;

#ifdef MODULE
int init_module(void)
#else
int i2c_scartswitch_init(void)
#endif
{
	i2c_add_driver(&driver);

	if (register_chrdev(SAA7126_MAJOR,"saa7126",&saa7126_fops))
	{
		printk("XXX.o: unable to get major %d\n", SAA7126_MAJOR);
		return -EIO;
	}

	saa7126_proc_init();

	return 0;
}

#ifdef MODULE
void cleanup_module(void)
{
	i2c_del_driver(&driver);

	if ((unregister_chrdev(SAA7126_MAJOR,"saa7126")))
	{
			printk("saa7126.o: unable to release major %d\n", SAA7126_MAJOR);
	}

	saa7126_proc_cleanup();
}
#endif

/*
 * Local variables:
 * c-indent-level: 8
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -8
 * c-argdecl-indent: 8
 * c-label-offset: -8
 * c-continued-statement-offset: 8
 * c-continued-brace-offset: 0
 * indent-tabs-mode: nil
 * tab-width: 8
 * End:
 */
