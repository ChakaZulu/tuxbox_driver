/*
 * $Id: cam_napi.c,v 1.1 2002/11/08 01:25:52 obi Exp $
 *
 * Copyright (C) 2002 by Andreas Oberritter <obi@tuxbox.org>
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <dvb-core/dvbdev.h>
#include <linux/dvb/ca.h>

#include <dbox/cam.h>


static struct dvb_device *ca_dev;
static u8 ca_dev_registered = 0;


static int cam_napi_ioctl (struct inode *inode, struct file *file, unsigned int cmd, void *parg)
{
	int ret = 0;

	if (((file->f_flags & O_ACCMODE) == O_RDONLY) &&
		((cmd != CA_GET_CAP) ||
		(cmd != CA_GET_SLOT_INFO) ||
		(cmd != CA_GET_DESCR_INFO) ||
		(cmd != CA_GET_MSG)))
			return -EPERM;

	switch (cmd) {

		case CA_RESET:
			ret = cam_reset();
			break;

		case CA_GET_CAP:
			ret = -EOPNOTSUPP;
			break;

		case CA_GET_SLOT_INFO:
			ret = -EOPNOTSUPP;
			break;

		case CA_GET_DESCR_INFO:
			ret = -EOPNOTSUPP;
			break;

		case CA_GET_MSG:
		{
			struct ca_msg *msg = parg;
			msg->index = 0;
			msg->type = 0;
			msg->length = cam_read_message(msg->msg, msg->length);
			break;
		}

		case CA_SEND_MSG:
		{
			struct ca_msg *msg = parg;
			ret = cam_write_message(msg->msg, msg->length);
			break;
		}

		case CA_SET_DESCR:
			ret = -EOPNOTSUPP;
			break;

	}

	return ret;

}


static struct file_operations cam_napi_fops = {

	.owner = THIS_MODULE,
	.ioctl = dvb_generic_ioctl,
	.open = dvb_generic_open,
	.release = dvb_generic_release,

};


static struct dvb_device cam_napi_dev = {

	.priv = 0,
	.users = 1,
	.writers = 1,
	.fops = &cam_napi_fops,
	.kernel_ioctl = cam_napi_ioctl,

};


int cam_napi_register (struct dvb_adapter *adapter, void *priv)
{

	int result;

	if (ca_dev_registered)
		return -EINVAL;

	if ((result = dvb_register_device(adapter, &ca_dev, &cam_napi_dev, NULL, DVB_DEVICE_CA)) < 0) {

		printk("cam_napi: cam_napi_register failed (errno = %d)\n", result);

		return result;

	}

	ca_dev_registered = 1;

	return 0;

}

void cam_napi_unregister (void)
{

	if (ca_dev_registered) {
	
		dvb_unregister_device(ca_dev);

		ca_dev_registered = 0;

	}

}


int cam_napi_init (void)
{

	printk("$Id: cam_napi.c,v 1.1 2002/11/08 01:25:52 obi Exp $\n");

	return 0;

}


void cam_napi_exit (void)
{
	
	cam_napi_unregister();

}


EXPORT_SYMBOL(cam_napi_register);
EXPORT_SYMBOL(cam_napi_unregister);

#if defined(MODULE)
module_init(cam_napi_init);
module_exit(cam_napi_exit);
#endif

