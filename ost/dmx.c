/* 
 * dmx.c
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
#include <ost/dmx.h>

#define OST_DEMUX_MAJOR 254

static int ost_demux_open(struct inode *inode, struct file *file)
{
        int minor=MINOR(inode->i_rdev);
	int device=(minor&0x40) ? DVB_DEVICE_DVR : DVB_DEVICE_DEMUX;
  
        return dvb_device_open(minor&0x3f, device, inode, file);
}

static int ost_demux_release(struct inode *inode, struct file *file)
{
        int minor=MINOR(inode->i_rdev);
	int device=(minor&0x40) ? DVB_DEVICE_DVR : DVB_DEVICE_DEMUX;
  
        return dvb_device_close(minor&0x3f, device, inode, file);
}

static ssize_t ost_demux_read(struct file *file,
				 char *buf, size_t count, loff_t *ppos)
{
        int minor=MINOR(file->f_dentry->d_inode->i_rdev);
	int device=(minor&0x40) ? DVB_DEVICE_DVR : DVB_DEVICE_DEMUX;
  
        return dvb_device_read(minor&0x3f, device, file, buf, count, ppos);
}

static ssize_t ost_demux_write(struct file *file, const char *buf, 
			      size_t count, loff_t *ppos)
{
        int minor=MINOR(file->f_dentry->d_inode->i_rdev);
	int device=(minor&0x40) ? DVB_DEVICE_DVR : DVB_DEVICE_DEMUX;
  
        return dvb_device_write(minor&0x3f, device, file, buf, count, ppos);
}

static int ost_demux_ioctl(struct inode *inode, struct file *file,
			      unsigned int cmd, unsigned long arg)
{
        int minor=MINOR(inode->i_rdev);
	int device=(minor&0x40) ? DVB_DEVICE_DVR : DVB_DEVICE_DEMUX;
	
	return dvb_device_ioctl(minor&0x3f, device, file, cmd, arg);
}	

static unsigned int ost_demux_poll(struct file *file, poll_table *wait)
{
        int minor=MINOR(file->f_dentry->d_inode->i_rdev);
	int device=(minor&0x40) ? DVB_DEVICE_DVR : DVB_DEVICE_DEMUX;

	return dvb_device_poll(minor&0x3f, device, file, wait);
}

static struct file_operations ost_demux_fops =
{
        read:		ost_demux_read,
	write:		ost_demux_write,
	ioctl:		ost_demux_ioctl,
	open:		ost_demux_open,
	release:	ost_demux_release,
	poll:		ost_demux_poll,
};


#if LINUX_VERSION_CODE >= 0x020300
int __init dmx_init_module(void)
#else
int init_module(void)
#endif
{
#if LINUX_VERSION_CODE < 0x020300
	if(register_chrdev(OST_DEMUX_MAJOR, "ost/demux", &ost_demux_fops))
#else

	if(devfs_register_chrdev(OST_DEMUX_MAJOR, "ost/demux", &ost_demux_fops))
#endif
	{
		printk("DEMUX: unable to get major %d\n", OST_DEMUX_MAJOR);
		return -EIO;
	}

	return 0;
}

#if LINUX_VERSION_CODE >= 0x020300
void __exit dmx_cleanup_module(void)
#else
void cleanup_module(void)
#endif
{
 
#if LINUX_VERSION_CODE < 0x020300
	unregister_chrdev(OST_DEMUX_MAJOR, "ost/demux");
#else
	devfs_unregister_chrdev(OST_DEMUX_MAJOR, "ost/demux");
#endif
}

#if LINUX_VERSION_CODE >= 0x020300
module_init(dmx_init_module);
module_exit(dmx_cleanup_module);
#endif

