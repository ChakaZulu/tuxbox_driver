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
 *   Revision 1.15  2001/07/03 19:24:09  gillem
 *   - add stv6412
 *
 *   Revision 1.14  2001/06/24 08:24:24  gillem
 *   - some changes in nokia ostnet scart api
 *
 *   Revision 1.12  2001/05/26 09:20:09  gillem
 *   - add stv6412
 *
 *   Revision 1.11  2001/04/18 09:06:28  Jolt
 *   Small fixes
 *
 *   Revision 1.10  2001/04/16 21:39:20  Jolt
 *   Autodetect
 *
 *   Revision 1.9  2001/04/01 10:54:01  gillem
 *   - fix mixer device
 *
 *   Revision 1.8  2001/03/31 20:21:58  gillem
 *   - add mixer stuff
 *
 *   Revision 1.7  2001/03/25 14:06:45  gillem
 *   - update includes
 *
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
 *   $Revision: 1.15 $
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

#include "dbox/avs_core.h"
#include "dbox/info.h"
#include "mtdriver/scartApi.h"

#include "cxa2092.h"
#include "cxa2126.h"
#include "stv6412.h"

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

static int avs_mixerdev;

struct avs_type
{
	char *name;
	u8 Vendor;
	u8 Type;
};

static struct avs_type avs_types[] = {
	{"CXA2092", VENDOR_SONY, 				CXA2092 },
	{"CXA2126", VENDOR_SONY, 				CXA2126 },
	{"STV6412", VENDOR_STMICROELECTRONICS, 	STV6412 }
};

struct s_avs
{
	int type;		         	/* chip type */
	scartVolume volume;			/* nokia api controls */
	scartRGBLevel rgblevel;		/* nokia api controls */
	scartBypass bypass;			/* nokia api controls */
};

struct s_avs * avs_data;

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
static int type  = CXAAUTO;

/* --------------------------------------------------------------------- */

#ifdef MODULE
MODULE_PARM(debug,"i");
MODULE_PARM(addr,"i");
MODULE_PARM(type,"i");
#endif

/* ---------------------------------------------------------------------- */

static const struct {
  unsigned volidx:4;
  unsigned left:4;
  unsigned right:4;
  unsigned stereo:1;
  unsigned recmask:13;
  unsigned avail:1;
} mixtable[SOUND_MIXER_NRDEVICES] = {
  [SOUND_MIXER_VOLUME] = { 0, 0x0, 0x0, 0, 0x0000, 1 },   /* master */
};

static loff_t avs_llseek_mixdev(struct file *file, loff_t offset, int origin)
{
	return -ESPIPE;
}

