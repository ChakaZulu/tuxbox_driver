/*
 *   cxa2126.c - audio/video switch driver (dbox-II-project)
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
 *   $Log: cxa2126.c,v $
 *   Revision 1.2  2001/01/28 09:06:30  gillem
 *   some fixes
 *
 *   Revision 1.1.1.1  2001/01/23 00:16:37  gillem
 *   initial release
 *
 *
 *   $Revision: 1.2 $
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
#include "cxa2126.h"

/* Addresses to scan */
static unsigned short normal_i2c[] 		= {I2C_CLIENT_END};
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

/*
 * cxa2126 data struct
 *
 */

typedef struct s_avs_data {
 /* Data 1 */
 unsigned char evc : 3;
 unsigned char evf : 3;
 unsigned char tvmute1 : 1;
 unsigned char zcd : 1;
 /* Data 2 */
 unsigned char vo5mute : 1;
 unsigned char res1 : 1;
 unsigned char vsw1 : 3;
 unsigned char asw1 : 3;
 /* Data 3 */
 unsigned char res2 : 2;
 unsigned char vsw2 : 3;
 unsigned char asw2 : 3;
 /* Data 4 */
 unsigned char res3 : 8;
 /* Data 5 */
 unsigned char res4 : 2;
 unsigned char fblk : 3;
 unsigned char fnc  : 3;
 unsigned char log  : 1;
 /* Data 6 */
 unsigned char res5 : 2;
 unsigned char vo6on : 1;
 unsigned char vo5on : 1;
 unsigned char vo4on : 1;
 unsigned char vo3on : 1;
 unsigned char vo2on : 1;
 unsigned char vo1on : 1;
 /* Data 7 */
 unsigned char tvamute : 1;
 unsigned char tvagain : 1;
 unsigned char res6 : 1;
 unsigned char vcrmono : 1;
 unsigned char tvmono : 1;
 unsigned char res7 : 3;
} s_avs_data;

#define AVS_DATA_SIZE 7

///////////////////////////////////////////////////////////////////////////////

static int avs_ioctl (struct inode *inode, struct file *file,
                         unsigned int cmd, unsigned long arg);
static int avs_open (struct inode *inode, struct file *file);

static int avs_set(void);

///////////////////////////////////////////////////////////////////////////////

