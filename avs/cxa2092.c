/*
 *   cxa2092.c - audio/video switch driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2001 Gillem htoa@gmx.net
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
 *   $Log: cxa2092.c,v $
 *   Revision 1.4  2001/01/15 17:02:32  gillem
 *   rewriten
 *
 *   Revision 1.3  2001/01/06 10:05:43  gillem
 *   cvs check
 *
 *   $Revision: 1.4 $
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/malloc.h>
#include <linux/poll.h>
#include <linux/types.h>

#include <linux/i2c.h>
#include "cxa2092.h"

/* Addresses to scan */
static unsigned short normal_i2c[] 				= {I2C_CLIENT_END};
static unsigned short normal_i2c_range[] 	= { 0x90>>1,0x90>>1,I2C_CLIENT_END};
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

///////////////////////////////////////////////////////////////////////////////

static int avs_ioctl (struct inode *inode, struct file *file,
                         unsigned int cmd, unsigned long arg);
static int avs_open (struct inode *inode, struct file *file);

static int avs_set(void);

///////////////////////////////////////////////////////////////////////////////

static struct file_operations avs_fops = {
	owner:		THIS_MODULE,
//	read:		lcd_read,
//	write:		lcd_write,
	ioctl:		avs_ioctl,
	open:			avs_open,
};

///////////////////////////////////////////////////////////////////////////////

#define AVS_COUNT					1
#define I2C_DRIVERID_AVS	1
#define AVS_MAJOR 				40

static int debug =  0; /* insmod parameter */
static int type  =  -1;
static int addr  =  0;

static int this_adap;

#define dprintk     if (debug) printk

#if LINUX_VERSION_CODE > 0x020100
MODULE_PARM(debug,"i");
MODULE_PARM(type,"i");
MODULE_PARM(addr,"i");
#endif

#if LINUX_VERSION_CODE < 0x02017f
void schedule_timeout(int j)
{
	current->state   = TASK_INTERRUPTIBLE;
	current->timeout = jiffies + j;
	schedule();
}
#endif

struct avs
{
	int type;           		/* chip type */
};

static struct i2c_driver driver;
static struct i2c_client client_template;

static int avs_status;	/* current status */
static unsigned char avs_data[5];	/* current settings */

/* ---------------------------------------------------------------------- */

struct avs_type
{
	char *name;
	u8 Vendor;
	u8 Type;
};

static struct avs_type avs_types[] = {
	{"CXA2092", 0, 0 }
};

/* ---------------------------------------------------------------------- */

void avs_set_volume( int vol )
{
	int c=0,f=0;

	if (vol&(vol<=56))
	{
		c = vol/8;
		f = vol-c;
	}

	SET_EVC(avs_data,c);
	SET_EVF(avs_data,f);
}

void avs_set_mute( int type )
{
	switch(type)
	{
		case AVS_MUTE_IM:
		case AVS_MUTE_ZC:
		case AVS_UNMUTE_IM:
		case AVS_UNMUTE_ZC:
			avs_data[0] = (avs_data[0]&(~3)) | type;
			break;
		default:
			break;
	}
}

void avs_set_fblk( int type )
{
	switch(type)
	{
		case AVS_FBLKOUT_NULL:
		case AVS_FBLKOUT_5V:
		case AVS_FBLKOUT_IN1:
		case AVS_FBLKOUT_IN2:
			avs_data[1] = (avs_data[1]&(~AVS_FBLK)) | (type<<6);
			break;
		default:
			break;
	}
}

void avs_set_fnc( int type )
{
	switch(type)
	{
		case AVS_FNCOUT_INTTV:
		case AVS_FNCOUT_EXT169:
		case AVS_FNCOUT_EXT43:
		case AVS_FNCOUT_EXT43_1:
			avs_data[2] = (avs_data[2]&(~AVS_FNC)) | (type<<6);
			break;
		default:
			break;
	}
}

void avs_set_ycm( int type )
{
	switch(type)
	{
		case 0:
		case 1:
			avs_data[3] = (avs_data[3]&(~AVS_YCM)) | (type<<7);
			break;
		default:
			break;
	}

}

void avs_set_vsw( int sw, int type )
{
	if(sw<0 || sw>2)
		return;

	switch(type)
	{
		case AVS_TVOUT_DE1:
		case AVS_TVOUT_DE2:
		case AVS_TVOUT_VCR:
		case AVS_TVOUT_AUX:
		case AVS_TVOUT_DE3:
		case AVS_TVOUT_DE4:
		case AVS_TVOUT_DE5:
		case AVS_TVOUT_VM1:
			avs_data[sw+1] = (avs_data[sw+1]&(~AVS_VSW1)) | (type<<3);
			break;
		default:
			break;
	}
}

void avs_set_asw( int sw, int type )
{
	if(sw<0 || sw>2)
		return;

	switch(type)
	{
		case AVS_AUXOUT_DE1:
		case AVS_AUXOUT_VM1:
		case AVS_AUXOUT_VCR:
		case AVS_AUXOUT_AUX:
		case AVS_AUXOUT_DE2:
		case AVS_AUXOUT_VM2:
		case AVS_AUXOUT_VM3:
		case AVS_AUXOUT_VM4:
			avs_data[sw+1] = (avs_data[sw+1]&(~AVS_ASW1)) | type;
			break;
		default:
			break;
	}
}

void avs_set_logic( int type )
{
	return;
}