static int mixer_ioctl(unsigned int cmd, unsigned long arg)
{
	unsigned int i,l;
	int val;

	if (_SIOC_DIR(cmd) == _SIOC_READ)
	{
		printk("mixer: READ\n");
		switch (_IOC_NR(cmd))
		{
			case SOUND_MIXER_RECSRC:
				printk("mixer: 1\n");
				return put_user(0,(int *)arg);
			case SOUND_MIXER_DEVMASK:
				printk("mixer: 2\n");
				return put_user(1,(int *)arg);
			case SOUND_MIXER_RECMASK:
				printk("mixer: 3\n");
				return put_user(0,(int *)arg);
			case SOUND_MIXER_STEREODEVS:
				printk("mixer: 4\n");
				return put_user(0,(int *)arg);
			case SOUND_MIXER_CAPS:
				printk("mixer: 5\n");
				return put_user(0,(int *)arg);
			case SOUND_MIXER_IMIX:
				return -EINVAL;
			default:
				i = _IOC_NR(cmd);

				if (i >= SOUND_MIXER_NRDEVICES || !mixtable[i].avail)
				{
					return -EINVAL;
				}

				switch (type)
				{
					case CXA2092:
						l=cxa2092_get_volume();
						break;
					case CXA2126:
						l=cxa2126_get_volume();
						break;
					default:
						return -EINVAL;
				}

				l = (((63-l)*100)/63);
				return put_user(l,(int *)arg);
		}
	}


	if (_SIOC_DIR(cmd) != (_SIOC_READ|_SIOC_WRITE))
	{
		return -EINVAL;
	}

	printk("mixer: write\n");
	switch (_IOC_NR(cmd))
	{
		case SOUND_MIXER_IMIX:
				return -EINVAL;
		case SOUND_MIXER_RECSRC:
			return -EINVAL;
		default:
			i = _IOC_NR(cmd);

			if (i >= SOUND_MIXER_NRDEVICES || !mixtable[i].avail)
			{
				return -EINVAL;
			}

			if(get_user(val, (int *)arg))
				return -EFAULT;

			l = val & 0xff;

			if (l > 100)
				l = 100;

			l = (63-((63*l)/100));

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

	return 0;
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

static struct file_operations avs_mixer_fops = {
	owner:		THIS_MODULE,
	llseek:		avs_llseek_mixdev,
	ioctl:		avs_ioctl_mixdev,
	open:		avs_open_mixdev,
	release:	avs_release_mixdev,
};

/* --------------------------------------------------------------------- */

static int avs_attach(struct i2c_adapter *adap, int addr,
			unsigned short flags, int kind)
{
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
    client->data = avs_data = kmalloc(sizeof(struct s_avs),GFP_KERNEL);

    if (NULL == avs_data)
	{
        kfree(client);
        return -ENOMEM;
    }

    memset(avs_data,0,sizeof(struct s_avs));

    if (type >= 0 && type < AVS_COUNT) {
	    avs_data->type = type;
	    strncpy(client->name, avs_types[avs_data->type].name, sizeof(client->name));
    } else {
	    avs_data->type = -1;
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

	switch (type)
	{
		case CXA2092:
			return cxa2092_command(client, cmd, arg );
			break;
		case CXA2126:
			return cxa2126_command(client, cmd, arg );
			break;
		case STV6412:
			return stv6412_command(client, cmd, arg );
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

/* ---------------------------------------------------------------------- */
/* nokia-ostnet scart api -> driver api wrapper */

int scart_command( unsigned int cmd, void *arg )
{
	dprintk("[SCART]: nokia-ostnet scart api\n");

	switch (cmd)
	{
		case SCART_VOLUME_SET:
		{
			scartVolume sVolume;

			if ( copy_from_user( &sVolume, arg, sizeof(scartVolume) ) )
			{
				return -EFAULT;
			}

			// ??? curVol ??? i'm not sure
			printk("vol: %d\n",sVolume.curVol);
			return avs_command( &client_template, AVSIOSVOL, &((scartVolume*)arg)->curVol );
		}
		case SCART_VOLUME_GET:
		{
			int err;
			int32_t value;			
			scartVolume sVolume;

			sVolume.minVol = 0;
			sVolume.maxVol = 63;
			sVolume.incVol = 1;

			if ( (err=avs_command( &client_template, AVSIOGVOL, &value )) )
			{
				return err;
			}

			sVolume.curVol = value;

			return copy_to_user( arg, &sVolume, sizeof(scartVolume) );
		}
		case SCART_MUTE_SET:
		{
			return avs_command( &client_template, AVSIOSMUTE, arg );
		}
		case SCART_MUTE_GET:
		{
			return avs_command( &client_template, AVSIOGMUTE, arg );
		}
		case SCART_AUD_FORMAT_SET:
		{
			int32_t value;

			if ( copy_from_user( &value, arg, sizeof(int32_t) ) )
			{
				return -EFAULT;
			}

			// TODO: set

			return 0;
		}
		case SCART_AUD_FORMAT_GET:
		{
			int32_t value;

			// TODO: get

			if ( copy_to_user( arg, &value, sizeof(int32_t) ) )
			{
				return -EFAULT;
			}

			return 0;
		}
		case SCART_VID_FORMAT_SET:
		{
			return avs_command( &client_template, AVSIOSFNC, arg );
		}
		case SCART_VID_FORMAT_GET:
		{
			return avs_command( &client_template, AVSIOGFNC, arg );
		}
		case SCART_VID_FORMAT_INPUT_GET:
		{
			int32_t value;

			// TODO: get

			if ( copy_to_user( arg, &value, sizeof(int32_t) ) )
			{
				return -EFAULT;
			}

			return 0;
		}
		case SCART_SLOW_SWITCH_SET:
		{
			return avs_command( &client_template, AVSIOSFBLK, arg );
		}
		case SCART_SLOW_SWITCH_GET:
		{
			return avs_command( &client_template, AVSIOGFBLK, arg );
		}
		case SCART_RGB_LEVEL_SET:
		{
			scartRGBLevel sRGBLevel;

			if ( copy_from_user( &sRGBLevel, arg, sizeof(scartRGBLevel) ) )
			{
				return -EFAULT;
			}

			// TODO: set

			return 0;
		}
		case SCART_RGB_LEVEL_GET:
		{
			scartRGBLevel sRGBLevel;

			// TODO: get

			if ( copy_to_user( arg, &sRGBLevel, sizeof(scartRGBLevel) ) )
			{
				return -EFAULT;
			}

			return 0;
		}
		case SCART_RGB_SWITCH_SET:
		{
			int32_t value;

			if ( copy_from_user( &value, arg, sizeof(int32_t) ) )
			{
				return -EFAULT;
			}

			return 0;
		}
		case SCART_RGB_SWITCH_GET:
		{
			int32_t value;

			// TODO: get

			if ( copy_to_user( arg, &value, sizeof(int32_t) ) )
			{
				return -EFAULT;
			}

			return 0;
		}
		case SCART_BYPASS_SET:
		{
			scartBypass sBypass;

			if ( copy_from_user( &sBypass, arg, sizeof(scartBypass) ) )
			{
				return -EFAULT;
			}

			if ( sBypass.ctrlCmd & SET_TV_SCART )
			{
			}

			if ( sBypass.ctrlCmd & SET_VCR_SCART )
			{
			}

			// TODO: set

			return 0;
		}
		case SCART_BYPASS_GET:
		{
			scartBypass sBypass;

			if ( copy_from_user( &sBypass, arg, sizeof(scartBypass) ) )
			{
				return -EFAULT;
			}

			if ( sBypass.ctrlCmd & SET_TV_SCART )
			{
			}

			if ( sBypass.ctrlCmd & SET_VCR_SCART )
			{
			}

			// TODO: get

			if ( copy_to_user( arg, &sBypass, sizeof(scartBypass) ) )
			{
				return -EFAULT;
			}

			return 0;
		}

		default:
			return -EOPNOTSUPP;
	}

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
		return avs_command( &client_template, cmd, (void*)arg );
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

/* ------------------------------------------------------------------------- */

EXPORT_SYMBOL(scart_command);

/* ---------------------------------------------------------------------- */

#ifdef MODULE
int init_module(void)
#else
int i2c_avs_init(void)
#endif
{

	struct dbox_info_struct dinfo;
	
	if (type == CXAAUTO) {
	
	    dbox_get_info(&dinfo);
	    
		switch(dinfo.mID)
		{
			case DBOX_MID_SAGEM:
				type = CXA2126;
				break;
			case DBOX_MID_PHILIPS:
				type = STV6412;
				break;
			default: // DBOX_MID_NOKIA
				type = CXA2092;
				break;
		}
	}
	
	i2c_add_driver(&driver);

	switch(type)
	{
		case CXA2092:
			cxa2092_init(&client_template);
			break;
		case CXA2126:
			cxa2126_init(&client_template);
			break;
		case STV6412:
			stv6412_init(&client_template);
			break;
		default:
			printk("[AVS]: wrong type %d\n", type);
			i2c_del_driver(&driver);
			return -EIO;
	}

	devfs_handle = devfs_register ( NULL, "dbox/avs0", DEVFS_FL_DEFAULT,
								0, 0,
								S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
								&avs_fops, NULL );

	if ( ! devfs_handle )
	{
		i2c_del_driver ( &driver );
		return -EIO;
	}

	avs_mixerdev=register_sound_mixer(&avs_mixer_fops, -1);

	return 0;
}

#ifdef MODULE
void cleanup_module(void)
{
	unregister_sound_mixer(avs_mixerdev);

	i2c_del_driver(&driver);

	devfs_unregister ( devfs_handle );
}
#endif

/* ---------------------------------------------------------------------- */
