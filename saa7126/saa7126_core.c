/*
 * $Id: saa7126_core.c,v 1.49 2006/05/27 10:24:37 barf Exp $
 * 
 * Philips SAA7126 digital video encoder
 *
 * Copyright (C) 2000-2001 Gillem <htoa@gmx.net>
 * Copyright (C) 2002 Andreas Oberritter <obi@saftware.de>
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
 * References:
 * http://www.semiconductors.philips.com/acrobat/datasheets/SAA7126H_7127H_2.pdf
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/video_encoder.h>
#include <linux/videodev.h>

#include <dbox/saa7126_core.h>

#include <tuxbox/info_dbox2.h>

TUXBOX_INFO(dbox2_mid);

#ifndef CONFIG_DEVFS_FS
#error device filesystem required
#endif

#define dprintk if (0) printk



/*
 * id = (1 << (board_1 - 1)) | (1 << (board_n - 1))
 * reg = register address
 * len = buffer size
 * buf = data bytes
 */

struct saa7126_initdata {

	u8 id;
	u8 reg;
	u8 len;
	u8 buf[28];

} __attribute__ ((packed));


static struct saa7126_initdata saa7126_inittab_pal [] =
{
	{ 0x07, 0x26,  8, { 0x00, 0x00, 0x21, 0x1d, 0x00, 0x00, 0x00, 0x0f } },	/* common */

	/* gain luminance, gain colour difference */
	{ 0x01, 0x38,  3, { 0x1a, 0x1a, 0x03 } },				/* nokia, recommended */
	{ 0x02, 0x38,  3, { 0x1c, 0x1c, 0x03 } },				/* philips */
	{ 0x04, 0x38,  3, { 0x1e, 0x1e, 0x03 } },				/* sagem */

	{ 0x01, 0x54, 12, { 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6b, 0x7d,
			    0xaf, 0x33, 0x35, 0x35 } },				/* nokia */
	{ 0x02, 0x54, 12, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6b, 0x78,
			    0xab, 0x1b, 0x26, 0x26 } },				/* philips */
	{ 0x04, 0x54, 12, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6b, 0x75,
			    0xa5, 0x37, 0x39, 0x39 } },				/* sagem */

	{ 0x01, 0x61, 28, { 0x06, 0x2f, 0xcb, 0x8a, 0x09, 0x2a, 0x00, 0x00,
		 	    0x00, 0x00, 0x52, 0x28, 0x01, 0x20, 0x31, 0x7d,
			    0xbc, 0x60, 0x42, 0x05, 0x00, 0x05, 0x16, 0x04,
			    0x16, 0x16, 0x36, 0x60 } },				/* nokia */
	{ 0x02, 0x61, 28, { 0x16, 0x2f, 0xcb, 0x8a, 0x09, 0x2a, 0x00, 0x00,
			    0x00, 0x00, 0x72, 0x00, 0x00, 0xa0, 0x31, 0x7d,
			    0xbb, 0x60, 0x42, 0x07, 0x00, 0x05, 0x16, 0x04,
			    0x16, 0x16, 0x36, 0x60 } },				/* philips */
	{ 0x04, 0x61, 28, { 0x06, 0x2f, 0xcb, 0x8a, 0x09, 0x2a, 0x00, 0x00,
			    0x00, 0x00, 0x52, 0x6F, 0x00, 0xA0, 0x31, 0x7d,
			    0xbf, 0x60, 0x42, 0x07, 0x00, 0x05, 0x16, 0x04,
			    0x16, 0x16, 0x36, 0x60 } },				/* sagem */

	{ 0x07, 0x7d,  3, { 0x00, 0x00, 0x00 } },				/* common */

	{ 0xff },								/* end */
};


static struct saa7126_initdata saa7126_inittab_ntsc [] =
{
	{ 0x07, 0x26,  8, { 0x00, 0x00, 0x19, 0x1d, 0x00, 0x00, 0x00, 0x0f } },

	{ 0x01, 0x38,  3, { 0x1a, 0x1a, 0x03 } },
	{ 0x02, 0x38,  3, { 0x1c, 0x1c, 0x03 } },
	{ 0x04, 0x38,  3, { 0x1e, 0x1e, 0x03 } },

