/* 
 * sec.c
 *
 * Copyright (C) 2000 Marcus Metzler <marcus@convergence.de>
 *                  & Ralph  Metzler <ralph@convergence.de>
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
#include <linux/poll.h>
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

#include <ost/video.h>
#include <ost/dvbdev.h>
#include <ost/sec.h> 

#define OST_SEC_MAJOR 251

static int ost_sec_open(struct inode *inode, struct file *file)
{
        return dvb_device_open(MINOR(inode->i_rdev), DVB_DEVICE_SEC,
			       inode, file);
}

static int ost_sec_release(struct inode *inode, struct file *file)
{
        return dvb_device_close(MINOR(inode->i_rdev), DVB_DEVICE_SEC,
				inode, file);
}

static int ost_sec_ioctl(struct inode *inode, struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	unsigned int minor = MINOR(inode->i_rdev);
	
	return dvb_device_ioctl(minor, DVB_DEVICE_SEC, file, cmd, arg);
}

static unsigned int ost_sec_poll(struct file *file, poll_table *wait)
{
        return 0;
}

static struct file_operations ost_sec_fops =
{
        //llseek:		ost_sec_lseek,
        //read:		ost_sec_read,
        //write:		ost_sec_write,
	ioctl:		ost_sec_ioctl,
	//	mmap:		ost_sec_mmap,
	open:		ost_sec_open,
	release:	ost_sec_release,
	poll:		ost_sec_poll,
};



int __init dvbdev_sec_init(void)
{
#if LINUX_VERSION_CODE < 0x020300
	if(register_chrdev(OST_SEC_MAJOR, "ost/sec", &ost_sec_fops))
#else

	if(devfs_register_chrdev(OST_SEC_MAJOR, "ost/sec", &ost_sec_fops))
#endif
	{
		printk("sec: unable to get major %d\n", OST_SEC_MAJOR);
		return -EIO;
	}

	return 0;
}

int
#if LINUX_VERSION_CODE >= 0x020300
__init sec_init_module(void)
#else
init_module(void)
#endif
{
        return dvbdev_sec_init();
}

void
#if LINUX_VERSION_CODE >= 0x020300
__exit sec_cleanup_module(void)
#else
cleanup_module(void)
#endif
{
 
#if LINUX_VERSION_CODE < 0x020300
	unregister_chrdev(OST_SEC_MAJOR, "ost/sec");
#else
	devfs_unregister_chrdev(OST_SEC_MAJOR, "ost/sec");
#endif
}

#if LINUX_VERSION_CODE >= 0x020300
module_init(sec_init_module);
module_exit(sec_cleanup_module);
#endif

