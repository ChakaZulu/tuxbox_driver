/*
 *   gtx_pig.c - pig driver for AViA gtx (dbox-II-project)
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

#include <dbox/avia_gt.h>
#include <dbox/gtx_capture.h>
#include <dbox/avia_gt_pig.h>

#define GTX_PIG_COUNT 1

extern int gtx_pig_hide(unsigned char pig_nr);
extern int gtx_pig_set_pos(unsigned char pig_nr, unsigned short x, unsigned short y);
extern int gtx_pig_set_size(unsigned char pig_nr, unsigned short width, unsigned short height, unsigned char stretch);
extern int gtx_pig_set_stack(unsigned char pig_nr, unsigned char stack_order);
extern int gtx_pig_show(unsigned char pig_nr);

//#define CAPTURE_WIDTH 720
#define CAPTURE_WIDTH 640
#define CAPTURE_HEIGHT 576
//#define PIG_WIDTH (160*1)
//#define PIG_HEIGHT (72*1)
#define PIG_WIDTH (CAPTURE_WIDTH / 2)
#define PIG_HEIGHT (CAPTURE_HEIGHT / 2)

static devfs_handle_t devfs_handle[GTX_PIG_COUNT];
static unsigned char pig_busy[GTX_PIG_COUNT] = {0};
static unsigned char *pig_buffer[GTX_PIG_COUNT] = {NULL};
static unsigned short pig_stride[GTX_PIG_COUNT] = {0};
static unsigned int pig_offset[GTX_PIG_COUNT] = {0};

static int gtx_pig_open(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t gtx_pig_read(struct file *file, char *buf, size_t count, loff_t *offset)
{
    return 0;
}

static int gtx_pig_release(struct inode *inode, struct file *file)
{
    return 0;
}

static int gtx_pig_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    unsigned char pig_nr = (unsigned char)MINOR(file->f_dentry->d_inode->i_rdev);
    
    if (pig_nr >= GTX_PIG_COUNT)
	return -ENODEV;

    switch(cmd) {
    
	case AVIA_PIG_HIDE:

	    gtx_pig_hide(pig_nr);

	break;

	case AVIA_PIG_SET_POS:

	    return gtx_pig_set_pos(pig_nr, (unsigned short)((arg >> 16) & 0xFFFF), (unsigned short)(arg & 0xFFFF));

	break;

	case AVIA_PIG_SET_SIZE:

	    return gtx_pig_set_size(pig_nr, (unsigned short)((arg >> 16) & 0xFFFF), (unsigned short)(arg & 0xFFFF), 0);

	break;

	case AVIA_PIG_SET_STACK:

	    return gtx_pig_set_stack(pig_nr, (unsigned char)(arg & 0xFF));

	break;
	
	case AVIA_PIG_SHOW:

	    gtx_pig_show(pig_nr);

	break;

    }

    return 0;
}

static ssize_t gtx_pig_write(struct file *file, const char *buf, size_t count, loff_t *offset) 
{
    unsigned char pig_nr = (unsigned char)MINOR(file->f_dentry->d_inode->i_rdev);
    unsigned char *kbuf;
    unsigned char *vbuf;
    unsigned int todo;
    
    if (pig_nr >= GTX_PIG_COUNT)
	return -ENODEV;

    if (!pig_busy[pig_nr])
	return -EIO;

    /*if ((kbuf = kmalloc(count, GFP_KERNEL)) == NULL )
	return -ENOMEM;

    if (copy_from_user(kbuf, buf, count) ) {
    
	kfree(kbuf);
	return -EFAULT;
	
    }
    
    vbuf = pig_buffer[pig_nr];
    
    while (count > 0) {
    
	count -= PIG_WIDTH;
	vbuf += pig_stride[pig_nr];
    
    }
	
    kfree(kbuf);*/
    
    if ((pig_offset[pig_nr] + count) >= (PIG_WIDTH * PIG_HEIGHT)) {
	todo = (pig_offset[pig_nr] + count) - (PIG_WIDTH * PIG_HEIGHT);
	count -= todo;
    } else {
	todo = 0;
    }
    
    kbuf = avia_gt_get_mem_addr();
    kbuf = (unsigned char *)(((unsigned int)kbuf) + ((unsigned int)(pig_buffer[pig_nr])) + (pig_offset[pig_nr]));
    
    if (copy_from_user(kbuf, buf, count)) {
	printk("gtx_pig: copy_from_user failed\n");
	return -EFAULT;
    }

    pig_offset[pig_nr] += count;
    
    if (todo) {
        pig_offset[pig_nr] = 0;
	count = todo;
	
        kbuf = avia_gt_get_mem_addr();
	kbuf = (unsigned char *)(((unsigned int)kbuf) + ((unsigned int)(pig_buffer[pig_nr])) + (pig_offset[pig_nr]));
    
	if (copy_from_user(kbuf, buf, count)) {
	    printk("gtx_pig: copy_from_user failed\n");
	    return -EFAULT;
	}
	
	pig_offset[pig_nr] += count;
    }
    
    printk("gtx_pig: wrote %d bytes\n", count);

    return 0;
}

