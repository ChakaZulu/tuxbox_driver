/* 
 * dvbdev.h
 *
 * Copyright (C) 2000 Ralph  Metzler <ralph@convergence.de>
 *                  & Marcus Metzler <marcus@convergence.de>
 *                    for convergence integrated media GmbH
 *               2001 Bastian Blank <bastianb@gmx.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Lesser Public License
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
 * $Id: dvbdev.h,v 1.7 2001/06/24 08:34:53 gillem Exp $
 */

#ifndef _DVBDEV_H_
#define _DVBDEV_H_

#include <linux/types.h>
#include <linux/version.h>

#ifdef __KERNEL__

#include <linux/poll.h>
#include <linux/devfs_fs_kernel.h>

#include <mtdriver/ostDevices.h>

#define DVB_DEVICES_NUM         1

#define DVB_SUBDEVICES_NUM      9

#define DVB_DEVICE_VIDEO        0
#define DVB_DEVICE_AUDIO        1
#define DVB_DEVICE_SEC          2
#define DVB_DEVICE_FRONTEND     3
#define DVB_DEVICE_DEMUX        4
#define DVB_DEVICE_DVR          5
#define DVB_DEVICE_CA           6
#define DVB_DEVICE_SCART        7

#define DVB_DEVFSDIRS_NUM       2

#define DVB_DEVFSDIR_DVB        0
#define DVB_DEVFSDIR_OST        1

const char * subdevice_names[] = { VIDEO_DEVICE_NAME, AUDIO_DEVICE_NAME, SEC_DEVICE_NAME, "qpskfe", DMX_DEVICE_NAME, "dvr", "ca", SCART_DEVICE_NAME, OSTKBD_DEVICE_NAME };

struct dvbdev_devfsinfo
{
  struct dvb_device * device;
  int type;
};

struct dvb_device
{
  char name[32];
  int type;
  int hardware;

  devfs_handle_t devfs_handle_dvb_dir;
  devfs_handle_t devfs_handle_dvb[DVB_SUBDEVICES_NUM];
  devfs_handle_t devfs_handle_ost[DVB_SUBDEVICES_NUM];
  struct dvbdev_devfsinfo devfs_info[DVB_SUBDEVICES_NUM];

  void *priv;

  int ( * open ) ( struct dvb_device *, int, struct inode *, struct file * );
  int ( * close ) ( struct dvb_device *, int, struct inode *, struct file * );
  ssize_t ( * read ) ( struct dvb_device *, int, struct file *, char *, size_t, loff_t * );
  ssize_t ( * write ) ( struct dvb_device *, int, struct file *, const char *, size_t, loff_t * );
  int ( * ioctl ) ( struct dvb_device *, int, struct file *, unsigned int , unsigned long );
  unsigned int ( * poll ) ( struct dvb_device *, int type, struct file *, poll_table * wait );
};

typedef struct dvbdev_devfsinfo dvbdev_devfsinfo_t;
typedef struct dvb_device dvb_device_t;

int dvb_register_device ( struct dvb_device * );
void dvb_unregister_device ( struct dvb_device * );

#endif

#endif /* #ifndef __DVBDEV_H */
