/*
 *   gtx_capture.c - capture driver for gtx (dbox-II-project)
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

#include <dbox/gtx.h>
#include <dbox/gtx_capture.h>

static int capture_open(struct inode *inode, struct file *file);
static ssize_t capture_read(struct file *file, char *buf, size_t count, loff_t *offset);
static int capture_release(struct inode *inode, struct file *file);
static int capture_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);

static unsigned char *gtx_mem;
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

int hpos_delta = 55;
int vpos_delta = 46;

static wait_queue_head_t frame_wait;
static DECLARE_MUTEX_LOCKED(lock_open);		// lock for open

static devfs_handle_t devfs_handle;

static struct file_operations gtx_capture_fops = {
	owner:  	THIS_MODULE,
	read:   	capture_read,
	ioctl:  	capture_ioctl,
	open:   	capture_open,
	release:	capture_release
};

static int capture_open(struct inode *inode, struct file *file)
{
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);

    return -EINVAL;	
	
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
		printk("gtx_capture: open\n");
		return 0;
	default:
		return -ENODEV;
	}
  return 0;
}

static ssize_t capture_read(struct file *file, char *buf, size_t count, loff_t *offset)
{
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev), read, done;

	return -EINVAL;

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
					printk("gtx_capture: no frame captured, waiting.. (data: %x)\n", gtx_mem[capt_buf_addr]);
					if (file->f_flags & O_NONBLOCK)
						return read;

					add_wait_queue(&frame_wait, &wait);
					set_current_state(TASK_INTERRUPTIBLE);
					schedule();
					current->state = TASK_RUNNING;
					remove_wait_queue(&frame_wait, &wait);

					if (signal_pending(current))
					{
						printk("gtx_capture: aborted. %x\n", gtx_mem[capt_buf_addr]);
						return -ERESTARTSYS;
					}
					
					printk("gtx_capture: ok\n");

					continue;
				}
				
				/*done=0;
				while (done*output_width < count) {
				    if ((done+1)*output_width > count)
					memcpy(buf+done*output_width, gtx_mem+capt_buf_addr+done*line_stride, output_width);
				    else
					memcpy(buf+done*output_width, gtx_mem+capt_buf_addr+done*line_stride, count-done*output_width);
				    done++;
				} */   
				memcpy(buf, gtx_mem+capt_buf_addr, count);
				
				read=count;
				
				//enable_capture();

				break;
			}
			//printk("gtx_capture: captured 0x%X bytes\n", read);
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
//FIXME	    up(&lock_open);
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
    
	    printk("gtx_capture: DEBUG - HPOS=0x%X, EVPOS=0x%X\n", x, y);
    
	    gtx_reg_s(VCSP)->HPOS = x;
	    gtx_reg_s(VCSP)->EVPOS = y;
	    
	break;
	
	case 2:

	    x = arg & 0xFFFF;
	    y = (arg >> 16);
    
	    printk("gtx_capture: DEBUG - HSIZE=0x%X, VSIZE=0x%X\n", x, y);
    
	    gtx_reg_s(VCS)->HSIZE = x;
	    gtx_reg_s(VCS)->VSIZE = y;
	
	break;    
	
    }	
    
    return 0;

//    return -ENODEV;
}

void gtx_capture_interrupt(int reg, int no)
{
    if (state==1)
    {
	//printk("gtx_capture: irq (state=0x%X, frames=0x%X)\n", state, frames);
	if (frames++>1)
	{
	    state=2;
//FIXME	    wake_up(&frame_wait);
	}
    }
}