static struct file_operations avs_fops = {
	owner:		THIS_MODULE,
	ioctl:		avs_ioctl,
	open:		avs_open,
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

//static int avs_status;	/* current status */
static struct s_avs_data avs_data;	/* current settings */

/* ---------------------------------------------------------------------- */

struct avs_type
{
	char *name;
	u8 Vendor;
	u8 Type;
};

static struct avs_type avs_types[] = {
	{"CXA2126", 0, 0 }
};

/* ---------------------------------------------------------------------- */

int avs_set()
{
	if ( AVS_DATA_SIZE != i2c_master_send(&client_template, (char*)&avs_data, AVS_DATA_SIZE))
		return -EFAULT;

	return 0;
}

int avs_set_volume( int vol )
{
	int c=0,f=0;

	if ( vol<=63 ) {
        if (vol) {
    		c = vol/8;

            // check round :-/
            if ( (c*8) > vol )
                c--;

    		f = vol-(c*8);
        }
	} else {
		return -EINVAL;
	}

    avs_data.evc = c;
    avs_data.evf = f;

	return avs_set();
}

int avs_set_mute( int type )
{
	if ((type<0) || (type>3))
		return -EINVAL;

    avs_data.tvmute1 = (type>>1)&1;
    avs_data.zcd     = type&1;

	return avs_set();
}

int avs_set_zcd( int type )
{
	if ((type<0) || (type>1))
		return -EINVAL;

    avs_data.zcd = type&1;

	return avs_set();
}

int avs_set_fblk( int type )
{
	if (type<0 || type>3)
		return -EINVAL;

    avs_data.fblk = type;

	return avs_set();
}

int avs_set_fnc( int type )
{
	if (type<0 || type>3)
		return -EINVAL;

    avs_data.fnc = type;

	return avs_set();
}

int avs_set_vsw( int sw, int type )
{
	if (type<0 || type>7)
		return -EINVAL;

    switch(sw) {
        case 0:
            avs_data.vsw1 = type;
            break;
        case 1:
            avs_data.vsw2 = type;
            break;
//        case 2:
//            avs_data.vsw3 = type;
//            break;
        default:
    		return -EINVAL;
    }

	return avs_set();
}

int avs_set_asw( int sw, int type )
{
	if (type<0 || type>7)
		return -EINVAL;

    switch(sw) {
        case 0:
            avs_data.asw1 = type;
            break;
        case 1:
            avs_data.asw2 = type;
            break;
//        case 2:
//            avs_data.asw3 = type;
//            break;
        default:
    		return -EINVAL;
    }

	return avs_set();
}

int avs_get_volume(void)
{
    return ((avs_data.evc*8)+avs_data.evf);
}

int avs_get_mute(void)
{
    return avs_data.tvmute1;
}

int avs_get_zcd(void)
{
    return avs_data.zcd;
}

int avs_get_fblk(void)
{
    return avs_data.fblk;
}

int avs_get_fnc(void)
{
    return avs_data.fnc;
}

int avs_get_vsw( int sw )
{
    switch(sw) {
        case 0:
            return avs_data.vsw1;
            break;
        case 1:
            return avs_data.vsw2;
            break;
//        case 2:
//            return avs_data.vsw3;
//            break;
        default:
            return -EINVAL;
    }

    return -EINVAL;
}

int avs_get_asw( int sw )
{
    switch(sw) {
        case 0:
            return avs_data.asw1;
            break;
        case 1:
            return avs_data.asw2;
            break;
//        case 2:
//            return avs_data.asw3;
//            break;
        default:
            return -EINVAL;
    }

    return -EINVAL;
}

/* ---------------------------------------------------------------------- */

int avs_set_logic( int type )
{
	if(type<0 || type>1)
		return -EINVAL;

    avs_data.log = type;

	return avs_set();
}

int avs_get_logic(void)
{
    return avs_data.log;
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
	int ret=0;

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
	
	if (cmd&AVSIOSET)
	{
		if ( get_user(val,(int*)arg) )
			return -EFAULT;

		switch (cmd)
		{
			/* set video */
			case AVSIOSVSW1:	return avs_set_vsw(0,val);
			case AVSIOSVSW2:	return avs_set_vsw(1,val);
//			case AVSIOSVSW3:	return avs_set_vsw(2,val);
			/* set audio */
			case AVSIOSASW1:	return avs_set_asw(0,val);
			case AVSIOSASW2:	return avs_set_asw(1,val);
//			case AVSIOSASW3:	return avs_set_asw(2,val);
			/* set vol & mute */
			case AVSIOSVOL:		return avs_set_volume(val);
			case AVSIOSMUTE:	return avs_set_mute(val);
			/* set video fast blanking */
			case AVSIOSFBLK:	return avs_set_fblk(val);
			/* set video function switch control */
			case AVSIOSFNC:		return avs_set_fnc(val);
			/* set output throgh vout 8 */
//			case AVSIOSYCM:		return avs_set_ycm(val);
			/* set zero cross detector */
			case AVSIOSZCD:		return avs_set_zcd(val);
			/* set logic outputs */
			case AVSIOSLOG1:	return avs_set_logic(val);

			default:                return -EINVAL;
		}
	} else
	{
		switch (cmd)
		{
			/* get video */
			case AVSIOGVSW1:
                                val = avs_get_vsw(0);
                                break;
			case AVSIOGVSW2:
                                val = avs_get_vsw(1);
                                break;
			case AVSIOGVSW3:
                                val = avs_get_vsw(2);
                                break;
			/* get audio */
			case AVSIOGASW1:
                                val = avs_get_asw(0);
                                break;
			case AVSIOGASW2:
                                val = avs_get_asw(1);
                                break;
			case AVSIOGASW3:
                                val = avs_get_asw(2);
                                break;
			/* get vol & mute */
			case AVSIOGVOL:
                                val = avs_get_volume();
                                break;
			case AVSIOGMUTE:
                                val = avs_get_mute();
                                break;
			/* get video fast blanking */
			case AVSIOGFBLK:
                                val = avs_get_fblk();
                                break;
			/* get video function switch control */
			case AVSIOGFNC:
                                val = avs_get_fnc();
                                break;
			/* get output throgh vout 8 */
//			case AVSIOGYCM:
//                                val = avs_get_ycm();
//                                break;
			/* get zero cross detector */
			case AVSIOGZCD:
                                val = avs_get_zcd();
                                break;
			/* get logic outputs */
			case AVSIOGLOG1:
                                val = avs_get_logic();
                                break;
            case AVSIOGSTATUS:
                                // TODO: error handling
                                val = avs_getstatus(&client_template);
                                break;

			default:
                                return -EINVAL;
		}

        	return put_user(val,(int*)arg);
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
		printk("cxa2126.o: unable to get major %d\n", AVS_MAJOR);
		return -EIO;
	}

	memset((void*)&avs_data,0,AVS_DATA_SIZE);

    avs_data.vo6on = 1;
    avs_data.vo5on = 1;
    avs_data.vo4on = 1;
    avs_data.vo3on = 1;
    avs_data.vo2on = 1;
    avs_data.vo1on = 1;

	if ( AVS_DATA_SIZE != i2c_master_send(&client_template, (char*)&avs_data, AVS_DATA_SIZE)) {
		return -EFAULT;
    }

	return 0;
}

#ifdef MODULE
void cleanup_module(void)
{
	i2c_del_driver(&driver);

	if ((unregister_chrdev(AVS_MAJOR,"avs"))) {
		printk("cxa2126.o: unable to release major %d\n", AVS_MAJOR);
	}
}
#endif
