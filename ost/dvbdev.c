/* 
 * dvbdev.c
 *
 * Copyright (C) 2000 Ralph  Metzler <ralph@convergence.de>
 *                  & Marcus Metzler <marcus@convergence.de>
                      for convergence integrated media GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/system.h>

#include <linux/kmod.h>

#include <ost/dvbdev.h>

static struct dvb_device *dvb_device[DVB_NUM_DEVICES];

#define INFU 32768

static int max_users[DVB_NUM_SUB_DEVICES] = { INFU, INFU, INFU, INFU, INFU, 1 };
static int max_writers[DVB_NUM_SUB_DEVICES] = { 1, 1, 1, 1, 1, 1 };

ssize_t dvb_device_read(int minor, int type, struct file *file, char *buf, 
			size_t count, loff_t *ppos)
{
        struct dvb_device *dvbdev=dvb_device[minor];

	if (!dvbdev)
	        return -ENODEV;
        return dvbdev->read(dvbdev, type, file, buf, count, ppos);
}

ssize_t dvb_device_write(int minor, int type, struct file *file, const char *buf, 
			 size_t count, loff_t *ppos)
{
        struct dvb_device *dvbdev=dvb_device[minor];

	if (!dvbdev)
	        return -ENODEV;

        return dvbdev->write(dvbdev, type, file, buf, count, ppos);
}

int dvb_device_open(int minor, int type, struct inode *inode, struct file *file)
{
        struct dvb_device *dvbdev=dvb_device[minor];
	int err;

	if (!dvbdev)
	        return -ENODEV;
	if (type<0 || type>=DVB_NUM_SUB_DEVICES)
                return -EINVAL;
	if (dvbdev->users[type]>=max_users[type])
	        return -EBUSY;
	
	if ((file->f_flags&O_ACCMODE)!=O_RDONLY) {
	        if (dvbdev->writers[type]>=max_writers[type])
		        return -EBUSY;
	}

        err=dvbdev->open(dvbdev, type, inode, file);

	if (err<0) 
	        return err;

	if ((file->f_flags&O_ACCMODE)!=O_RDONLY)
		dvbdev->writers[type]++;

        dvbdev->users[type]++;
	return 0;
}

int dvb_device_close(int minor, int type, struct inode *inode, struct file *file)
{
        struct dvb_device *dvbdev=dvb_device[minor];
	int err;

	if (!dvbdev)
	        return -ENODEV;
	if (type<0 || type>=DVB_NUM_SUB_DEVICES)
                return -EINVAL;

        err=dvbdev->close(dvbdev, type, inode, file);
	if (err<0) 
	        return err;
	if ((file->f_flags&O_ACCMODE)!=O_RDONLY)
		dvbdev->writers[type]--;
        dvbdev->users[type]--;
	return 0;
}

int dvb_device_ioctl(int minor, int type, 
		     struct file *file, unsigned int cmd, unsigned long arg)
{
        struct dvb_device *dvbdev=dvb_device[minor];

	if (!dvbdev)
	        return -ENODEV;

        return dvbdev->ioctl(dvbdev, type, file, cmd, arg);
}

unsigned int dvb_device_poll(int minor, int type, 
			     struct file *file, poll_table *wait)
{
        struct dvb_device *dvbdev=dvb_device[minor];

	if (!dvbdev)
	        return -ENODEV;

        return dvbdev->poll(dvbdev, type, file, wait);
}

static void dvb_init_device(dvb_device_t *dev)
{
        int i;
	
	for (i=0; i<DVB_NUM_SUB_DEVICES; i++) {
	        dev->users[i]=0;
		dev->writers[i]=0;
	}	
}


int dvb_register_device(dvb_device_t *dev)
{
	int i=0;
	
	for(i=0; i<DVB_NUM_DEVICES; i++)
	{
		if(dvb_device[i]==NULL)
		{
			dvb_device[i]=dev;
			dev->minor=i;
			dvb_init_device(dev);
			MOD_INC_USE_COUNT;
			return 0;
		}
	}
	return -ENFILE;
}

void dvb_unregister_device(dvb_device_t *dev)
{
        if (dvb_device[dev->minor]!=dev)
	        panic("dvbdev: bad unregister");
        dvb_device[dev->minor]=NULL;
	MOD_DEC_USE_COUNT;
}

int 
#if LINUX_VERSION_CODE >= 0x020300
__init
#endif
dvbdev_init(void)
{
	int i=0;
	
	for(i=0; i<DVB_NUM_DEVICES; i++)
	        dvb_device[i]=NULL;
	return 0;
}

#ifdef MODULE		
int init_module(void)
{
	return dvbdev_init();
}

void cleanup_module(void)
{

}

#endif

EXPORT_SYMBOL(dvb_register_device);
EXPORT_SYMBOL(dvb_unregister_device);
EXPORT_SYMBOL(dvb_device_open);
EXPORT_SYMBOL(dvb_device_close);
EXPORT_SYMBOL(dvb_device_read);
EXPORT_SYMBOL(dvb_device_write);
EXPORT_SYMBOL(dvb_device_ioctl);
EXPORT_SYMBOL(dvb_device_poll);

MODULE_AUTHOR("Ralph Metzler");
MODULE_DESCRIPTION("Device registrar for DVB drivers");


