/* 
 * video.c
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

#define OST_VIDEO_MAJOR 250

static int ost_video_open(struct inode *inode, struct file *file)
{
        return dvb_device_open(MINOR(inode->i_rdev), DVB_DEVICE_VIDEO,
			       inode, file);
}

static int ost_video_release(struct inode *inode, struct file *file)
{
        return dvb_device_close(MINOR(inode->i_rdev), DVB_DEVICE_VIDEO,
				inode, file);
}

static ssize_t ost_video_write(struct file *file, const char *buf, 
			      size_t count, loff_t *ppos)
{
        return dvb_device_write(MINOR(file->f_dentry->d_inode->i_rdev), 
				DVB_DEVICE_VIDEO, file, buf, count, ppos);
}

static ssize_t ost_video_read(struct file *file,
			      char *buf, size_t count, loff_t *ppos)
{
        return dvb_device_read(MINOR(file->f_dentry->d_inode->i_rdev), 
				DVB_DEVICE_VIDEO, file, buf, count, ppos);
}

static int ost_video_ioctl(struct inode *inode, struct file *file,
			   unsigned int cmd, unsigned long arg)
{
	unsigned int minor = MINOR(inode->i_rdev);
	
	return dvb_device_ioctl(minor, DVB_DEVICE_VIDEO, file, cmd, arg);
}

static unsigned int ost_video_poll(struct file *file, poll_table *wait)
{
        return dvb_device_poll(MINOR(file->f_dentry->d_inode->i_rdev), 
			       DVB_DEVICE_VIDEO, file, wait);
}

static struct file_operations ost_video_fops =
{
        //llseek:		ost_video_lseek,
        read:		ost_video_read,
	write:		ost_video_write,
	ioctl:		ost_video_ioctl,
	//	mmap:		ost_video_mmap,
	open:		ost_video_open,
	release:	ost_video_release,
	poll:		ost_video_poll,
};

#ifdef CONFIG_DEVFS_FS
static devfs_handle_t devfs_handle;
#endif

int __init dvbdev_video_init(void)
{
#if LINUX_VERSION_CODE < 0x020300
	if(register_chrdev(OST_VIDEO_MAJOR, "ost/video", &ost_video_fops))
#else

	if(devfs_register_chrdev(OST_VIDEO_MAJOR, "ost/video", &ost_video_fops))
#endif
	{
		printk("video: unable to get major %d\n", OST_VIDEO_MAJOR);
		return -EIO;
	}

#ifdef CONFIG_DEVFS_FS
	devfs_handle = devfs_register (NULL, "ost/video0", DEVFS_FL_DEFAULT,
                                       OST_VIDEO_MAJOR, 0,
                                       S_IFCHR | S_IRUSR | S_IWUSR,
                                       &ost_video_fops, NULL);
#endif

	return 0;
}

int 
#if LINUX_VERSION_CODE >= 0x020300
__init video_init_module(void)
#else
init_module(void)
#endif
{
        return dvbdev_video_init();
}

void 
#if LINUX_VERSION_CODE >= 0x020300
__exit video_cleanup_module(void)
#else
cleanup_module(void)
#endif
{
 
#if LINUX_VERSION_CODE < 0x020300
	unregister_chrdev(OST_VIDEO_MAJOR, "ost/video");
#else
	devfs_unregister_chrdev(OST_VIDEO_MAJOR, "ost/video");
#endif

#ifdef CONFIG_DEVFS_FS
	devfs_unregister (devfs_handle);
#endif
}

#if LINUX_VERSION_CODE >= 0x020300
module_init(video_init_module);
module_exit(video_cleanup_module);
#endif