	{ 0x01, 0x54, 12, { 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa3, 0x7d,
			    0xaf, 0x33, 0x35, 0x35 } },
	{ 0x02, 0x54, 12, { 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa3, 0x78,
			    0xab, 0x1b, 0x26, 0x26 } },
	{ 0x04, 0x54, 12, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa3, 0x75,
			    0xa5, 0x37, 0x39, 0x39 } },

	{ 0x01, 0x61, 28, { 0x05, 0x43, 0x1f, 0x7c, 0xf0, 0x21, 0x00, 0x00,
		 	    0x00, 0x00, 0x52, 0x28, 0x01, 0x20, 0x31, 0x7d,
			    0xbc, 0x60, 0x54, 0x05, 0x00, 0x06, 0x10, 0x05,
			    0x10, 0x16, 0x36, 0xe0 } },
	{ 0x02, 0x61, 28, { 0x15, 0x43, 0x1f, 0x7c, 0xf0, 0x21, 0x00, 0x00,
			    0x00, 0x00, 0x72, 0x00, 0x00, 0xa0, 0x31, 0x7d,
			    0xb0, 0x60, 0x54, 0x07, 0x00, 0x06, 0x10, 0x05,
			    0x16, 0x16, 0x36, 0xe0 } },
	{ 0x04, 0x61, 28, { 0x05, 0x43, 0x1f, 0x7c, 0xf0, 0x21, 0x00, 0x00,
			    0x00, 0x00, 0x52, 0x6F, 0x00, 0xA0, 0x31, 0x7d,
			    0xbf, 0x60, 0x54, 0x07, 0x00, 0x06, 0x10, 0x05,
			    0x10, 0x16, 0x36, 0xe0 } },

	{ 0x07, 0x7e,  2, { 0x00, 0x00 } },

	{ 0xff },
};


/*
WSS-Bits
--------
for PAL-625 there are 8 combinations:

    Index  Bits  Aspectr Format Pos      ActiveLines RecommendedDisplay
    -----------------------------------------------------------------------------------------
	0 (01000)   4:3   full    -          576      centered, black side bars
	1 (00001)  14:9   lbx    center      504      centered, black side bars (or full)
	2 (00010)  14:9   lbx    top         504      centered, black side bars (or full)
	3 (01011)  16:9   lbx    center      430      full width
	4 (00100)  16:9   lbx    top         430      full width
	5 (01101) >16:9   lbx    center       -       full width (lbx'ed) or zoomed in (full)
	6 (01110)  14:9   full   center      576
	7 (00111)  16:9   anam    -          576

for NTSC-525 only bits 0 and 1 are used, rest is set to 0:

    Bit 0	1
    ---------------------
    b0  4:3	16:9
    b1  normal	letterbox

*/
static const unsigned char wss_data[8] =
{
	0x08, 0x01, 0x02, 0x0b, 0x04, 0x0d, 0x0e, 0x07
};



/*
 * i2c detection
 */

static unsigned short normal_i2c[] = { 0x44, I2C_CLIENT_END };
static unsigned short normal_i2c_range[] = { 0x44, 0x44, I2C_CLIENT_END };
I2C_CLIENT_INSMOD;



/*
 * saa device
 */

static int saa7126_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int saa7126_open(struct inode *inode, struct file *file);

static struct file_operations saa7126_fops = {
	owner:		THIS_MODULE,
	ioctl:		saa7126_ioctl,
	open:		saa7126_open,
};



/*
 * module parameters
 */

static int mode;
static int ntsc;



/*
 * device counter
 */

static int saa7126_id;



/*
 * invalid i2c driver id ;)
 */

#define I2C_DRIVERID_SAA7126	1



/*
 * maximum amount of bytes transfered at once
 */

#define SAA7126_I2C_BLOCK_SIZE	0x40



/*
 * private data struct for each saa
 */

struct saa7126
{
	devfs_handle_t devfs_handle;
	struct i2c_client *i2c_client;

	u8 standby;
	u8 reg_2d;
	u8 reg_3a;
	u8 reg_61;

