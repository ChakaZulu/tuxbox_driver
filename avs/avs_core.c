/*
 *   avs_core.c - audio/video switch core driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2001 Gillem htoa@gmx.net
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
 *   $Log: avs_core.c,v $
 *   Revision 1.2  2001/03/03 11:02:57  gillem
 *   - cleanup
 *
 *   Revision 1.1  2001/03/03 09:38:58  gillem
 *   - initial release
 *
 *
 *   $Revision: 1.2 $
 *
 */

/* ---------------------------------------------------------------------- */

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

#include "avs_core.h"

#include "cxa2092.h"
#include "cxa2126.h"

/* ---------------------------------------------------------------------- */

/* Addresses to scan */
static unsigned short normal_i2c[] 			= {I2C_CLIENT_END};
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

static struct i2c_driver driver;
static struct i2c_client client_template;

static int this_adap;

struct avs_type
{
	char *name;
	u8 Vendor;
	u8 Type;
};

static struct avs_type avs_types[] = {
	{"CXA2092", 0, 0 },
	{"CXA2126", 0, 0 }
};

struct avs
{
	int type;           		/* chip type */
};

/* ---------------------------------------------------------------------- */

static int avs_ioctl (struct inode *inode, struct file *file,
                         unsigned int cmd, unsigned long arg);
static int avs_open (struct inode *inode, struct file *file);

static int avs_set(void);

/* ---------------------------------------------------------------------- */

static struct file_operations avs_fops = {
	owner:		THIS_MODULE,
	ioctl:		avs_ioctl,
	open:		avs_open,
};

/* ---------------------------------------------------------------------- */

#define dprintk     if (debug) printk

static int debug = 0;
static int addr  = 0;
static int type  = CXA2092;

#ifdef MODULE
MODULE_PARM(debug,"i");
MODULE_PARM(addr,"i");
MODULE_PARM(type,"i");
#endif

/* ---------------------------------------------------------------------- */

static int avs_attach(struct i2c_adapter *adap, int addr,
			unsigned short flags, int kind)
{
    struct avs *t;
    struct i2c_client *client;

    dprintk("[AVS]: attach\n");

    if (this_adap > 0)
	{
        return -1;
    }

    this_adap++;
	
    client_template.adapter = adap;
    client_template.addr = addr;

    dprintk("[AVS]: chip found @ 0x%x\n",addr);

    if (NULL == (client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
	{
        return -ENOMEM;
    }

    memcpy(client,&client_template,sizeof(struct i2c_client));
    client->data = t = kmalloc(sizeof(struct avs),GFP_KERNEL);

    if (NULL == t)
	{
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
	int ret=0;

	dprintk("[AVS]: probe\n");

	if (0 != addr)
	{
		normal_i2c_range[0] = addr;
		normal_i2c_range[1] = addr;
	}

	this_adap = 0;
	
	if (1)
	{
		ret = i2c_probe(adap, &addr_data, avs_attach );
    }

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

/* ---------------------------------------------------------------------- */

int avs_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
                  unsigned long arg)
{
	dprintk("[AVS]: IOCTL\n");

	if ( cmd == AVSIOGTYPE )
	{
		return put_user(type,(int*)arg);
	}
	else
	{
		switch (type)
		{
			case CXA2092:
				return cxa2092_command(&client_template, cmd, (void*)arg );
				break;
			case CXA2126:
				return cxa2092_command(&client_template, cmd, (void*)arg );
				break;
			default:
				return -EINVAL;
		}
	}

	return 0;
}

/* ---------------------------------------------------------------------- */

int avs_open (struct inode *inode, struct file *file)
{
	return 0;
}

/* ---------------------------------------------------------------------- */

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

/* ---------------------------------------------------------------------- */

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

/* ---------------------------------------------------------------------- */

#ifdef MODULE
int init_module(void)
#else
int i2c_avs_init(void)
#endif
{
	i2c_add_driver(&driver);

	switch(type)
	{
		case CXA2092:
			cxa2092_init(&client_template);
			break;
		case CXA2126:
			cxa2126_init(&client_template);
			break;
		default:
			printk("[AVS]: wrong type %d\n", type);
			i2c_del_driver(&driver);
			return -EIO;
	}

	if (register_chrdev(AVS_MAJOR,"avs",&avs_fops))
	{
		printk("[AVS]: unable to get major %d\n", AVS_MAJOR);
		i2c_del_driver(&driver);
		return -EIO;
	}

	return 0;
}

#ifdef MODULE
void cleanup_module(void)
{
	i2c_del_driver(&driver);

	if ((unregister_chrdev(AVS_MAJOR,"avs"))) {
		printk("[AVS]: unable to release major %d\n", AVS_MAJOR);
	}
}
#endif

/* ---------------------------------------------------------------------- */
