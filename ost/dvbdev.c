/* 
 * dvbdev.c
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
 * $Id: dvbdev.c,v 1.2 2001/03/03 08:24:01 waldi Exp $
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

static struct dvb_device * dvb_device[DVB_NUM_DEVICES];

#define INFU 32768

static int max_users[DVB_NUM_SUB_DEVICES] = { INFU, INFU, INFU, INFU, INFU, 1 };
static int max_writers[DVB_NUM_SUB_DEVICES] = { 1, 1, 1, 1, 1, 1 };

ssize_t dvb_device_read ( int type, struct file * file, char * buf,
                          size_t count, loff_t * ppos )
{
  struct dvb_device * dvbdev = dvb_device[0];

  if ( ! dvbdev )
    return -ENODEV;

  return dvbdev -> read ( dvbdev, type, file, buf, count, ppos );
}

ssize_t dvb_device_write ( int type, struct file * file, const char * buf,
                           size_t count, loff_t * ppos )
{
  struct dvb_device * dvbdev = dvb_device[0];

  if ( ! dvbdev )
    return -ENODEV;

  return dvbdev -> write ( dvbdev, type, file, buf, count, ppos );
}

int dvb_device_open ( int type, struct inode * inode, struct file * file )
{
  struct dvb_device * dvbdev = dvb_device[0];
  int err;

  if ( ! dvbdev )
    return -ENODEV;
  if ( type < 0 || type >= DVB_NUM_SUB_DEVICES )
    return -EINVAL;
  if ( dvbdev -> users[type] >= max_users[type] )
    return -EBUSY;
	
  if ( ( file -> f_flags & O_ACCMODE ) != O_RDONLY )
  {
    if ( dvbdev -> writers[type] >= max_writers[type] )
      return -EBUSY;
  }

  err=dvbdev -> open ( dvbdev, type, inode, file );

  if ( err < 0 ) 
    return err;

  if ( ( file -> f_flags & O_ACCMODE ) != O_RDONLY )
    dvbdev -> writers[type]++;

  dvbdev -> users[type]++;
  return 0;
}

int dvb_device_close ( int type, struct inode * inode, struct file * file )
{
  struct dvb_device * dvbdev = dvb_device[0];
  int err;

  if ( ! dvbdev )
    return -ENODEV;
  if ( type < 0 || type >= DVB_NUM_SUB_DEVICES )
    return -EINVAL;

  err=dvbdev -> close ( dvbdev, type, inode, file );

  if ( err < 0 ) 
    return err;
  if ( ( file -> f_flags & O_ACCMODE ) != O_RDONLY )
    dvbdev -> writers[type]--;

  dvbdev -> users[type]--;
  return 0;
}

int dvb_device_ioctl ( int type, struct file * file,
                       unsigned int cmd, unsigned long arg)
{
  struct dvb_device * dvbdev = dvb_device[0];

  if ( ! dvbdev )
    return -ENODEV;

  return dvbdev -> ioctl ( dvbdev, type, file, cmd, arg );
}

unsigned int dvb_device_poll ( int type, struct file * file, poll_table * wait )
{
  struct dvb_device * dvbdev = dvb_device[0];

  if ( ! dvbdev )
    return -ENODEV;

  return dvbdev -> poll ( dvbdev, type, file, wait );
}

static void dvb_init_device ( dvb_device_t * dev )
{
  int i;

  for ( i = 0; i < DVB_NUM_SUB_DEVICES; i++ )
  {
    dev -> users[i] = 0;
    dev -> writers[i] = 0;
  }	
}

int dvb_register_device ( dvb_device_t * dev )
{
  int i;

  for ( i = 0; i < DVB_NUM_DEVICES; i++ )
  {
    if ( dvb_device[i] == NULL )
    {
      dvb_device[i] = dev;
      dvb_init_device ( dev );
      MOD_INC_USE_COUNT;
      return 0;
    }
  }

  return -ENFILE;
}

void dvb_unregister_device ( dvb_device_t * dev )
{
  MOD_DEC_USE_COUNT;
}

int __init dvbdev_init_module ()
{
  int i;
	
  for ( i = 0; i < DVB_NUM_DEVICES; i++ )
    dvb_device[i] = NULL;

  return 0;
}

module_init ( dvbdev_init_module );

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
