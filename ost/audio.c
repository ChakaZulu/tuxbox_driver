/* 
 * audio.c
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

#include <ost/dvbdev.h>
#include <ost/audio.h>

#define OST_AUDIO_MAJOR 252

static int ost_audio_open(struct inode *inode, struct file *file)
{
        return dvb_device_open(MINOR(inode->i_rdev), DVB_DEVICE_AUDIO,
			       inode, file);
}

static int ost_audio_release(struct inode *inode, struct file *file)
{
        return dvb_device_close(MINOR(inode->i_rdev), DVB_DEVICE_AUDIO,
				inode, file);
}

static ssize_t ost_audio_write(struct file *file, const char *buf, 
			       size_t count, loff_t *ppos)
{
        return dvb_device_write(MINOR(file->f_dentry->d_inode->i_rdev), 
				DVB_DEVICE_AUDIO, file, buf, count, ppos);
}

static ssize_t ost_audio_read(struct file *file,
			      char *buf, size_t count, loff_t *ppos)
{
        return dvb_device_read(MINOR(file->f_dentry->d_inode->i_rdev), 
				DVB_DEVICE_AUDIO, file, buf, count, ppos);
}

static int ost_audio_ioctl(struct inode *inode, struct file *file,
			   unsigned int cmd, unsigned long arg)
{
	return dvb_device_ioctl(MINOR(inode->i_rdev),
				DVB_DEVICE_AUDIO, file, cmd, arg);
}

static unsigned int ost_audio_poll(struct file *file, poll_table *wait)
{
        return dvb_device_poll(MINOR(file->f_dentry->d_inode->i_rdev), 
			       DVB_DEVICE_AUDIO, file, wait);
}

static struct file_operations ost_audio_fops =
{
        read:		ost_audio_read,
	write:		ost_audio_write,
	ioctl:		ost_audio_ioctl,
	open:		ost_audio_open,
	release:	ost_audio_release,
	poll:		ost_audio_poll,
};

#if LINUX_VERSION_CODE >= 0x020300
int __init audio_init_module(void)
#else
int init_module(void)
#endif
{
#if LINUX_VERSION_CODE < 0x020300
	if(register_chrdev(OST_AUDIO_MAJOR, "ost/audio", &ost_audio_fops))
#else
	if(devfs_register_chrdev(OST_AUDIO_MAJOR, "ost/audio", &ost_audio_fops))
#endif
	{
		printk("audio_dev: unable to get major %d\n", OST_AUDIO_MAJOR);
		return -EIO;
	}

	return 0;
}

#if LINUX_VERSION_CODE >= 0x020300
void __exit audio_cleanup_module(void)
#else
void cleanup_module(void)
#endif
{
 
#if LINUX_VERSION_CODE < 0x020300
	unregister_chrdev(OST_AUDIO_MAJOR, "ost/audio");
#else
	devfs_unregister_chrdev(OST_AUDIO_MAJOR, "ost/audio");
#endif
}

#if LINUX_VERSION_CODE >= 0x020300
module_init(audio_init_module);
module_exit(audio_cleanup_module);
#endif
