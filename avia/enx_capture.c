/*
 *   enx_capture.c - capture driver for enx (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2001 Florian Schirmer <jolt@tuxbox.org>
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
#include <linux/malloc.h>
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

#include <dbox/enx.h>
#include <dbox/enx_capture.h>

static int capture_open(struct inode *inode, struct file *file);
static ssize_t capture_read(struct file *file, char *buf, size_t count, loff_t *offset);
static int capture_release(struct inode *inode, struct file *file);
static int capture_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);

static unsigned char *enx_mem;
static int capt_buf_addr = 0xA0000;

static unsigned char capture_busy = 0;
static unsigned short input_height = 576;
static unsigned short input_width = 720;
static unsigned short input_x = 0;
static unsigned short input_y = 0;
static unsigned short line_stride = 360;
static unsigned short output_height = 288;
static unsigned short output_width = 360;

static int state = 0, frames;		 // 0: idle, 1: frame is capturing, 2: frame is done

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
		printk("enx_capture: open\n");
		return 0;
	default:
		return -ENODEV;
	}
  return 0;
}

static ssize_t capture_read(struct file *file, char *buf, size_t count, loff_t *offset)
{
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev), read, done;

	switch (minor)
	{
		case 0:
		{
			DECLARE_WAITQUEUE(wait, current);
			read=0;

			for(;;)
			{
				if (state!=2)		// frame not done
				{
					printk("enx_capture: no frame captured, waiting.. (data: %x)\n", enx_mem[capt_buf_addr]);
					if (file->f_flags & O_NONBLOCK)
						return read;

					add_wait_queue(&frame_wait, &wait);
					set_current_state(TASK_INTERRUPTIBLE);
					schedule();
					current->state = TASK_RUNNING;
					remove_wait_queue(&frame_wait, &wait);

					if (signal_pending(current))
					{
						printk("enx_capture: aborted. %x\n", enx_mem[capt_buf_addr]);
						return -ERESTARTSYS;
					}
					
					printk("enx_capture: ok\n");

					continue;
				}
				
				done=0;
				while (done*output_width < count) {
				    if ((done+1)*output_width > count)
					memcpy(buf+done*output_width, enx_mem+capt_buf_addr+done*line_stride, output_width);
				    else
					memcpy(buf+done*output_width, enx_mem+capt_buf_addr+done*line_stride, count-done*output_width);
				    done++;
				}    
				read=count;
				
				//enable_capture();

				break;
			}
			//printk("enx_capture: captured 0x%X bytes\n", read);
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
	    up(&lock_open);
	    return 0;
	}
    return -EINVAL;
}

static int capture_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{

/*    switch(cmd) {
    
	case AVIA_CAPTURE_:
	break;
	
    }*/
    
    int x, y;
    
    switch(cmd) {
    
	case 1:
    
	    x = arg & 0xFFFF;
	    y = (arg >> 16);
    
	    printk("enx_capture: DEBUG - HPOS=0x%X, EVPOS=0x%X\n", x, y);
    
	    enx_reg_s(VCP)->HPOS = x;
	    enx_reg_s(VCP)->EVPOS = y;
	    
	break;
	
	case 2:

	    x = arg & 0xFFFF;
	    y = (arg >> 16);
    
	    printk("enx_capture: DEBUG - HSIZE=0x%X, VSIZE=0x%X\n", x, y);
    
	    enx_reg_s(VCSZ)->HSIZE = x;
	    enx_reg_s(VCSZ)->VSIZE = y;
	
	break;    
	
    }	
    
    return 0;

//    return -ENODEV;
}

void enx_capture_interrupt(int reg, int no)
{
    if (state==1)
    {
	//printk("enx_capture: irq (state=0x%X, frames=0x%X)\n", state, frames);
	if (frames++>1)
	{
	    state=2;
	    wake_up(&frame_wait);
	}
    }
}

