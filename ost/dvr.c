/* 
 * dvr.c
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
 * $Id: dvr.c,v 1.2 2001/03/03 11:49:17 waldi Exp $
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

#ifndef CONFIG_DEVFS_FS
#error no devfs
#endif

static devfs_handle_t devfs_handle;

static int ost_dvr_open ( struct inode *inode, struct file *file )
{
  return dvb_device_open ( DVB_DEVICE_DVR, inode, file );
}

static int ost_dvr_release ( struct inode *inode, struct file *file )
{
  return dvb_device_close ( DVB_DEVICE_DVR, inode, file );
}

static ssize_t ost_dvr_read ( struct file *file, char *buf,
                              size_t count, loff_t *ppos )
{
  return dvb_device_read ( DVB_DEVICE_DVR, file, buf, count, ppos );
}

static ssize_t ost_dvr_write ( struct file *file, const char *buf,
                               size_t count, loff_t *ppos )
{
  return dvb_device_write ( DVB_DEVICE_DVR, file, buf, count, ppos );
}

static int ost_dvr_ioctl ( struct inode *inode, struct file *file,
                           unsigned int cmd, unsigned long arg )
{
  return dvb_device_ioctl ( DVB_DEVICE_DVR, file, cmd, arg );
}	

static unsigned int ost_dvr_poll ( struct file *file, poll_table *wait )
{
  return dvb_device_poll ( DVB_DEVICE_DVR, file, wait );
}

static struct file_operations ost_dvr_fops =
{
  open:		ost_dvr_open,
  release:	ost_dvr_release,
  read:		ost_dvr_read,
  write:	ost_dvr_write,
  ioctl:	ost_dvr_ioctl,
  poll:		ost_dvr_poll,
};

int __init dvr_init_module ()
{
  devfs_handle = devfs_register ( NULL, "ost/dvr0", DEVFS_FL_DEFAULT,
                                  0, 0,
                                  S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
                                  &ost_dvr_fops, NULL );

  return 0;
}

void __exit dvr_cleanup_module ()
{
  devfs_unregister ( devfs_handle );
}

module_init ( dvr_init_module );
module_exit ( dvr_cleanup_module );