int avs_set()
{
	if ( 5 != i2c_master_send(&client_template, avs_data, 5))
		return -EFAULT;

	return 0;
}
/* ---------------------------------------------------------------------- */

static int avs_getstatus(struct i2c_client *c)
{
	unsigned char byte;

	if (1 != i2c_master_recv(c,&byte,1))
		return -1;

	return byte;
}

/* ---------------------------------------------------------------------- */

static int avs_attach(struct i2c_adapter *adap, int addr,
			unsigned short flags, int kind)
{
	struct avs *t;
	struct i2c_client *client;

	dprintk("[AVS]: attach\n");

	if (this_adap > 0)
		return -1;

	this_adap++;
	
  client_template.adapter = adap;
  client_template.addr = addr;

  dprintk("[AVS]: chip found @ 0x%x\n",addr);

  if (NULL == (client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
		return -ENOMEM;

  memcpy(client,&client_template,sizeof(struct i2c_client));
  client->data = t = kmalloc(sizeof(struct avs),GFP_KERNEL);

  if (NULL == t) {
		kfree(client);
    return -ENOMEM;
	}

  memset(t,0,sizeof(struct avs));

	if (type >= 0 && type < AVS_COUNT) {
		t->type = type;
		strncpy(client->name, avs_types[t->type].name, sizeof(client->name));
	} else {
		t->type = -1;
	}

	i2c_attach_client(client);
	//MOD_INC_USE_COUNT;

	return 0;
}

static int avs_probe(struct i2c_adapter *adap)
{
	int ret;

	ret = 0;

	dprintk("[AVS]: probe\n");

	if (0 != addr) {
		normal_i2c_range[0] = addr;
		normal_i2c_range[1] = addr;
	}

	this_adap = 0;
	
	if (1)
		ret = i2c_probe(adap, &addr_data, avs_attach );

	dprintk("[AVS]: probe end %d\n",ret);

	return ret;
}

static int avs_detach(struct i2c_client *client)
{
	struct avs *t = (struct avs*)client->data;

	dprintk("[AVS]: detach\n");

	i2c_detach_client(client);
	kfree(t);
	kfree(client);
	return 0;
}

static int avs_command(struct i2c_client *client, unsigned int cmd, void *arg )
{
	dprintk("[AVS]: command\n");
	return 0;
}
///////////////////////////////////////////////////////////////////////////////

int avs_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
                  unsigned long arg)
{
	int val;
	dprintk("[AVS]: IOCTL\n");
	
	switch (cmd)
	{
		case AVSIOSVSW1:
			if ( get_user(val,(int*)arg) )
				return -EFAULT;
			avs_set_vsw(0,val);
			avs_set();
			break;
		case AVSIOSVSW2:
			if ( get_user(val,(int*)arg) )
				return -EFAULT;
			avs_set_vsw(1,val);
			avs_set();
			break;
		case AVSIOSVSW3:
			if ( get_user(val,(int*)arg) )
				return -EFAULT;
			avs_set_vsw(2,val);
			avs_set();
			break;
		case AVSIOSASW1:
			if ( get_user(val,(int*)arg) )
				return -EFAULT;
			avs_set_asw(0,val);
			avs_set();
			break;
		case AVSIOSASW2:
			if ( get_user(val,(int*)arg) )
				return -EFAULT;
			avs_set_asw(1,val);
			avs_set();
			break;
		case AVSIOSASW3:
			if ( get_user(val,(int*)arg) )
				return -EFAULT;
			avs_set_asw(2,val);
			avs_set();
			break;
		case AVSIOSVOL:
			if ( get_user(val,(int*)arg) )
				return -EFAULT;
			avs_set_volume(val);
			avs_set();
			break;
		case AVSIOSMUTE:
			if ( get_user(val,(int*)arg) )
				return -EFAULT;
			avs_set_mute(val);
			avs_set();
			break;

		case AVSIOGVSW1:
		case AVSIOGVSW2:
		case AVSIOGVSW3:
		case AVSIOGASW1:
		case AVSIOGASW2:
		case AVSIOGASW3:
		case AVSIOGVOL:
		case AVSIOGMUTE:
			break;

		default:
			return -EINVAL;
			break;
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////

int avs_open (struct inode *inode, struct file *file)
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
        "i2c audio/video switch driver",
        I2C_DRIVERID_AVS,
        I2C_DF_NOTIFY,
        avs_probe,
        avs_detach,
        avs_command,
        inc_use,
        dec_use,
};

static struct i2c_client client_template =
{
        "i2c audio/video switch chip",          /* name       */
        I2C_DRIVERID_AVS,           /* ID         */
        0,
				0, /* interpret as 7Bit-Adr */
        NULL,
        &driver
};

EXPORT_NO_SYMBOLS;

#ifdef MODULE
int init_module(void)
#else
int i2c_avs_init(void)
#endif
{
	i2c_add_driver(&driver);

	if (register_chrdev(AVS_MAJOR,"avs",&avs_fops)) {
		printk("cxa2092.o: unable to get major %d\n", AVS_MAJOR);
		return -EIO;
	}

	memset(avs_data,0,5);

	if ( 5 != i2c_master_send(&client_template, avs_data, 5))
		return -EFAULT;

//	avs_status = avs_getstatus(&client_template);

	return 0;
}

#ifdef MODULE
void cleanup_module(void)
{
	i2c_del_driver(&driver);

	if ((unregister_chrdev(AVS_MAJOR,"avs"))) {
			printk("cxa2092.o: unable to release major %d\n", AVS_MAJOR);
	}
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
