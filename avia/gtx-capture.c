/*
 *   gtx-capture.c - capture driver for gtx (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2001 Felix Domke <tmbinc@gmx.net>
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
 *   $Log: gtx-capture.c,v $
 *   Revision 1.3  2002/04/12 23:20:25  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.2  2001/12/01 06:37:06  gillem
 *   - malloc.h -> slab.h
 *
 *   Revision 1.1  2001/09/17 12:24:36  tmbinc
 *   added gtx-capture, very alpha
 *
 *   $Revision: 1.3 $
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
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <linux/devfs_fs_kernel.h>

#ifndef CONFIG_DEVFS_FS
#error no devfs
#endif

#include <dbox/avia_gt.h>

static int capture_open(struct inode *inode, struct file *file);
static ssize_t capture_read(struct file *file, char *buf, size_t count, loff_t *offset);
static int capture_release(struct inode *inode, struct file *file);
static int capture_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);

static void enable_capture(void);
static void disable_capture(void);

static unsigned char *gtxmem, *gtxreg;
static int gtx_addr=0xA0000, state=0, frames, read_offset;		// 0: idle, 1: frame is capturing, 2: frame is done

static wait_queue_head_t frame_wait;
static DECLARE_MUTEX_LOCKED(lock_open);		// lock for open

static devfs_handle_t devfs_handle;

static struct file_operations capture_fops = {
	owner:  	THIS_MODULE,
	read:   	capture_read,
	ioctl:  	capture_ioctl,
	open:   	capture_open,
	release:	capture_release
};

static int capture_open(struct inode *inode, struct file *file)
{
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);
	switch (minor)
	{
	case 0:
		if (file->f_flags & O_NONBLOCK)
		{
			if (down_trylock(&lock_open))
				return -EAGAIN;
		}	else
		{
			if (down_interruptible(&lock_open))
				return -ERESTARTSYS;
		}
		printk("open\n");
		enable_capture();
		return 0;
	default:
		return -ENODEV;
	}
  return 0;
}

static ssize_t capture_read(struct file *file, char *buf, size_t count, loff_t *offset)
{
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev), read;

	switch (minor)
	{
		case 0:
		{
			DECLARE_WAITQUEUE(wait, current);
			read=0;

			for(;;)
			{
				if (state!=2)		// frame not fone
				{
					printk("no frame captured, waiting.. (data: %x)\n", gtxmem[gtx_addr]);
					if (file->f_flags & O_NONBLOCK)
						return read;

					add_wait_queue(&frame_wait, &wait);
					set_current_state(TASK_INTERRUPTIBLE);
					schedule();
					current->state = TASK_RUNNING;
					remove_wait_queue(&frame_wait, &wait);

					if (signal_pending(current))
					{
						printk("aborted. %x\n", gtxmem[gtx_addr]);
						return -ERESTARTSYS;
					}
					
					printk("ok\n");

					continue;
				}
				
				memcpy(buf, gtxmem+gtx_addr, count);
				read=count;
				
				enable_capture();

				break;
			}

			return read;
		}

		default:
			return -EINVAL;
	}
  return 0;
}

static int capture_release(struct inode *inode, struct file *file)
{
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);

	switch (minor)
	{
	case 0:
		disable_capture();
		up(&lock_open);
		return 0;
	}
	return -EINVAL;
}

static int capture_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	return -ENODEV;
}

static void vl0_interrupt(int reg, int no)
{
	if (state==1)
	{
		if (frames++>1)
		{
			state=2;
			wake_up(&frame_wait);
		}
	}
}

static void enable_capture(void)
{
	printk("enable capture (%x)\n", gtxmem[gtx_addr]=0);
	state=1;
	frames=0;
	rw(VSCP)=(48<<17)|(22<<1);
	rw(VCS)=(3<<28) | 	// decimation factor: 2:1
	        (320<<17) | // HSIZE
	        (1<<16) |		// horizontal filter
	        (3<<12) |		// Vertical decimation 4:1
	        (288<<1) |	// VSIZE
	        (0);
	rw(VSCA)=1|gtx_addr;
}

static void disable_capture(void)
{
	printk("disable capture\n");
	rw(VSCA)=gtx_addr;	// enable not set.
	state=0;
}

static int init_capture(void)
{
	printk("$Id: gtx-capture.c,v 1.3 2002/04/12 23:20:25 Jolt Exp $\n");
	
	
  gtxmem = gtx_get_mem();
  gtxreg = gtx_get_reg();

	rh(RR0)&=1<<14;		// take video capture out of reset
	init_waitqueue_head(&frame_wait);

  if (!gtxmem)
    return -1;

  if (gtx_allocate_irq(1, 11, vl0_interrupt ) < 0 )	// VL0 
  {
    printk("gtx-capture.o: unable to get interrupt\n");
    return -EIO;
  }

  rw(VLI1)=0;	// at beginning of every frame.
  
  devfs_handle=devfs_register ( NULL, "dbox/capture", DEVFS_FL_DEFAULT, 0, 0,	// <-- last 0 is the minor
		S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
		&capture_fops, NULL );
	if (!devfs_handle)
	{
		gtx_free_irq(1, 14);
		return -EIO;
	}
	up(&lock_open);

	return 0;
}

static void __exit cleanup_capture(void)
{
  devfs_unregister(devfs_handle);
	down(&lock_open);
	gtx_free_irq(1, 11);
}

#ifdef MODULE
module_init(init_capture);
module_exit(cleanup_capture);
#endif
