/* 
 * frontend.c
 *
 * Copyright (C) 2000 Marcus Metzler <marcus@convergence.de>
 *                  & Ralph  Metzler <ralph@convergence.de>
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
#include <ost/frontend.h>

#ifndef CONFIG_DEVFS_FS
#error no devfs
#endif

static devfs_handle_t devfs_handle;

static int ost_frontend_open ( struct inode *inode, struct file *file )
{
  return dvb_device_open ( DVB_DEVICE_FRONTEND, inode, file );
}

static int ost_frontend_release ( struct inode *inode, struct file *file )
{
  return dvb_device_close ( DVB_DEVICE_FRONTEND, inode, file );
}

static ssize_t ost_frontend_read ( struct file *file, char *buf,
                                   size_t count, loff_t *ppos )
{
  return -ENOTSUPP;
}

static ssize_t ost_frontend_write ( struct file *file, const char *buf,
                                    size_t count, loff_t *ppos )
{
  return -ENOTSUPP;
}

static int ost_frontend_ioctl ( struct inode *inode, struct file *file,
                                unsigned int cmd, unsigned long arg )
{
  return dvb_device_ioctl ( DVB_DEVICE_FRONTEND, file, cmd, arg );
}

static unsigned int ost_frontend_poll ( struct file *file, poll_table *wait )
{
  return 0;
}

static struct file_operations ost_frontend_fops =
{
  open:		ost_frontend_open,
  release:	ost_frontend_release,
  read:		ost_frontend_read,
  write:	ost_frontend_write,
  ioctl:	ost_frontend_ioctl,
  poll:		ost_frontend_poll,
};

int __init frontend_init_module ()
{
  devfs_handle = devfs_register ( NULL, "ost/frontend0", DEVFS_FL_DEFAULT,
                                  0, 0,
                                  S_IFCHR | S_IRUSR | S_IWUSR,
                                  &ost_frontend_fops, NULL );

  return 0;
}

void __exit frontend_cleanup_module ()
{
  devfs_unregister ( devfs_handle );
}

module_init ( frontend_init_module );
module_exit ( frontend_cleanup_module );
