/*
 *   lirc_dbox2.c - lirc ir driver for AViA eNX/GTX (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2002 Florian Schirmer (jolt@tuxbox.org)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <linux/devfs_fs_kernel.h>

#include <dbox/avia_gt_ir.h>
#include "lirc.h"

#ifndef CONFIG_DEVFS_FS
#error no devfs
#endif

static devfs_handle_t devfs_handle;
static lirc_t pulse_buffer[AVIA_GT_IR_MAX_PULSE_COUNT * 2];

#ifdef MODULE
MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("AViA lirc driver");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#endif

static int avia_gt_lirc_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{

	unsigned long value;
	int result;

	switch(cmd) {
	
		case LIRC_GET_FEATURES:

			result = put_user(/*LIRC_CAN_REC_MODE2 |*/ LIRC_CAN_SEND_PULSE | LIRC_CAN_SET_SEND_CARRIER | LIRC_CAN_SET_SEND_DUTY_CYCLE, (unsigned long *)arg);
			
			if (result)
				return result; 
				
		break;
		
/*		case LIRC_GET_REC_MODE:
		
			result = put_user(LIRC_MODE_MODE2, (unsigned long *)arg);

			if (result)
				return result; 

		break;*/

		case LIRC_GET_SEND_MODE:
		
			result = put_user(LIRC_MODE_PULSE, (unsigned long *)arg);

			if (result)
				return result; 

		break;

/*		case LIRC_SET_REC_MODE:

			result = get_user(value, (unsigned long *)arg);

			if (result)
				return result;
				
			if (value != LIRC_MODE_MODE2)
				return -EINVAL;

		break;*/
		
		case LIRC_SET_SEND_CARRIER:

			result = get_user(value, (unsigned long *)arg);

			if (result)
				return result;

			avia_gt_ir_set_frequency(value);				
				
		break;
		
		case LIRC_SET_SEND_DUTY_CYCLE:

			result = get_user(value, (unsigned long *)arg);

			if (result)
				return result;
				
			avia_gt_ir_set_duty_cycle(value);

		break;
		
		case LIRC_SET_SEND_MODE:

			result = get_user(value, (unsigned long *)arg);

			if (result)
				return result;
				
			if (value != LIRC_MODE_PULSE)
				return -EINVAL;

		break;
		
		default:

			return -ENOIOCTLCMD;
			
		break;
			
	}

	return 0;

}

static ssize_t avia_gt_lirc_read(struct file *file, char *buf, size_t count, loff_t *f_pos)
{

	return count;

}

static ssize_t avia_gt_lirc_write(struct file *file, const char *buf, size_t count, loff_t *offset)
{

	u32 pulse_count;
	u32 pulse_nr;
	int result;

	if (count % sizeof(lirc_t))
		return -EINVAL;

	if (count > (AVIA_GT_IR_MAX_PULSE_COUNT * sizeof(lirc_t)))
		count = AVIA_GT_IR_MAX_PULSE_COUNT * sizeof(lirc_t);

	if (copy_from_user(pulse_buffer, buf, count))
		return -EFAULT;
	
	pulse_count = count / sizeof(lirc_t);

	for (pulse_nr = 0; pulse_nr < pulse_count; pulse_nr += 2) {
	
		if ((result = avia_gt_ir_queue_pulse(pulse_buffer[pulse_nr], pulse_buffer[pulse_nr + 1], !(file->f_flags & O_NONBLOCK))))
			return result;
			
	}

	return avia_gt_ir_send_buffer(!(file->f_flags & O_NONBLOCK));

}

static struct file_operations avia_gt_lirc_fops = {

	owner:	THIS_MODULE,
	ioctl:	avia_gt_lirc_ioctl,
	read:	avia_gt_lirc_read,
	write:	avia_gt_lirc_write,

};

static int __init avia_gt_lirc_init(void)
{

	printk("avia_gt_lirc: $Id: avia_gt_lirc.c,v 1.1 2002/05/11 20:23:22 Jolt Exp $\n");

	devfs_handle = devfs_register(NULL, "lirc", DEVFS_FL_DEFAULT, 0, 0, S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, &avia_gt_lirc_fops, NULL);

	if (!devfs_handle) {

		printk("avia_gt_lirc: error - can't register devfs handle\n");

		return -EIO;

	}

	avia_gt_ir_set_frequency(38000);				
	avia_gt_ir_set_duty_cycle(33);

	return 0;

}

static void __exit avia_gt_lirc_exit(void)
{

	devfs_unregister (devfs_handle);

}

#ifdef MODULE
module_init(avia_gt_lirc_init);
module_exit(avia_gt_lirc_exit);
#endif
