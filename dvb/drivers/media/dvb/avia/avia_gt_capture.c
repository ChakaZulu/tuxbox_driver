/*
 *   avia_gt_capture.c - capture driver for eNX/GTX (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2001-2002 Florian Schirmer <jolt@tuxbox.org>
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
 *
 *   $Log: avia_gt_capture.c,v $
 *   Revision 1.5  2002/04/12 14:00:20  Jolt
 *   eNX/GTX merge
 *
 *
 *
 *   $Revision: 1.5 $
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
#include <dbox/avia_gt_capture.h>

static ssize_t capture_read(struct file *file, char *buf, size_t count, loff_t *offset);
static int capture_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);

unsigned char capture_chip_type;
static unsigned char *gt_mem;
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

DECLARE_WAIT_QUEUE_HEAD(capture_wait);

static devfs_handle_t devfs_handle;

static struct file_operations capture_fops = {
	owner:  	THIS_MODULE,
	read:   	capture_read,
	ioctl:  	capture_ioctl,
};

static ssize_t capture_read(struct file *file, char *buf, size_t count, loff_t *offset)
{
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev), read;
//	unsigned int done;

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
					printk("avia_gt_capture: no frame captured, waiting.. (data: %x)\n", gt_mem[capt_buf_addr]);
					if (file->f_flags & O_NONBLOCK)
						return read;

					add_wait_queue(&capture_wait, &wait);
					set_current_state(TASK_INTERRUPTIBLE);
					schedule();
					current->state = TASK_RUNNING;
					remove_wait_queue(&capture_wait, &wait);

					if (signal_pending(current))
					{
						printk("avia_gt_capture: aborted. %x\n", gt_mem[capt_buf_addr]);
						return -ERESTARTSYS;
					}
					
					printk("avia_gt_capture: ok\n");

					continue;
				}
				
				/*done=0;
				while (done*output_width < count) {
				    if ((done+1)*output_width > count)
					memcpy(buf+done*output_width, enx_mem+capt_buf_addr+done*line_stride, output_width);
				    else
					memcpy(buf+done*output_width, enx_mem+capt_buf_addr+done*line_stride, count-done*output_width);
				    done++;
				} */   
				memcpy(buf, gt_mem+capt_buf_addr, count);
				
				read=count;
				
				//enable_capture();

				break;
			}
			//printk("avia_gt_capture: captured 0x%X bytes\n", read);
			return read;
		}

		default:
			return -EINVAL;
	}
  return 0;
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
    
	    printk("avia_gt_capture: DEBUG - HPOS=0x%X, EVPOS=0x%X\n", x, y);
    
	    enx_reg_s(VCP)->HPOS = x;
	    enx_reg_s(VCP)->EVPOS = y;
	    
	break;
	
	case 2:

	    x = arg & 0xFFFF;
	    y = (arg >> 16);
    
	    printk("avia_gt_capture: DEBUG - HSIZE=0x%X, VSIZE=0x%X\n", x, y);
    
	    enx_reg_s(VCSZ)->HSIZE = x;
	    enx_reg_s(VCSZ)->VSIZE = y;
	
	break;    
	
    }	
    
    return 0;

//    return -ENODEV;
}

void avia_gt_capture_interrupt(unsigned char irq_reg, unsigned char irq_bit)
{
    if (state==1)
    {
	//printk("avia_gt_capture: irq (state=0x%X, frames=0x%X)\n", state, frames);
	if (frames++>1)
	{
	    state=2;
	    wake_up(&capture_wait);
	}
    }
}

