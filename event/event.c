/*
 *	event.c - global event driver (dbox-II-project)
 *
 *	Homepage: http://dbox2.elxsi.de
 *
 *	Copyright (C) 2001 Gillem (gillem@berlios.de)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *	$Log: event.c,v $
 *	Revision 1.1  2001/12/07 14:30:37  gillem
 *	- initial release (not ready today)
 *	
 *
 *	$Revision: 1.1 $
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/types.h>
#include <linux/devfs_fs_kernel.h>

#ifndef CONFIG_DEVFS_FS
#error no devfs
#endif

static devfs_handle_t devfs_handle;

static int event_ioctl (struct inode *inode, struct file *file,
                         unsigned int cmd, unsigned long arg);
static int event_open (struct inode *inode, struct file *file);

static struct file_operations event_fops = {
	owner:		THIS_MODULE,
	ioctl:		event_ioctl,
	open:		event_open,
};

#define dprintk     if (debug) printk

static int debug = 0;

int event_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
                  unsigned long arg)
{
	return 0;
}

int event_open (struct inode *inode, struct file *file)
{
	return 0;
}

#ifdef MODULE
int init_module(void)
#else
int event_init(void)
#endif
{
	devfs_handle = devfs_register ( NULL, "dbox/event0", DEVFS_FL_DEFAULT,
		0, 0,
		S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
		&event_fops, NULL );

	if ( ! devfs_handle )
	{
		return -EIO;
	}

	return 0;
}

#ifdef MODULE
void cleanup_module(void)
{
	devfs_unregister ( devfs_handle );
}
#endif