	int norm;
	int enable;
};


static struct i2c_driver i2c_driver_saa7126;


static int saa7126_readbuf (struct i2c_client *client, char reg, char *buf, short len)
{
	int ret;
	struct i2c_msg msg [] = { { addr: client->addr, flags: 0, len: 1, buf: &reg },
			{ addr: client->addr, flags: I2C_M_RD, len: len, buf: buf } };

	dprintk("[%s]: %s\n", __FILE__, __FUNCTION__);
	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret != 2)
		printk("%s: i2c read error (ret == %d)\n", __FUNCTION__, ret);

	return 0;
}


static int saa7126_writebuf (struct i2c_client *client, u8 reg, u8 *data, u8 datalen)
{
	u8 buf[SAA7126_I2C_BLOCK_SIZE + 1];
	u8 len;
	u16 i;

	for (i = 0; i < datalen; i += len) {

		if ((datalen - i) >= (SAA7126_I2C_BLOCK_SIZE))
			len = SAA7126_I2C_BLOCK_SIZE;
		else
			len = datalen - i;
	
		buf[0] = reg + i;
		memcpy(buf + 1, data + i, len);
		i2c_master_send(client, buf, len + 1);
	}

	return 0;
}


static int saa7126_writereg(struct i2c_client *client, u8 reg, u8 val)
{
	char msg[2] = { reg, val };

	if (i2c_master_send(client, msg, 2) != 2)
		return -1;

	return 0;
}

static u8 saa7126_readreg(struct i2c_client *client, u8 reg)
{
	u8 val;

	saa7126_readbuf(client, reg, &val, 1);

	return val;
}


static int saa7126_write_inittab (struct i2c_client *client, char init)
{
	unsigned short i;
	struct saa7126 *encoder = (struct saa7126 *) client->data;
	struct saa7126_initdata *inittab;

	switch (encoder->norm) {
	case VIDEO_MODE_NTSC:
		inittab = saa7126_inittab_ntsc;
		break;
	case VIDEO_MODE_PAL:
		inittab = saa7126_inittab_pal;
		break;
	default:
		return -EINVAL;
	}

	/* init all null register with 0x00 */
	for (i = 0x01; i <= 0x25; i++  )
		saa7126_writereg(client, i, 0x00);
		
	for (i = 0x2e; i <= 0x37; i++ )
		saa7126_writereg(client, i, 0x00);

	saa7126_writereg(client, 0x60, 0x00);

	for (i = 0; inittab[i].id != 0xff; i++){
		if (inittab[i].id & (1 << (tuxbox_dbox2_mid - 1))){
			saa7126_writebuf(client, inittab[i].reg, inittab[i].buf, inittab[i].len);
			if (init){	/* need to init these two the first time */
				if (inittab[i].reg<=0x2d && (inittab[i].reg+inittab[i].len)>0x2d){
					encoder->reg_2d = inittab[i].buf[(0x2d-inittab[i].reg)];
				} else if (inittab[i].reg<=0x3a && (inittab[i].reg+inittab[i].len)>0x3a){
					encoder->reg_3a = inittab[i].buf[(0x3a-inittab[i].reg)];
				}
			}
		}
	}
	encoder->standby = 0;

	saa7126_writereg(client, 0x2d, encoder->reg_2d);
	saa7126_writereg(client, 0x3a, encoder->reg_3a);
	encoder->reg_61 = saa7126_readreg(client, 0x61);
	return 0;
}

#define REG_2D_RGB 	0x0f /* RGB + CVBS (for sync) */
#define REG_2D_FBAS	0x9e /* 10011110   */
#define REG_2D_SVIDEO	0xbf /* 10111111   */
#define REG_2D_YUV_V 	0xcf /* reg 2D = 11001111, all DAC's on, RGB + VBS */
#define REG_2D_YUV_C	0x8f /* reg 2D = 10001111, all DAC's on, RGB + CVBS */

