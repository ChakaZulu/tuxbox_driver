/*
 * $Id: avia_gt_capture.c,v 1.32 2003/09/30 05:45:35 obi Exp $
 * 
 * capture driver for eNX/GTX (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2001-2002 Florian Schirmer <jolt@tuxbox.org>
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
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/devfs_fs_kernel.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>

#include "avia_gt.h"
#include "avia_gt_capture.h"

#ifndef CONFIG_DEVFS_FS
#error no devfs
#endif

static int capt_buf_addr = AVIA_GT_MEM_CAPTURE_OFFS;
static sAviaGtInfo *gt_info;

static ssize_t capture_read(struct file *file, char *buf, size_t count, loff_t *offset);
static int capture_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int capture_open(struct inode *inode, struct file *file);
static int capture_release(struct inode *inode, struct file *file);

static unsigned char capture_busy;
static unsigned int captured_frames;
static unsigned short input_height = 576;
static unsigned short input_width = 720;
static unsigned short input_x;
static unsigned short input_y;
static unsigned short line_stride = 360;
static unsigned short output_height = 288;
static unsigned short output_width = 360;

DECLARE_WAIT_QUEUE_HEAD(capture_wait);

static devfs_handle_t devfs_handle;

static struct file_operations capture_fops = {
	owner:	 THIS_MODULE,
	read:	 capture_read,
	ioctl:	 capture_ioctl,
	open:	 capture_open,
	release: capture_release
};

static int capture_open(struct inode *inode, struct file *file)
{
	if ( capture_busy )
		return -EBUSY;

	MOD_INC_USE_COUNT;

	return 0;
}

static int capture_release(struct inode *inode, struct file *file)
{
	avia_gt_capture_stop();
	MOD_DEC_USE_COUNT;

	return 0;
}

static ssize_t capture_read(struct file *file, char *buf, size_t count, loff_t *offset)
{
	unsigned max_count = (unsigned)0;

	if (!capture_busy)
		avia_gt_capture_start(NULL, NULL);

	if (file->f_flags & O_NONBLOCK)
		return -EWOULDBLOCK;

	captured_frames = 0;
    
	if (count > (max_count = line_stride * output_height) )
		count = max_count;

	wait_event_interruptible(capture_wait, captured_frames);

	if (copy_to_user(buf, gt_info->mem_addr + capt_buf_addr, count))
		return -EFAULT;

	return count;
}

static int capture_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned short stride = (unsigned short)0;
	int result = 0;

	switch(cmd) {
	case AVIA_GT_CAPTURE_START:
		result = avia_gt_capture_start(NULL, &stride);
		if (result < 0)
			return result;
		else
			return stride;
	case AVIA_GT_CAPTURE_STOP:
		avia_gt_capture_stop();
		return 0;
	case AVIA_GT_CAPTURE_SET_INPUT_POS:
		return avia_gt_capture_set_input_pos(arg & 0xFFFF, (arg & 0xFFFF0000) >> 16);
	case AVIA_GT_CAPTURE_SET_INPUT_SIZE:
		return avia_gt_capture_set_input_size(arg & 0xFFFF, (arg & 0xFFFF0000) >> 16);
	case AVIA_GT_CAPTURE_SET_OUTPUT_SIZE:
		return avia_gt_capture_set_output_size(arg & 0xFFFF, (arg & 0xFFFF0000) >> 16);
	}	
 
	return -EINVAL;
}

void avia_gt_capture_interrupt(unsigned short irq)
{
	unsigned char field = 0;

	if (avia_gt_chip(ENX))
		field = enx_reg_s(VLC)->F;
	else if (avia_gt_chip(GTX))
		field = gtx_reg_s(VLC)->F;

	captured_frames++;

	wake_up_interruptible(&capture_wait);
}

int avia_gt_capture_start(unsigned char **capture_buffer, unsigned short *stride)
{
	u16 capture_width;
	u16 capture_height;

	u8 scale_x;
	u8 scale_y;

	if (capture_busy)
		return -EBUSY;

	scale_x = input_width / output_width;
	scale_y = input_height / output_height;

	if ((scale_x == 0) || (scale_y == 0))
		return -EINVAL;

	capture_height = input_height / scale_y;
	capture_width = input_width / scale_x;
	line_stride = (capture_width + 3) & ~3;

#define BLANK_TIME 132
#define VIDCAP_PIPEDELAY 2

	if (avia_gt_chip(ENX))
		enx_reg_set(VCP, HPOS, ((BLANK_TIME - VIDCAP_PIPEDELAY) + input_x) / 2);
	else if (avia_gt_chip(GTX))
		gtx_reg_set(VCP, HPOS, (96 + input_x) / 2);	/* FIXME: Verify VIDCAP_PIPEDELAY for GTX */

	avia_gt_reg_set(VCP, OVOFFS, (scale_y - 1) / 2);
	avia_gt_reg_set(VCP, EVPOS, 21 + (input_y / 2));
	avia_gt_reg_set(VCSZ, HDEC, scale_x - 1);
	avia_gt_reg_set(VCSZ, HSIZE, input_width / 2);
	avia_gt_reg_set(VCSZ, VDEC, scale_y - 1);
	avia_gt_reg_set(VCSZ, VSIZE, input_height / 2);

	if (avia_gt_chip(ENX))
		enx_reg_set(VCOFFS, Offset, line_stride >> 2);

	// If scale_y is even and greater then zero we get better results if we capture only the even fields
	// than if we scale down both fields
	if ((scale_y > 0) && (!(scale_y & 0x01))) {
		avia_gt_reg_set(VCSZ, B, 0);	/* even fields */
		if (avia_gt_chip(ENX))
			enx_reg_set(VCSTR, STRIDE, line_stride >> 2);
	}
	else {
		avia_gt_reg_set(VCSZ, B, 1);	/* both fields */
		if (avia_gt_chip(ENX))
			enx_reg_set(VCSTR, STRIDE, (line_stride * 2) >> 2);
	}

	if (avia_gt_alloc_irq(gt_info->irq_vl1, avia_gt_capture_interrupt) < 0)
		return -EIO;

	if (avia_gt_chip(ENX))
		enx_reg_set(VCSA1, Addr, capt_buf_addr >> 2);
	else if (avia_gt_chip(GTX))
		gtx_reg_set(VCSA1, Addr, capt_buf_addr >> 1);

	avia_gt_reg_set(VCSA1, E, 1);
    
	capture_busy = 1;
	captured_frames = 0;
    
	if (capture_buffer)
		*capture_buffer = (unsigned char *)capt_buf_addr;
	
	if (stride)
		*stride = line_stride;
	
	return 0;
	
}

