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
 *   Revision 1.6  2001/03/16 20:49:21  gillem
 *   - fix errors
 *
 *   Revision 1.5  2001/03/15 22:20:23  Hunz
 *   nothing important...
 *
 *   Revision 1.4  2001/03/03 17:13:37  waldi
 *   complete move to devfs; doesn't compile without devfs
 *
 *   Revision 1.3  2001/03/03 11:09:21  gillem
 *   - bugfix
 *
 *   Revision 1.2  2001/03/03 11:02:57  gillem
 *   - cleanup
 *
 *   Revision 1.1  2001/03/03 09:38:58  gillem
 *   - initial release
 *
 *
 *   $Revision: 1.6 $
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
#include <linux/sound.h>
#include <linux/soundcard.h>

#include "avs_core.h"

#include "cxa2092.h"
#include "cxa2126.h"

#include <linux/devfs_fs_kernel.h>

#ifndef CONFIG_DEVFS_FS
#error no devfs
#endif

static devfs_handle_t devfs_handle;

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
	{"CXA2126", 0, 1 }
};

struct avs
{
	int type;           		/* chip type */
};

/* ---------------------------------------------------------------------- */

static int avs_ioctl (struct inode *inode, struct file *file,
                         unsigned int cmd, unsigned long arg);
static int avs_open (struct inode *inode, struct file *file);

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

/* --------------------------------------------------------------------- */

#ifdef MODULE
MODULE_PARM(debug,"i");
MODULE_PARM(addr,"i");
MODULE_PARM(type,"i");
#endif

/* ---------------------------------------------------------------------- */

/* TODO -Hunz

static loff_t avs_llseek_mixdev(struct file *file, loff_t offset, int origin)
{
	return -ESPIPE;
}

static int mixer_ioctl(unsigned int cmd, unsigned long arg)
{
  unsigned int i,l;
  int val;

  if (_SIOC_DIR(cmd) == _SIOC_READ) {
    switch (_IOC_NR(cmd)) {
    case SOUND_MIXER_RECSRC:
      return put_user(0,(int *)arg);
    case SOUND_MIXER_DEVMASK:
      return put_user(1,(int *)arg);
    case SOUND_MIXER_RECMASK:
      return put_user(0,(int *)arg);
    case SOUND_MIXER_STEREODEVS:
      return put_user(0,(int *)arg);
    case SOUND_MIXER_CAPS:
      return put_user(0,(int *)arg);
    case SOUND_MIXER_IMIX:
      return put_user(0,(int *)arg);
    default:
      switch (type)
      {
      case CXA2092:
	l=cxa2092_get_volume(&client_template );
	break;
      case CXA2126:
	l=cxa2126_get_volume(&client_template );
	break;
      default:
	return -EINVAL;
      }
      l=(100/63)*(63-l);
      return put_user(l,(int *)arg);
    }
  }
  if (_SIOC_DIR(cmd) != (_SIOC_READ|_SIOC_WRITE)) 
    return -EINVAL;
  switch (_IOC_NR(cmd)) {
    
  case SOUND_MIXER_IMIX:
    return -EINVAL;
    
  case SOUND_MIXER_RECSRC:
    return -EINVAL;
    
  default:
    i = _IOC_NR(cmd);
    if (i > 1)
      return -EINVAL;
    if(get_user(val, (int *)arg))
      return -EFAULT;
    l = val & 0xff;
    if (l > 100)
      l = 100;
    l=63-(int)((63/100)*l);
    switch (type)
      {
      case CXA2092:
	return cxa2092_set_volume(&client_template, l );
	break;
      case CXA2126:
	return cxa2126_set_volume(&client_template, l );
	break;
      default:
	return -EINVAL;
      }
  }
}

static int avs_open_mixdev(struct inode *inode, struct file *file)
{

  	MOD_INC_USE_COUNT;
	return 0;
}

static int avs_release_mixdev(struct inode *inode, struct file *file)
{
  	MOD_DEC_USE_COUNT;
	return 0;
}

static int avs_ioctl_mixdev(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	return mixer_ioctl(cmd, arg);
}

static int avs_mixerdev;

static struct file_operations avs_mixer_fops = {
	&avs_llseek_mixdev,
	NULL,
	NULL,
	NULL,
	NULL,
	&avs_ioctl_mixdev,
	NULL,
	&avs_open_mixdev,
	NULL,
	&avs_release_mixdev,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};
*/

/* --------------------------------------------------------------------- */

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
				return cxa2126_command(&client_template, cmd, (void*)arg );
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

//	if (register_chrdev(AVS_MAJOR,"avs",&avs_fops))
//	{
//		printk("[AVS]: unable to get major %d\n", AVS_MAJOR);
//		i2c_del_driver(&driver);
//		return -EIO;
//	}

	devfs_handle = devfs_register ( NULL, "dbox/avs0", DEVFS_FL_DEFAULT,
                                  0, 0,
                                  S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
                                  &avs_fops, NULL );

	if ( ! devfs_handle )
	{
		i2c_del_driver ( &driver );
		return -EIO;
	}

  //  avs_mixerdev=register_sound_mixer(&avs_mixer_fops, -1);

	return 0;
}

#ifdef MODULE
void cleanup_module(void)
{
  // unregister_sound_mixer(avs_mixerdev);

	i2c_del_driver(&driver);

	devfs_unregister ( devfs_handle );
}
#endif

/* ---------------------------------------------------------------------- */