static int saa7126_set_mode(struct i2c_client *client, int inp)
{
	struct saa7126 *encoder = (struct saa7126 *) client->data;
	encoder->reg_3a = 0x03;		/* by default switch YUV to RGB-matrix on */

	// reg_2D: VBS-EN1 VBS-EN0 CVBS-EN C-EN		CVBS-TRI R-TRI G-TRI B-TRI
	
	switch (inp) {
	case SAA_MODE_RGB:
		encoder->reg_2d = REG_2D_RGB;
		break;
	case SAA_MODE_FBAS:
		encoder->reg_2d = REG_2D_FBAS;
		break;
	case SAA_MODE_SVIDEO:
	  	encoder->reg_2d = REG_2D_SVIDEO;
		break;
	case SAA_MODE_YUV_V:
		encoder->reg_2d = REG_2D_YUV_V;
		encoder->reg_3a = 0x0b; /* reg 3A = 00001011, bypass RGB-matrix */
		break;
	case SAA_MODE_YUV_C:
		encoder->reg_2d = REG_2D_YUV_C;
		encoder->reg_3a = 0x0b; /* reg 3A = 00001011, bypass RGB-matrix */
		break;
	default:
		return -EINVAL;
	}

	saa7126_writereg(client, 0x2d, encoder->reg_2d);
	saa7126_writereg(client, 0x3a, encoder->reg_3a);
	return 0;
}

static int saa7126_get_mode(struct i2c_client *client)
{
	struct saa7126 *encoder = (struct saa7126 *) client->data;

	switch (encoder->reg_2d) {
	case REG_2D_RGB:
		return SAA_MODE_RGB;
		break;
#if REG_2D_FBAS != REG_2D_SVIDEO 
	case REG_2D_FBAS:
		return SAA_MODE_FBAS;
		break;
#endif
	case REG_2D_SVIDEO:
		return SAA_MODE_SVIDEO;
		break;
	case REG_2D_YUV_V:
		return SAA_MODE_YUV_V;
		break;
	case REG_2D_YUV_C:
		return SAA_MODE_YUV_C;
		break;
	default:
		return -EINVAL;
	}
}

static int saa7126_set_norm(struct i2c_client *client, u8 pal)
{
	struct saa7126 *encoder = (struct saa7126 *) client->data;

	if (pal)
		encoder->norm = VIDEO_MODE_PAL;
	else
		encoder->norm = VIDEO_MODE_NTSC;

	return saa7126_write_inittab(client,0);	/* not init */
}


static int saa7126_set_standby(struct i2c_client *client, u8 enable)
{
	struct saa7126 *encoder = (struct saa7126 *) client->data;

	if ((enable) && (!encoder->standby)) {

		/* write 2dh */
		saa7126_writereg(client, 0x2d, (encoder->reg_2d & 0xf0));
		/* write 61h */
		saa7126_writereg(client, 0x61, (encoder->reg_61 | 0xc0));

		encoder->standby = 1;

	}

	else if ((!enable) && (encoder->standby)) {

		/* write 2dh */
		saa7126_writereg(client, 0x2d, encoder->reg_2d);
		/* write 61h */
		saa7126_writereg(client, 0x61, encoder->reg_61);

		encoder->standby = 0;
	}

	else {
		return -EINVAL;
	}

	return 0;
}



/* -------------------------------------------------------------------------
 * the functional interface to the i2c busses
 * -------------------------------------------------------------------------
 */