int gtx_capture_start(unsigned char **capture_buffer, unsigned short *stride)
{
    unsigned short capture_height;
    unsigned short capture_width;
    unsigned short delta_x;
    unsigned short delta_y;
    unsigned char scale_x;
    unsigned char scale_y;

    if (capture_busy)
	return -EBUSY;
	
    printk("gtx_capture: capture_start\n");
    
    scale_x = input_width / output_width;
    scale_y = input_height / output_height;
//    scale_y = input_height / output_height / 2;
    capture_height = input_height / scale_y;
    capture_width = input_width / scale_x;
    delta_x = (capture_width - output_width) / 2;
    delta_y = (capture_height - output_height) / 2;
    line_stride = ((input_width / scale_x) + 3) & ~3;
    line_stride *= 2;

    printk("gtx_capture: input_width=%d, output_width=%d, scale_x=%d, delta_x=%d\n", input_width, output_width, scale_x, delta_x);
    printk("gtx_capture: input_height=%d, output_height=%d, scale_y=%d, delta_y=%d\n", input_height, output_height, scale_y, delta_y);


//    gtx_reg_s(VCSP)->HPOS = 63 + ((input_x + delta_x) / 2);
    gtx_reg_s(VCSP)->HPOS = hpos_delta + ((input_x + delta_x) / 2);		
//    gtx_reg_s(VCSP)->OVOFFS = (scale_y - 1) / 2;
    gtx_reg_s(VCSP)->OVOFFS = 0;
//    gtx_reg_s(VCSP)->EVPOS = 42 + ((input_y + delta_y) / 2);
    gtx_reg_s(VCSP)->EVPOS = vpos_delta + ((input_y + delta_y) / 2);	

    gtx_reg_s(VCS)->HDEC = scale_x - 1;
    gtx_reg_s(VCS)->HSIZE = input_width / 2;

    // If scale_y is even and greater then zero we get better results if we capture only the even fields
    // than if we scale down both fields
//    if ((scale_y > 0) && (!(scale_y & 0x01))) {

	gtx_reg_s(VCS)->B = 0;   				// Even-only fields
//	gtx_reg_s(VCS)->VDEC = (scale_y / 2) - 1;		
    
//    } else {

//	gtx_reg_s(VCS)->B = 1;   				// Both fields
	gtx_reg_s(VCS)->VDEC = scale_y - 1;		
    
//    }

    gtx_reg_s(VCS)->VSIZE = input_height / 2;

    gtx_reg_s(VCSA)->Addr = capt_buf_addr >> 1;
//    gtx_reg_s(VPSA)->Addr = (capt_buf_addr + (capture_width * capture_height)) >> 1;

    gtx_reg_s(VCSA)->E = 1;
    
    printk("gtx_capture: HDEC=%d, HSIZE=%d, VDEC=%d, VSIZE=%d, B=%d, STRIDE=%d\n", gtx_reg_s(VCS)->HDEC, gtx_reg_s(VCS)->HSIZE, gtx_reg_s(VCS)->VDEC, gtx_reg_s(VCS)->VSIZE, gtx_reg_s(VCS)->B, line_stride);
//    printk("gtx_capture: VCSA->Addr=0x%X, VPSA->Addr=0x%X, Delta=%d\n", gtx_reg_s(VCSA)->Addr, gtx_reg_s(VCSA)->Addr, gtx_reg_s(VCSA)->Addr - gtx_reg_s(VPSA)->Addr);
    
    state = 1;
    frames = 0;
    capture_busy = 1;
    
    if (capture_buffer)
	*capture_buffer = (unsigned char *)capt_buf_addr;
	
    if (stride)
	*stride = line_stride;
    
    return 0;
}

void gtx_capture_stop(void)
{
    if (capture_busy) {
	printk("gtx_capture: capture_stop\n");
    
	gtx_reg_s(VCSA)->E = 0;
    
	state = 0;
	capture_busy = 0;
    }	
}

int gtx_capture_update_param(void)
{
    if (capture_busy)
	return -EBUSY;

    return 0;
}

int gtx_capture_set_output(unsigned short width, unsigned short height)
{
    if (capture_busy)
	return -EBUSY;
	
    output_width = width;
    output_height = height;	
	
    return 0;
}

int gtx_capture_set_input(unsigned short x, unsigned short y, unsigned short width, unsigned short height)
{
    if (capture_busy)
	return -EBUSY;

    input_x = x;
    input_y = y;
    input_width = width;
    input_height = height;	

    return 0;
}

int gtx_capture_init(void)
{
    gtx_mem = gtx_get_mem_addr();
    
    gtx_reg_16(RR0) &= ~(1 << 14);			// Take video capture out of reset

    gtx_reg_s(VCS)->F = 1;   				// Enable filter
    gtx_reg_s(VCS)->B = 0;				// Enable hardware double buffering
    
    if (gtx_allocate_irq(1, 13, gtx_capture_interrupt) < 0)	// VL1
    {
	printk("gtx_capture: unable to get interrupt\n");
	return -EIO;
    }

    gtx_reg_16(VLI1) = 0;	// at beginning of every frame.

    return 0;
}

void gtx_capture_cleanup(void)
{
    gtx_capture_stop();

    gtx_free_irq(1, 13);
    gtx_reg_16(RR0) |= (1 << 14);		
}

static int init_capture(void)
{
    printk("$Id: gtx_capture.c,v 1.3 2001/12/01 06:37:06 gillem Exp $\n");

    devfs_handle = devfs_register(NULL, "dbox/capture", DEVFS_FL_DEFAULT, 0, 0, S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, &gtx_capture_fops, NULL);

    if (!devfs_handle)
	return -EIO;

//FIXME    init_waitqueue_head(&frame_wait);
  
    gtx_capture_init();

//FIXME    up(&lock_open);
    
    return 0;
}

static void __exit cleanup_capture(void)
{
    devfs_unregister(devfs_handle);
    
    gtx_capture_cleanup();
    
//FIXME    down(&lock_open);
}

EXPORT_SYMBOL(gtx_capture_set_input);
EXPORT_SYMBOL(gtx_capture_set_output);
EXPORT_SYMBOL(gtx_capture_start);
EXPORT_SYMBOL(gtx_capture_stop);

#ifdef MODULE
MODULE_PARM(hpos_delta, "i");
MODULE_PARM(vpos_delta, "i");
module_init(init_capture);
module_exit(cleanup_capture);
#endif
