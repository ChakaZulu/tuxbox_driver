/*
 * $Id: event.c,v 1.11 2003/09/08 22:38:56 obi Exp $
 * 
 * global event driver (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2001 Gillem <gillem@berlios.de>
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

#include <linux/devfs_fs_kernel.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/slab.h>

#include <dbox/event.h>

#define EVENTBUFFERSIZE		32

struct event_data_t {
	struct event_t event[EVENTBUFFERSIZE];
	u16 event_free;
	u16 event_ptr;
	u16 event_read_ptr;
};

struct event_private_t {
	u32 event_filter;
	struct event_data_t event_data;
};

#define MAX_EVENT_OPEN 5

static int open_handle = 0;
static struct event_private_t *event_private[5];
static DECLARE_WAIT_QUEUE_HEAD(event_wait);
static spinlock_t event_lock;
static devfs_handle_t devfs_handle;

#define dprintk(x...)
//#define dprintk(x...) printk(x)

void event_write_message(struct event_t *event, size_t count)
{
	struct event_private_t *e;
	int i, s;

	spin_lock(&event_lock);

	dprintk("write %d event's ...\n", count);

	for (s = 0; s < count; s++) {
		for (i = 0; i < MAX_EVENT_OPEN; i++) {
			if (event_private[i]) {
				e = event_private[i];
				dprintk("write event ... free found\n");
				if ((e->event_data.event_free > 0) && (event[s].event & e->event_filter)) {
					dprintk("write event ... filter ok\n");
					e->event_data.event_free--;
					memcpy(&e->event_data.event[e->event_data.event_ptr], &event[s], sizeof(struct event_t));
					e->event_data.event_ptr++;
					if (EVENTBUFFERSIZE == e->event_data.event_ptr)
						e->event_data.event_ptr = 0;
				}
			}
		}
	}

	spin_unlock(&event_lock);

	wake_up_interruptible(&event_wait);
}

static int event_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
		  unsigned long arg)
{
	struct event_private_t *event_priv = file->private_data;

	switch (cmd) {
	case EVENT_SET_FILTER:
		dprintk("set event: %08lX\n",arg);
		event_priv->event_filter = arg;
		break;
	default:
		break;
	}

	return 0;
}

static int event_open(struct inode *inode, struct file *file)
{
	int i;

	if (open_handle == MAX_EVENT_OPEN)
		return -EMFILE;

	open_handle++;

	file->private_data = kmalloc(sizeof(struct event_private_t), GFP_KERNEL);
	memset(file->private_data, 0 ,sizeof(struct event_private_t));

	for (i = 0; i < MAX_EVENT_OPEN; i++)
		if (event_private[i] == NULL) {
			event_private[i] = file->private_data;
			break;
		}

	event_private[i]->event_filter = 0xFFFFFFFF;
	event_private[i]->event_data.event_free = EVENTBUFFERSIZE;

	return 0;
}

static int event_release(struct inode *inode, struct file *file)
{
	int i;
	open_handle--;

	if (file->private_data) {
		for(i = 0; i < MAX_EVENT_OPEN; i++)
			if (event_private[i] == file->private_data) {
				event_private[i] = NULL;
				break;
			}
		kfree(file->private_data);
		file->private_data = NULL;
	}

	return 0;
}

static ssize_t event_read(struct file *file, char *buf, size_t count, loff_t *offset)
{
	struct event_private_t *event_priv = file->private_data;
	DECLARE_WAITQUEUE(wait, current);
	ssize_t retval = 0;
	int err;

	if (count < sizeof(struct event_t))
		return -EINVAL;

	add_wait_queue(&event_wait, &wait);

	current->state = TASK_INTERRUPTIBLE;

	do {
		err = 0;

		spin_lock_irq(&event_lock);

		if (EVENTBUFFERSIZE != event_priv->event_data.event_free) {
			dprintk("found data %d %d\n",event_priv->event_data.event_free,event_priv->event_data.event_read_ptr);
			retval = put_user(event_priv->event_data.event[event_priv->event_data.event_read_ptr], (struct event_t *)buf);

			if (!retval) {
				event_priv->event_data.event_free++;

				event_priv->event_data.event_read_ptr++;
				if (EVENTBUFFERSIZE == event_priv->event_data.event_read_ptr)
					event_priv->event_data.event_read_ptr = 0;

				retval = sizeof(unsigned long);
			}
			else {
				err = 1;
			}
		}

		spin_unlock_irq(&event_lock);

		if (err || retval)
			break;

		if (file->f_flags & O_NONBLOCK) {
			retval = -EAGAIN;
			break;
		}
		
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}

		schedule();

	} while (1);

	current->state = TASK_RUNNING;

	remove_wait_queue(&event_wait, &wait);

	return retval;
}

static unsigned int event_poll(struct file *file, poll_table *wait)
{
	struct event_private_t *event_priv = file->private_data;

	poll_wait(file, &event_wait, wait);

	if (EVENTBUFFERSIZE != event_priv->event_data.event_free)
		return (POLLIN | POLLRDNORM);

	return 0;
}

static struct file_operations event_fops = {
	.owner = THIS_MODULE,
	.read = event_read,
	.ioctl = event_ioctl,
	.open = event_open,
	.release = event_release,
	.poll = event_poll,
};

static int __init event_init(void)
{
	printk(KERN_INFO "event: $Id: event.c,v 1.11 2003/09/08 22:38:56 obi Exp $\n");

	memset(event_private, 0, sizeof(event_private));

	devfs_handle = devfs_register( NULL, "dbox/event0", DEVFS_FL_DEFAULT,
		0, 0,
		S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
		&event_fops, NULL);

	if (!devfs_handle)
		return -EIO;

	return 0;
}

static void __exit event_exit(void)
{
	devfs_unregister(devfs_handle);
}

module_init(event_init);
module_exit(event_exit);

MODULE_AUTHOR("Gillem <gillem@berlios.de>");
MODULE_DESCRIPTION("global event driver");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(event_write_message);