static int saa7126_detect_client(struct i2c_adapter *adapter, int address,
		unsigned short flags, int kind)
{
	int ret;
	struct i2c_client *client;
	struct saa7126 *encoder;
	char devfs_name[10];

	dprintk("[%s]: %s\n", __FILE__, __FUNCTION__);

	client = kmalloc(sizeof(struct i2c_client) +
			sizeof(struct saa7126), GFP_KERNEL);

	if (!client)
		return -ENOMEM;

	sprintf(devfs_name, "dbox/saa%d", saa7126_id);

	client->data = client + 1;
	encoder = (struct saa7126 *) (client->data);
	memset(encoder, 0x00, sizeof(struct saa7126));
	
	client->addr = address;
	client->data = encoder;
	client->adapter = adapter;
	client->driver = &i2c_driver_saa7126;
	client->flags = 0;

#if 0
	if (kind < 0) {
		/* TODO: detect */
	}
#endif

	strcpy(client->name, "saa7126");
	client->id = saa7126_id++;

	encoder->i2c_client = client;
	encoder->enable = 1;

	if (ntsc)
		encoder->norm = VIDEO_MODE_NTSC;
	else
		encoder->norm = VIDEO_MODE_PAL;

	encoder->devfs_handle = devfs_register (NULL, devfs_name, DEVFS_FL_DEFAULT, 0, 0,
			S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
			&saa7126_fops, encoder);

	if (!encoder->devfs_handle) {
		kfree(client);
		return -EIO;
	}

	if ((ret = i2c_attach_client(client))) {
		kfree(client);
		return ret;
	}

	dprintk("[%s]: chip found @ 0x%x\n", __FILE__,  client->addr);

	saa7126_write_inittab(client, 1); /* init */
	/* set video mode */
	saa7126_set_mode(client, mode);

	return 0;
}

/* ------------------------------------------------------------------------- */

static int saa7126_attach(struct i2c_adapter *adapter)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif

	dprintk("[%s]: %s\n", __FILE__, __FUNCTION__);

	return i2c_probe(adapter, &addr_data, saa7126_detect_client);
}

/* ------------------------------------------------------------------------- */

static int saa7126_detach(struct i2c_client *client)
{
	int ret;
	struct saa7126 *encoder = (struct saa7126 *) client->data;

	dprintk("[%s]: %s\n", __FILE__, __FUNCTION__);

	devfs_unregister(encoder->devfs_handle);
	saa7126_id--;

	saa7126_set_standby(client, 1);

	if ((ret = i2c_detach_client(client))) {
		printk("[%s]: i2c_detach_client failed\n", __FILE__);
		return ret;
	}

	kfree(client);
	return 0;
}

/* ------------------------------------------------------------------------- */