int enx_capture_start(unsigned char **capture_buffer, unsigned short *stride)
{
    unsigned short delta_x;
    unsigned short delta_y;
    unsigned char scale_x;
    unsigned char scale_y;

    if (capture_busy)
	return -EBUSY;
	
    printk("enx_capture: capture_start\n");
    
    scale_x = input_width / output_width;
    scale_y = input_height / output_height;
    delta_x = ((input_width / scale_x) - output_width) / 2;
    delta_y = ((input_height / scale_y) - output_height) / 2;
    line_stride = ((input_width / scale_x) + 3) & ~3;
    
    printk("enx_capture: input_width=%d, output_width=%d, scale_x=%d, delta_x=%d\n", input_width, output_width, scale_x, delta_x);
    printk("enx_capture: input_height=%d, output_height=%d, scale_y=%d, delta_y=%d\n", input_height, output_height, scale_y, delta_y);

#define BLANK_TIME 132
#define VIDCAP_PIPEDELAY 2

    enx_reg_s(VCP)->HPOS = ((BLANK_TIME - VIDCAP_PIPEDELAY) + input_x + delta_x) / 2;
    enx_reg_s(VCP)->EVPOS = 21 + input_y + delta_y;

    enx_reg_s(VCSZ)->HDEC = (input_width / output_width) - 1;
    enx_reg_s(VCSZ)->HSIZE = input_width / 2;
    enx_reg_s(VCSZ)->VDEC = (input_height / output_height) - 1;		
    enx_reg_s(VCSZ)->VSIZE = input_height / 2;

    enx_reg_s(VCSTR)->STRIDE = line_stride / 4;
    
    enx_reg_s(VCSA1)->Addr = capt_buf_addr >> 2;
    enx_reg_s(VCSA2)->Addr = (capt_buf_addr + ((input_width / scale_x) * (input_height / scale_y))) >> 2;

    enx_reg_s(VCSA1)->E = 1;
    
    printk("enx_capture: HDEC=%d, HSIZE=%d, VDEC=%d, VSIZE=%d, STRIDE=%d\n", enx_reg_s(VCSZ)->HDEC, enx_reg_s(VCSZ)->HSIZE, enx_reg_s(VCSZ)->VDEC, enx_reg_s(VCSZ)->VSIZE, enx_reg_s(VCSTR)->STRIDE);
    printk("enx_capture: VCSA1->Addr=0x%X, VCSA2->Addr=0x%X, Delta=%d\n", enx_reg_s(VCSA1)->Addr, enx_reg_s(VCSA2)->Addr, enx_reg_s(VCSA2)->Addr - enx_reg_s(VCSA1)->Addr);
    
    state=1;
    frames=0;
    capture_busy = 1;
    
    if (capture_buffer)
	//*capture_buffer = (unsigned char *)((((enx_reg_s(VCSA1)->Addr << 2) + (delta_x / 2) + ((delta_y / 2) * line_stride)) + 3) & ~3);
	*capture_buffer = (unsigned char *)(enx_reg_s(VCSA1)->Addr << 2);
	
    if (stride)
	*stride = line_stride;
    
    return 0;
}

void enx_capture_stop(void)
{
    if (capture_busy) {
	printk("enx_capture: capture_stop\n");
    
	enx_reg_s(VCSA1)->E = 0;
    
	state=0;
	capture_busy = 0;
    }	
}

int enx_capture_update_param(void)
{
    if (capture_busy)
	return -EBUSY;

    return 0;
}

int enx_capture_set_output(unsigned short width, unsigned short height)
{
    if (capture_busy)
	return -EBUSY;
	
    output_width = width;
    output_height = height;	
	
    return 0;
}

int enx_capture_set_input(unsigned short x, unsigned short y, unsigned short width, unsigned short height)
{
    if (capture_busy)
	return -EBUSY;

    input_x = x;
    input_y = y;
    input_width = width;
    input_height = height;	

    return 0;
}

int enx_capture_init(void)
{
    enx_mem = enx_get_mem_addr();
    
    enx_reg_w(RSTR0) &= ~(1 << 10);		// take video capture out of reset

    enx_reg_w(VCOFFS) = 0;
//    enx_reg_w(VCOFFS) |= ((SCALED_HSIZE * SCALED_VSIZE / 4) & 0x1FFFFC);

    enx_reg_s(VCSTR)->B = 0;				// Enable hardware double buffering

    enx_reg_s(VCSZ)->F = 1;   
    enx_reg_s(VCSZ)->B = 0;   
    
    if (enx_allocate_irq(0, 5, enx_capture_interrupt) < 0)	// VL1
    {
	printk("enx_capture: unable to get interrupt\n");
	return -EIO;
    }

    enx_reg_h(VLI1) = 0;	// at beginning of every frame.

    return 0;
}

void enx_capture_cleanup(void)
{
    enx_capture_stop();

    enx_free_irq(0, 5);
    enx_reg_w(RSTR0) |= (1 << 10);		
}

static int init_capture(void)
{
    printk("$Id: enx_capture.c,v 1.1 2001/10/23 08:49:35 Jolt Exp $\n");

    devfs_handle = devfs_register(NULL, "dbox/capture", DEVFS_FL_DEFAULT, 0, 0,	// <-- last 0 is the minor
				    S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
				    &capture_fops, NULL );
    if (!devfs_handle)
	return -EIO;

    init_waitqueue_head(&frame_wait);
  
    enx_capture_init();

    up(&lock_open);
    
    return 0;
}

static void __exit cleanup_capture(void)
{
    devfs_unregister(devfs_handle);
    
    enx_capture_cleanup();
    
    down(&lock_open);
}

EXPORT_SYMBOL(enx_capture_set_input);
EXPORT_SYMBOL(enx_capture_set_output);
EXPORT_SYMBOL(enx_capture_start);
EXPORT_SYMBOL(enx_capture_stop);

#ifdef MODULE
module_init(init_capture);
module_exit(cleanup_capture);
#endif