static struct file_operations gtx_pig_fops = {
	owner:  	THIS_MODULE,
	read:   	gtx_pig_read,
	ioctl:  	gtx_pig_ioctl,
	open:   	gtx_pig_open,
	release:	gtx_pig_release,
	write:		gtx_pig_write
};

int gtx_pig_hide(unsigned char pig_nr)
{
    if (pig_nr >= GTX_PIG_COUNT)
	return -ENODEV;

    if (pig_busy[pig_nr]) {

	gtx_reg_s(VPSA)->E = 0;

	gtx_capture_stop();
    
	pig_busy[pig_nr] = 0;
	
    }	
    
    return 0;
}

int gtx_pig_set_pos(unsigned char pig_nr, unsigned short x, unsigned short y)
{
    if (pig_nr >= GTX_PIG_COUNT)
	return -ENODEV;

    //TODO check for S
    gtx_reg_s(VPP)->HPOS = 63 + (x / 2);
    gtx_reg_s(VPP)->VPOS = 21 + y;
    
    return 0;
}

int gtx_pig_set_stack(unsigned char pig_nr, unsigned char stack_order)
{
    if (pig_nr >= GTX_PIG_COUNT)
	return -ENODEV;
	
    gtx_reg_s(VPS)->P = stack_order;							
    
    return 0;
}

int gtx_pig_set_size(unsigned char pig_nr, unsigned short width, unsigned short height, unsigned char stretch)
{
    int result;

    if (pig_nr >= GTX_PIG_COUNT)
	return -ENODEV;
	
    if (pig_busy[pig_nr])
	return -EBUSY;
	
    result = gtx_capture_set_output(width, height);
    
    if (result < 0)
	return result;

    gtx_reg_s(VPS)->WIDTH = width / 2;
    gtx_reg_s(VPS)->S = stretch;
    gtx_reg_s(VPS)->HEIGHT = height / 2;
    
    printk("gtx_pig: WIDTH=%d, S=%d, HEIGHT=%d\n", gtx_reg_s(VPS)->WIDTH, gtx_reg_s(VPS)->S, gtx_reg_s(VPS)->HEIGHT);
    
    return 0;
}

int gtx_pig_show(unsigned char pig_nr)
{
    if (pig_nr >= GTX_PIG_COUNT)
	return -ENODEV;
	
    if (pig_busy[pig_nr])
	return -EBUSY;

    gtx_capture_set_input(0, 0, CAPTURE_WIDTH, CAPTURE_HEIGHT);
    gtx_capture_start(&pig_buffer[pig_nr], &pig_stride[pig_nr]);

    printk("gtx_pig: buffer=0x%X, stride=0x%X\n", (unsigned int)pig_buffer[pig_nr], pig_stride[pig_nr]);

//    gtx_reg_s(VPO)->OFFSET = ((unsigned int)(pig_stride[pig_nr])) >> 2;
    gtx_reg_s(VPO)->OFFSET = 0;
    gtx_reg_s(VPO)->STRIDE = ((unsigned int)(pig_stride[pig_nr])) >> 2;
    gtx_reg_s(VPO)->B = 0;								// Enable hardware double buffering
    
    gtx_reg_s(VPSA)->Addr = ((unsigned int)(pig_buffer[pig_nr])) >> 1;			// Set buffer address (for non d-buffer mode)
//    gtx_reg_s(VPSA)->Addr = ((unsigned int)(pig_buffer[pig_nr] + (pig_stride[pig_nr] * PIG_HEIGHT))) >> 1;			// Set buffer address (for d-buffer mode)

    gtx_reg_s(VPP)->F = 0;

    gtx_reg_s(VPSA)->E = 1;

    pig_busy[pig_nr] = 1;

    return 0;
}

static int gtx_pig_init(void)
{
    unsigned char pig_nr = 0;
    
    printk("gtx_pig: init\n");
    
    devfs_handle[pig_nr] = devfs_register(NULL, "dbox/pig0", DEVFS_FL_DEFAULT, 0, 0, S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, &gtx_pig_fops, NULL);
    
    if (!devfs_handle[pig_nr])
	return -EIO;
    
    gtx_reg_16(RR0) &= ~(1 << 15);

    gtx_pig_set_pos(pig_nr, 150, 50);
    gtx_pig_set_size(pig_nr, PIG_WIDTH, PIG_HEIGHT, 0);
    gtx_pig_set_stack(pig_nr, 1);
    
    //gtx_pig_show(pig_nr);

    return 0;
}

static void __exit gtx_pig_cleanup(void)
{
    unsigned char pig_nr = 0;
    
    printk("gtx_pig: cleanup\n");
    
    gtx_pig_hide(pig_nr);
    
    gtx_reg_16(RR0) |= (1 << 15);

    devfs_unregister(devfs_handle[pig_nr]);
}

#ifdef MODULE
module_init(gtx_pig_init);
module_exit(gtx_pig_cleanup);
#endif
