/* 
 * video.c
 *
 * Copyright (C) 2000 Ralph  Metzler <ralph@convergence.de>
 *                  & Marcus Metzler <marcus@convergence.de>
 *                    for convergence integrated media GmbH
 *               2001 Bastian Blank <bastianb@gmx.de>
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
 * $Id: video.c,v 1.4 2001/03/03 11:49:17 waldi Exp $
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

#ifndef CONFIG_DEVFS_FS
#error no devfs
#endif

static devfs_handle_t devfs_handle;

static int ost_video_open ( struct inode *inode, struct file *file )
{
  return dvb_device_open ( DVB_DEVICE_VIDEO, inode, file );
}

static int ost_video_release ( struct inode *inode, struct file *file )
{
  return dvb_device_close ( DVB_DEVICE_VIDEO, inode, file );
}

static ssize_t ost_video_read ( struct file *file, char *buf,
                                size_t count, loff_t *ppos )
{
  return dvb_device_read ( DVB_DEVICE_VIDEO, file, buf, count, ppos );
}

static ssize_t ost_video_write ( struct file *file, const char *buf, 
                                 size_t count, loff_t *ppos )
{
  return dvb_device_write ( DVB_DEVICE_VIDEO, file, buf, count, ppos );
}

static int ost_video_ioctl ( struct inode *inode, struct file *file,
			     unsigned int cmd, unsigned long arg )
{
  return dvb_device_ioctl ( DVB_DEVICE_VIDEO, file, cmd, arg );
}

static unsigned int ost_video_poll ( struct file *file, poll_table *wait )
{
  return dvb_device_poll ( DVB_DEVICE_VIDEO, file, wait );
}

static struct file_operations ost_video_fops =
{
  read:		ost_video_read,
  write:	ost_video_write,
  ioctl:	ost_video_ioctl,
  open:		ost_video_open,
  release:	ost_video_release,
  poll:		ost_video_poll,
};

int __init video_init_module ()
{
  devfs_handle = devfs_register ( NULL, "ost/video0", DEVFS_FL_DEFAULT,
                                  0, 0,
                                  S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
                                  &ost_video_fops, NULL );

  return 0;
}

void __exit video_cleanup_module ()
{
  devfs_unregister ( devfs_handle );
}

module_init ( video_init_module );
module_exit ( video_cleanup_module );