int avia_gt_capture_start(unsigned char **capture_buffer, unsigned short *stride, unsigned short *odd_offset)
{
    unsigned short buffer_odd_offset;
    unsigned short capture_height;
    unsigned short capture_width;
    unsigned short delta_x;
    unsigned short delta_y;
    unsigned char scale_x;
    unsigned char scale_y;

    if (capture_busy)
	return -EBUSY;
	
    printk("avia_gt_capture: capture_start\n");
    
    scale_x = input_width / output_width;
//    scale_y = input_height / output_height / 2;
    scale_y = input_height / output_height;
    capture_height = input_height / scale_y;
    capture_width = input_width / scale_x;
    delta_x = (capture_width - output_width) / 2;
    delta_y = (capture_height - output_height) / 2;
    line_stride = (((input_width / scale_x) + 3) & ~3) * 2;
    buffer_odd_offset = (line_stride * (capture_height / 2)) + 100;
    buffer_odd_offset = line_stride / 2;

    printk("avia_gt_capture: input_width=%d, output_width=%d, scale_x=%d, delta_x=%d\n", input_width, output_width, scale_x, delta_x);
    printk("avia_gt_capture: input_height=%d, output_height=%d, scale_y=%d, delta_y=%d\n", input_height, output_height, scale_y, delta_y);

#define BLANK_TIME 132
#define VIDCAP_PIPEDELAY 2

    enx_reg_s(VCP)->HPOS = ((BLANK_TIME - VIDCAP_PIPEDELAY) + input_x + delta_x) / 2;
//    enx_reg_s(VCP)->OVOFFS = (scale_y - 1) / 2;
    enx_reg_s(VCP)->OVOFFS = 0;
    enx_reg_s(VCP)->EVPOS = 21 + ((input_y + delta_y) / 2);

    enx_reg_s(VCSZ)->HDEC = scale_x - 1;
    enx_reg_s(VCSZ)->HSIZE = input_width / 2;

    // If scale_y is even and greater then zero we get better results if we capture only the even fields
    // than if we scale down both fields
//    if ((scale_y > 0) && (!(scale_y & 0x01))) {

	enx_reg_s(VCSZ)->B = 0;   				// Even-only fields
//	enx_reg_s(VCSZ)->VDEC = (scale_y / 2) - 1;		
    
//    } else {

//	enx_reg_s(VCSZ)->B = 1;   				// Both fields
	enx_reg_s(VCSZ)->VDEC = scale_y - 1;		
    
//    }

    enx_reg_s(VCSZ)->VSIZE = input_height / 2;

    enx_reg_s(VCSTR)->STRIDE = line_stride / 4;

    enx_reg_s(VCOFFS)->Offset = buffer_odd_offset >> 2;
//    enx_reg_s(VCOFFS)->Offset = 0;
    
    enx_reg_s(VCSA1)->Addr = capt_buf_addr >> 2;
//    enx_reg_s(VCSA2)->Addr = (capt_buf_addr + (capture_width * capture_height)) >> 2;

    enx_reg_s(VCSA1)->E = 1;
    
    printk("avia_gt_capture: HDEC=%d, HSIZE=%d, VDEC=%d, VSIZE=%d, B=%d, STRIDE=%d\n", enx_reg_s(VCSZ)->HDEC, enx_reg_s(VCSZ)->HSIZE, enx_reg_s(VCSZ)->VDEC, enx_reg_s(VCSZ)->VSIZE, enx_reg_s(VCSZ)->B, enx_reg_s(VCSTR)->STRIDE);
    printk("avia_gt_capture: VCSA1->Addr=0x%X, VCSA2->Addr=0x%X, Delta=%d\n", enx_reg_s(VCSA1)->Addr, enx_reg_s(VCSA2)->Addr, enx_reg_s(VCSA2)->Addr - enx_reg_s(VCSA1)->Addr);
    
    state=1;
    frames=0;
    capture_busy = 1;
    
    if (capture_buffer)
	*capture_buffer = (unsigned char *)(enx_reg_s(VCSA1)->Addr << 2);
	
    if (stride)
	*stride = line_stride;
	
    if (odd_offset)
	*odd_offset = buffer_odd_offset;
    
    return 0;
}

void avia_gt_capture_stop(void)
{
    if (capture_busy) {
	printk("avia_gt_capture: capture_stop\n");
    
	enx_reg_s(VCSA1)->E = 0;
    
	state=0;
	capture_busy = 0;
    }	
}

int avia_gt_capture_update_param(void)
{
    if (capture_busy)
	return -EBUSY;

    return 0;
}

int avia_gt_capture_set_output(unsigned short width, unsigned short height)
{
    if (capture_busy)
	return -EBUSY;
	
    output_width = width;
    output_height = height;	
	
    return 0;
}

int avia_gt_capture_set_input(unsigned short x, unsigned short y, unsigned short width, unsigned short height)
{

    if (capture_busy)
	return -EBUSY;

    input_x = x;
    input_y = y;
    input_width = width;
    input_height = height;	

    return 0;
    
}

int __init avia_gt_capture_init(void)
{

    printk("avia_gt_capture: $Id: avia_gt_capture.c,v 1.5 2002/04/12 14:00:20 Jolt Exp $\n");

    devfs_handle = devfs_register(NULL, "dbox/capture", DEVFS_FL_DEFAULT, 0, 0,	// <-- last 0 is the minor
				    S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
				    &capture_fops, NULL );

    if (!devfs_handle)
	return -EIO;

    if (avia_gt_alloc_irq(0, 5, avia_gt_capture_interrupt) < 0) {
    
	printk("avia_gt_capture: unable to get interrupt\n");
	
	devfs_unregister(devfs_handle);

	return -EIO;
	
    }

    capture_chip_type = avia_gt_get_chip_type();

    if ((capture_chip_type != AVIA_GT_CHIP_TYPE_ENX) && (capture_chip_type != AVIA_GT_CHIP_TYPE_ENX)) {
    
        printk("avia_gt_pcm: Unsupported chip type\n");

	avia_gt_free_irq(0, 5);
	devfs_unregister(devfs_handle);
	    
        return -EIO;
		    
    }

    gt_mem = avia_gt_get_mem_addr();
    
    enx_reg_s(RSTR0)->VIDC = 1;		
    enx_reg_s(RSTR0)->VIDC = 0;		
    enx_reg_s(VCSTR)->B = 0;				// Enable hardware double buffering
    enx_reg_s(VCSZ)->F = 1;   				// Enable filter
    enx_reg_h(VLI1) = 0;	

    return 0;
}

void __exit avia_gt_capture_exit(void)
{

    devfs_unregister(devfs_handle);

    avia_gt_capture_stop();

    avia_gt_free_irq(0, 5);
    
    // Reset video capture
    enx_reg_s(RSTR0)->VIDC = 1;		
    
}

#ifdef MODULE
EXPORT_SYMBOL(avia_gt_capture_set_input);
EXPORT_SYMBOL(avia_gt_capture_set_output);
EXPORT_SYMBOL(avia_gt_capture_start);
EXPORT_SYMBOL(avia_gt_capture_stop);
#endif

#if defined(MODULE) && defined(STANDALONE)
module_init(avia_gt_capture_init);
module_exit(avia_gt_capture_exit);
#endif