void avia_gt_capture_stop(void)
{
	if (capture_busy) {
		avia_gt_reg_set(VCSA1, E, 0);
		avia_gt_free_irq(gt_info->irq_vl1);
		capture_busy = 0;
	}
}

int avia_gt_capture_set_output_size(u16 width, u16 height)
{
	if (capture_busy)
		return -EBUSY;
	
	output_width = width;
	output_height = height;	
	
	return 0;
}

int avia_gt_capture_set_input_pos(u16 x, u16 y)
{
	if (capture_busy)
		return -EBUSY;

	input_x = x;
	input_y = y;

	return 0;
}

int avia_gt_capture_set_input_size(u16 width, u16 height)
{
	if (capture_busy)
		return -EBUSY;

	input_width = width;
	input_height = height;	

	return 0;
}

void avia_gt_capture_reset(int reenable)
{
	avia_gt_reg_set(RSTR0, VIDC, 1);

	if (reenable)
		avia_gt_reg_set(RSTR0, VIDC, 0);
}

int __init avia_gt_capture_init(void)
{
	printk(KERN_INFO "avia_gt_capture: $Id: avia_gt_capture.c,v 1.32 2003/09/30 05:45:35 obi Exp $\n");

	gt_info = avia_gt_get_info();

	if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {
		printk(KERN_ERR "avia_gt_capture: Unsupported chip type\n");
		return -EIO;
	}
 
	devfs_handle = devfs_register(NULL, "dbox/capture0", DEVFS_FL_DEFAULT, 0, 0,	// <-- last 0 is the minor
					S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
					&capture_fops, NULL);

	if (!devfs_handle)
		return -EIO;

	avia_gt_capture_reset(1);

	if (avia_gt_chip(ENX)) {
		enx_reg_set(VCP, U, 0);		// Using squashed mode
		enx_reg_set(VCSTR, B, 0);	// Hardware double buffering
	}
#if 0
	/* FIXME */
	else if (avia_gt_chip(GTX)) {
		gtx_reg_set(VCS, B, 0);		// Hardware double buffering

	}
#endif
	avia_gt_reg_set(VCSZ, F, 1);	/*  Filter */

	avia_gt_reg_set(VLI1, F, 1);
	avia_gt_reg_set(VLI1, E, 1);
	avia_gt_reg_set(VLI1, LINE, 0);

	return 0;
}

void __exit avia_gt_capture_exit(void)
{
	devfs_unregister(devfs_handle);
	avia_gt_capture_stop();
	avia_gt_capture_reset(0);
}

#if defined(STANDALONE)
module_init(avia_gt_capture_init);
module_exit(avia_gt_capture_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("AViA eNX/GTX capture driver");
MODULE_LICENSE("GPL");
#endif

EXPORT_SYMBOL(avia_gt_capture_set_input_pos);
EXPORT_SYMBOL(avia_gt_capture_set_input_size);
EXPORT_SYMBOL(avia_gt_capture_set_output_size);
EXPORT_SYMBOL(avia_gt_capture_start);
EXPORT_SYMBOL(avia_gt_capture_stop);
