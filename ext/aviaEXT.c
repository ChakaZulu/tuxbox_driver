/*
 * Extension device for non-API covered stuff for the Avia
 * (hopefully will disappear at some point)
 *
 * $Id: aviaEXT.c,v 1.3 2004/07/03 22:46:12 carjay Exp $
 *
 * Copyright (C) 2004 Carsten Juttner <carjay@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/devfs_fs_kernel.h>
#include <asm/uaccess.h>

#include "avia_av.h"
#include <dbox/aviaEXT.h>

static devfs_handle_t devfs_h;

static int aviaEXT_ioctl(struct inode *inode, struct file *file, 
						unsigned int cmd, unsigned long arg)
{
	switch (cmd){
	case AVIA_EXT_IEC_SET:	/* true turns on optical audio output, false turns it off */
		if (avia_av_is500())
			return -EOPNOTSUPP;
		if (!arg) {
			avia_av_dram_write(AUDIO_CONFIG, avia_av_dram_read(AUDIO_CONFIG)|0x100);
		} else {
			avia_av_dram_write(AUDIO_CONFIG, avia_av_dram_read(AUDIO_CONFIG)&~0x100);
		}
		avia_av_new_audio_config();
		break;
	case AVIA_EXT_IEC_GET:	/* true if optical output enabled, false if disabled */
		if (avia_av_is500())
			return -EOPNOTSUPP;
		if (put_user((!(avia_av_dram_read(AUDIO_CONFIG)&0x100)),(int *)arg))
			return -EFAULT;
		break;		
	default:
		printk (KERN_WARNING "aviaEXT: unknown ioctl: %d\n",cmd);
		break;
	}
	return 0;
}

static struct file_operations aviaEXT_fops = {
		THIS_MODULE,
		.ioctl = aviaEXT_ioctl
};

static int __init aviaEXT_init(void)
{
	if (!(devfs_h = devfs_register(NULL,"dbox/aviaEXT", DEVFS_FL_DEFAULT, 0, 0, 
					S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, &aviaEXT_fops, NULL))){
		printk(KERN_ERR "aviaEXT: could not register with devfs.\n");
		return -EIO;
	}
	return 0;
}

static void __exit aviaEXT_exit(void)
{
	devfs_unregister(devfs_h);
}

module_init(aviaEXT_init);
module_exit(aviaEXT_exit);

MODULE_AUTHOR("Carsten Juttner <carjay@gmx.net>");
MODULE_DESCRIPTION("AViA non-API extensions");
MODULE_LICENSE("GPL");
