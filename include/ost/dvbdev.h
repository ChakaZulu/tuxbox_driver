/* 
 * dvbdev.h
 *
 * Copyright (C) 2000 Ralph  Metzler <ralph@convergence.de>
 *                  & Marcus Metzler <marcus@convergence.de>
                      for convergence integrated media GmbH
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
 */

#ifndef _DVBDEV_H_
#define _DVBDEV_H_

#include <linux/types.h>
#include <linux/version.h>

#ifdef __KERNEL__

#if LINUX_VERSION_CODE >= 0x020100
#include <linux/poll.h>
#endif
#if LINUX_VERSION_CODE >= 0x020300
#include <linux/devfs_fs_kernel.h>
#endif

#define DVB_NUM_DEVICES 16
#define DVB_NUM_SUB_DEVICES 6

#define DVB_DEVICE_VIDEO    0
#define DVB_DEVICE_AUDIO    1
#define DVB_DEVICE_SEC      2
#define DVB_DEVICE_FRONTEND 3
#define DVB_DEVICE_DEMUX    4
#define DVB_DEVICE_DVR      5

struct dvb_device
{
	char name[32];
        int type;
	int hardware;
        u32 capabilities;

	void *priv;
	int minor;
#if LINUX_VERSION_CODE >= 0x020300
	devfs_handle_t devfs_handle;
#endif
        int users[DVB_NUM_SUB_DEVICES];
        int writers[DVB_NUM_SUB_DEVICES];

        int (*open)(struct dvb_device *, int, struct inode *, struct file *);
	int (*close)(struct dvb_device *, int, struct inode *, struct file *);
        ssize_t (*read)(struct dvb_device *, int, struct file *, char *, 
			size_t, loff_t *);
        ssize_t (*write)(struct dvb_device *, int, struct file *, const char *, 
			 size_t, loff_t *);
	int (*ioctl)(struct dvb_device *, int, struct file *, 
		     unsigned int , unsigned long);
        unsigned int (*poll)(struct dvb_device *, int type,
			     struct file *file, poll_table * wait);
};

typedef struct dvb_device dvb_device_t;

extern int dvb_device_init(void);
extern int dvb_register_device(struct dvb_device *);
extern void dvb_unregister_device(struct dvb_device *);

extern int dvb_device_open(int, int, struct inode *inode, struct file *file);
extern int dvb_device_close(int, int, struct inode *inode, struct file *file);
extern ssize_t dvb_device_write(int, int, struct file *file, const char *buf, 
				size_t count, loff_t *ppos);
extern ssize_t dvb_device_read(int, int, struct file *file, char *buf, 
			       size_t count, loff_t *ppos);
extern int dvb_device_ioctl(int, int, struct file *, unsigned int, unsigned long);
extern unsigned int dvb_device_poll(int minor, int type, 
				    struct file *file, poll_table *wait);

int dvb_register_device(struct dvb_device *);
void dvb_unregister_device(struct dvb_device *);

#endif

#endif /* #ifndef __DVBDEV_H */