static int saa7126_command(struct i2c_client *client, unsigned int cmd, void *parg)
{
	struct saa7126 *encoder = client->data;
	unsigned long arg = (unsigned long) parg;

	dprintk("[%s]: %s\n", __FILE__, __FUNCTION__);

	switch (cmd) {
	case ENCODER_GET_CAPABILITIES:{
		struct video_encoder_capability *cap = parg;
		cap->flags = VIDEO_ENCODER_PAL | VIDEO_ENCODER_NTSC;
		cap->inputs  = 1;
		cap->outputs = 1;
		break;
	}

	case ENCODER_SET_NORM:
		switch (arg) {
		case VIDEO_MODE_NTSC:
			saa7126_set_norm(client, 0);
		        saa7126_writereg(client, 0x2d, encoder->reg_2d);
        		saa7126_writereg(client, 0x3a, encoder->reg_3a);
			break;
		case VIDEO_MODE_PAL:
			saa7126_set_norm(client, 1);
		        saa7126_writereg(client, 0x2d, encoder->reg_2d);
        		saa7126_writereg(client, 0x3a, encoder->reg_3a);
			break;
		default:
			return -EINVAL;
		}
		break;

	case ENCODER_SET_INPUT:
		switch (arg) {
		case 0: /* avia */
			saa7126_writereg(client, 0x3a, 0x02);
			break;
		case 1: /* color bar */
			saa7126_writereg(client, 0x3a, 0x80);
			break;
		default:
			return -EINVAL;
		}
		break;

	case ENCODER_SET_OUTPUT:
		/* not much choice of outputs */
		if (arg != 0)
			return -EINVAL;
		break;

	case ENCODER_ENABLE_OUTPUT:
		encoder->enable = !!arg;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* ------------------------------------------------------------------------- */

static int saa7126_input_control(struct i2c_client *client, u8 inp)
{
	return saa7126_writereg(client, 0x3a, inp & 0x9f);
}

static int saa7126_output_control(struct i2c_client *client, u8 inp)
{
	return saa7126_writereg(client, 0x2d, inp);
}

/* ------------------------------------------------------------------------- */

static int saa7126_vps_set_data(struct i2c_client *client, char *buf)
{
	u8 reg_54;

	/* read vps status */
	reg_54 = saa7126_readreg(client, 0x54);

	if (buf[0] == 1) {
		reg_54 |= 0x80;
		i2c_master_send(client, buf, 5);
	}

	else if (buf[1] == 0) {
		reg_54 &= ~0x80;
	}

	else {
		return -EINVAL;
	}

	/* write vps status */
	saa7126_writereg(client, 0x54, reg_54);

	return 0;
}

static int saa7126_vps_get_data(struct i2c_client *client, char *buf)
{
	u8 reg_54;

	/* read vps status */
	reg_54 = saa7126_readreg(client, 0x54);

	if (reg_54 & 0x80)
		buf[0] = 1;
	else
		buf[0] = 0;

	/* read vps date */
	saa7126_readbuf(client, 0x54, buf + 1, 5);

	return 0;
}

/* ------------------------------------------------------------------------- */

static int saa7126_wss_get(struct i2c_client *client)
{
	if (saa7126_readreg(client, 0x27) & 0x80) {

		u8 i;
		u8 reg_26 = saa7126_readreg(client, 0x26) & 0x0f;

		for (i = 0; i < 8; i++)
			if (wss_data[i] == reg_26)
				return i;
	}

	return SAA_WSS_OFF;
}

static int saa7126_wss_set(struct i2c_client *client, int i)
{
	if (i == SAA_WSS_OFF) {
		saa7126_writereg(client, 0x27, 0x00);
		return 0;
	}

	if (i > 7)
		return -EINVAL;

	saa7126_writereg(client, 0x26, wss_data[i]);
	saa7126_writereg(client, 0x27, 0x80); // no WSS (c) bit set by default!

	return 0;
}

static int saa7126_csync_set(struct i2c_client *client, u8 val)
{
	u8 reg;
	if (val == 0) {
		reg = saa7126_readreg(client, 0x75);
		reg &= 0x07; /* Clear CSYNCA Bits */
		saa7126_writereg(client, 0x75, reg);
		reg = saa7126_readreg(client, 0x3A);
		reg &= ~0x04; /* Clear CSYNC Bit */
		saa7126_writereg(client, 0x3A, reg);
	}
	else if(val<32) {
		reg = saa7126_readreg(client, 0x75);
		reg = (reg & 0x07) | (val<<3); /* Set CSYNCA Bits */
		saa7126_writereg(client, 0x75, reg);
		reg = saa7126_readreg(client, 0x3A);
		reg |= 0x04; /* Set CSYNC Bit */
		saa7126_writereg(client, 0x3A, reg);
		return 0;
	}
	else
		return -EINVAL;

	return 0;
}

static int saa7126_csync_get(struct i2c_client *client)
{
	u8 reg;
	reg = saa7126_readreg(client, 0x3A);
	reg &= 0x04; /* GET CSYNC Bit */
	if (reg){
		reg = (saa7126_readreg(client, 0x75) | ~0x07) >> 3;
		return reg;
	} else
		return 0;
}

static int saa7126_ttx_set(struct i2c_client *client, u8 enable)
{
	u8 reg;
	reg = saa7126_readreg(client, 0x6f);

	if (enable)
		reg |= 0x20;
	else
		reg &= ~0x20;

	saa7126_writereg(client, 0x6f, reg);
	return 0;		
}

static int saa7126_ttx_get(struct i2c_client *client)
{
	return !!(saa7126_readreg(client, 0x6f)&0x20);
}

/* ------------------------------------------------------------------------- */

static int saa7126_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct saa7126 *encoder = (struct saa7126 *) file->private_data;
	struct i2c_client *client = (struct i2c_client *) encoder->i2c_client;

	char saa_data[SAA_DATA_SIZE];
	void *parg = (void *) arg;
	int val;

	dprintk("[%s]: %s\n", __FILE__, __FUNCTION__);

	/* no ioctl in power save mode */
	if ((encoder->standby) && (cmd != SAAIOSPOWERSAVE) && (cmd != SAAIOGPOWERSAVE))
		return -EINVAL;

	switch (cmd) {
	case SAAIOGREG:
		saa7126_readbuf(client, 0x00, saa_data, sizeof(saa_data));
		return copy_to_user((void *) arg, saa_data, sizeof(saa_data));

	case SAAIOSINP:
		if (copy_from_user(&val, parg, sizeof(val)))
			return -EFAULT;

		return saa7126_input_control(client, val);

	case SAAIOSOUT:
		if (copy_from_user(&val, parg, sizeof(val)))
			return -EFAULT;

		return saa7126_output_control(client, val);

	case SAAIOSMODE:
		if (copy_from_user(&val, parg, sizeof(val)))
			return -EFAULT;

		return saa7126_set_mode(client, val);

	case SAAIOGMODE:
		val = saa7126_get_mode(client);
		return (copy_to_user(parg, &val, sizeof(val)));
		
	case SAAIOSENC:
		if (copy_from_user(&val, parg, sizeof(val)))
			return -EFAULT;

		return saa7126_set_norm(client, val);

	case SAAIOSPOWERSAVE:
		if (copy_from_user(&val, parg, sizeof(val)))
			return -EFAULT;

		return saa7126_set_standby(client, val);

	case SAAIOGPOWERSAVE:
		return copy_to_user(parg, &encoder->standby, sizeof(encoder->standby));

	case SAAIOSVPSDATA:
		if (copy_from_user(saa_data, parg, 6))
			return -EFAULT;

		return saa7126_vps_set_data(client, saa_data);

	case SAAIOGVPSDATA:
		saa7126_vps_get_data(client, saa_data);
		return copy_to_user(parg, saa_data, 6);

	case SAAIOSWSS:
		if (copy_from_user(&val, parg,sizeof(val)))
			return -EFAULT;

		return saa7126_wss_set(client, val);

	case SAAIOGWSS:
		val = saa7126_wss_get(client);

		if (val < 0)
			return val;

		return copy_to_user(parg, &val, sizeof(val));

	case SAA_READREG:
		if (copy_from_user(&val, parg, sizeof(val)))
			return -EFAULT;

		val = saa7126_readreg(client, val);

		return (copy_to_user(parg, &val, sizeof(val)));

	case SAA_WRITEREG:
		if (copy_from_user(&val, parg, sizeof(val)))
			return -EFAULT;
		return saa7126_writereg(client, (val >> 8) & 0xff, val & 0xff);

	case SAAIOSCSYNC:
		if (copy_from_user(&val, parg, sizeof(val)))
				return -EFAULT;
		return saa7126_csync_set(client, val);

	case SAAIOGCSYNC:
		val = saa7126_csync_get(client);
		return (copy_to_user(parg, &val, sizeof(val)));
	
	case SAAIOSTTX:
		if (copy_from_user(&val, parg, sizeof(val)))
			return -EFAULT;
		return saa7126_ttx_set(client,val);

	case SAAIOGTTX:
		val = saa7126_ttx_get(client);
		return (copy_to_user(parg, &val, sizeof(val)));
		
	 default:
		return -EINVAL;
	}

	return 0;
}

/* ------------------------------------------------------------------------- */

static int saa7126_open (struct inode *inode, struct file *file)
{
	return 0;
}

/* ------------------------------------------------------------------------- */

static struct i2c_driver i2c_driver_saa7126 = {
	.name           = "saa7126",
	.id             = I2C_DRIVERID_SAA7126,
	.flags          = I2C_DF_NOTIFY,
	.attach_adapter = &saa7126_attach,
	.detach_client  = &saa7126_detach,
	.command        = &saa7126_command
};

/* ------------------------------------------------------------------------- */

int __init saa7126_init(void)
{
	i2c_add_driver(&i2c_driver_saa7126);

	return 0;
}

void __exit saa7126_exit(void)
{
	i2c_del_driver(&i2c_driver_saa7126);
}

module_init(saa7126_init);
module_exit(saa7126_exit);
MODULE_DESCRIPTION("SAA7126 digital PAL/NTSC encoder");
MODULE_AUTHOR("Gillem <htoa@gmx.net>, Andreas Oberritter <obi@saftware.de>");
MODULE_LICENSE("GPL");
MODULE_PARM(mode,"i");
MODULE_PARM(ntsc,"i");
