/*
    cxa2092.c - dbox-II-project

    Copyright (C) 2000 Gillem htoa@gmx.net

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

static int scartswitch_ioctl (struct inode *inode, struct file *file,
                         unsigned int cmd, unsigned long arg);
static int scartswitch_open (struct inode *inode, struct file *file);

///////////////////////////////////////////////////////////////////////////////

static struct file_operations scartswitch_fops = {
	owner:		THIS_MODULE,
//	read:		lcd_read,
//	write:		lcd_write,
	ioctl:		scartswitch_ioctl,
	open:		scartswitch_open,
};

///////////////////////////////////////////////////////////////////////////////

#define I2C_DRIVERID_SCARTSWITCH	1
#define SCARTSWITCH_MAJOR 				40

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

struct scartswitch
{
	int type;           /* chip type */
	int data1;					/* current settings */
	int data2;
	int data3;
	int data4;
};

static struct i2c_driver driver;
static struct i2c_client client_template;

/* ---------------------------------------------------------------------- */

struct scartswitchtype
{
	char *name;
	u8 Vendor;
	u8 Type;
};

static struct scartswitchtype scartswitch[] = {
	{"CXA2092", 0, 0 }
};

/* ---------------------------------------------------------------------- */

static int scartswitch_getstatus(struct i2c_client *c)
{
	unsigned char byte;

	if (1 != i2c_master_recv(c,&byte,1))
		return 0;
	return byte;
}

/* ---------------------------------------------------------------------- */

static int scartswitch_attach(struct i2c_adapter *adap, int addr,
			unsigned short flags, int kind)
{
	struct scartswitch *t;
	struct i2c_client *client;

	dprintk("[SCSW]: attach\n");

	if (this_adap > 0)
		return -1;
	this_adap++;
	
        client_template.adapter = adap;
        client_template.addr = addr;

        dprintk("scartswitch: chip found @ 0x%x\n",addr);

        if (NULL == (client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
                return -ENOMEM;
        memcpy(client,&client_template,sizeof(struct i2c_client));
        client->data = t = kmalloc(sizeof(struct scartswitch),GFP_KERNEL);
        if (NULL == t) {
                kfree(client);
                return -ENOMEM;
        }
        memset(t,0,sizeof(struct scartswitch));

	if (type >= 0 && type < SCARTSWITCHES) {
		t->type = type;
		strncpy(client->name, scartswitch[t->type].name, sizeof(client->name));
	} else {
		t->type = -1;
	}
        i2c_attach_client(client);
	//MOD_INC_USE_COUNT;

	return 0;
}

static int scartswitch_probe(struct i2c_adapter *adap)
{
	int ret;

	ret = 0;

	dprintk("[SCSW]: probe\n");

	if (0 != addr) {
		normal_i2c_range[0] = addr;
		normal_i2c_range[1] = addr;
	}

	this_adap = 0;
	
	//if (adap->id == (I2C_ALGO_BIT | I2C_HW_B_BT848))

	if (1)
		ret = i2c_probe(adap, &addr_data, scartswitch_attach );

	dprintk("[SCSW]: probe end %d\n",ret);

	return ret;
}

static int scartswitch_detach(struct i2c_client *client)
{
	struct scartswitch *t = (struct scartswitch*)client->data;

	dprintk("[SCSW]: detach\n");

	i2c_detach_client(client);
	kfree(t);
	kfree(client);
	return 0;
}

static int scartswitch_command(struct i2c_client *client, unsigned int cmd, void *arg )
{
	dprintk("[SCSW]: command\n");
	return 0;
}
///////////////////////////////////////////////////////////////////////////////

int scartswitch_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
                  unsigned long arg)
{
	int i;
	static unsigned char buf[5];
  struct scsw_switch *scsw_sw;
	char sw_buf[] = { 0x0d,0x29,0xc9,0xa9,0x00 };

	dprintk("[SCSW]: IOCTL\n");
	
	switch (cmd)
	{
		case IOCTL_WRITE_CONTROL_REG:
			if ( copy_from_user( buf, (unsigned char*)arg, 5 ) )
				return -EFAULT;

			dprintk("[SCSW]: send\n");

			if ( 5 != i2c_master_send(&client_template, buf ,5))
			{
				return -EFAULT;
			}

			dprintk("[SCSW]: send ready \n");

			break;

		case IOCTL_READ_STATUS:

			buf[0] = 0;
			buf[1] = 0;

			if ( 2 != i2c_master_recv(&client_template, buf ,2))
			{
				return -EFAULT;
			}

			i = buf[0]&0xFF;

			if ( copy_to_user( (int*)arg, &i, sizeof(int) ) )
				return -EFAULT;

			break;

		case IOCTL_SWITCH:
			scsw_sw = (scsw_switch*)arg;
			
			if (scsw_sw->out==SCSW_OUT1)
				sw_buf[1] = scsw_sw->inp;
			else
				sw_buf[2] = scsw_sw->inp;

			if ( 5 != i2c_master_recv(&client_template, sw_buf ,5))
			{
				return -EFAULT;
			}

			break;
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////

int scartswitch_open (struct inode *inode, struct file *file)
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
        "i2c scart switch driver",
        I2C_DRIVERID_SCARTSWITCH,
        I2C_DF_NOTIFY,
        scartswitch_probe,
        scartswitch_detach,
        scartswitch_command,
        inc_use,
        dec_use,
};

static struct i2c_client client_template =
{
        "i2c scart switch chip",          /* name       */
        I2C_DRIVERID_SCARTSWITCH,           /* ID         */
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
	int i;

	/* shameless stolen from br ;-) */

/* ADDR
 * 0    ???
 * 1    SCART 1
 * 2    SCART 2
 * 3    ???
 * 4    ???
 */
	char buf[] = {
	0x0d,0x29,0xc9,0xa9,0x00,
	0x0f,0x29,0xc9,0xa9,0x80,
	0x0f,0x2f,0xcf,0xaf,0x80,
	0x0f,0x29,0xc9,0xa9,0x80,
	0x0f,0x29,0xc9,0xa9,0x00};

	i2c_add_driver(&driver);

	if (register_chrdev(SCARTSWITCH_MAJOR,"scartswitch",&scartswitch_fops)) {
		printk("cxa2092.o: unable to get major %d\n", SCARTSWITCH_MAJOR);
		return -EIO;
	}

  for(i=0;i<5;i++)
	{
		if ( 5 != i2c_master_send(&client_template, buf+(i*5),5))
		{
			return -EFAULT;
		}
	}

	return 0;
}

#ifdef MODULE
void cleanup_module(void)
{
	i2c_del_driver(&driver);

	if ((unregister_chrdev(SCARTSWITCH_MAJOR,"scartswitch"))) {
			printk("cxa2092.o: unable to release major %d\n", SCARTSWITCH_MAJOR);
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
