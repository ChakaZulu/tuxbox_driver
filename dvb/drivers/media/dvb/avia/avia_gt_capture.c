/*
 * $Id: avia_gt_capture.c,v 1.29 2003/07/24 01:59:21 homar Exp $
 * 
 * capture driver for eNX/GTX (dbox-II-project)
 *
 * Homepage: http://dbox2.elxsi.de
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
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>

#include "avia_gt.h"
#include "avia_gt_capture.h"

//#undef dprintk
//#define dprintk printk

static int capt_buf_addr = AVIA_GT_MEM_CAPTURE_OFFS;
static sAviaGtInfo *gt_info;

static unsigned char capture_busy = 0;
static unsigned short capture_irq = 0;
static unsigned int captured_frames = 0;
static unsigned short input_height = 576;
static unsigned short input_width = 720;
static unsigned short input_x = 0;
static unsigned short input_y = 0;
static unsigned short line_stride = 360;
static unsigned short output_height = 288;
static unsigned short output_width = 360;

DECLARE_WAIT_QUEUE_HEAD(capture_wait);

void avia_gt_capture_interrupt(unsigned short irq)
{

	unsigned char field = 0;

//	printk("avia_gt_capture: irq\n");
//	printk("L%dF%d ", enx_reg_s(VLC)->LINE, enx_reg_s(VLC)->F);
    
	if (avia_gt_chip(ENX))
		field = enx_reg_s(VLC)->F;
	else if (avia_gt_chip(GTX))
		field = gtx_reg_s(VLC)->F;
	
	captured_frames++;

	wake_up_interruptible(&capture_wait);
    
}

int avia_gt_capture_start(unsigned char **capture_buffer, unsigned short *stride,unsigned char pig)
{
	u16 capture_width;
	u16 capture_height;

	u8 scale_x;
	u8 scale_y;

	if (capture_busy)
		return -EBUSY;
	
	dprintk("avia_gt_capture: capture_start\n");
    
	scale_x = input_width / output_width;
	scale_y = input_height / output_height;

	if ((scale_x == 0) || (scale_y == 0))
		return -EINVAL;

	capture_height = input_height / scale_y;
	capture_width = input_width / scale_x;
	line_stride = (capture_width + 3) & ~3;

	dprintk("avia_gt_capture: input_width=%d, capture_width=%d, output_width=%d, scale_x=%d\n", input_width, capture_width, output_width, scale_x);
	dprintk("avia_gt_capture: input_height=%d, capture_height=%d, output_height=%d, scale_y=%d\n", input_height, capture_height, output_height, scale_y);
	dprintk("avia_gt_capture: input_x=%d, input_y=%d, line_stride=%d\n", input_x, input_y, line_stride);
    
	if (avia_gt_chip(ENX)) {

#define BLANK_TIME 132
#define VIDCAP_PIPEDELAY 2

		enx_reg_set(VCP, HPOS, ((BLANK_TIME - VIDCAP_PIPEDELAY) + input_x) / 2);
		enx_reg_set(VCP, OVOFFS, (scale_y - 1) / 2);
		enx_reg_set(VCP, EVPOS, 21 + (input_y / 2));

		enx_reg_set(VCSZ, HDEC, scale_x - 1);
		enx_reg_set(VCSZ, HSIZE, input_width / 2);
		enx_reg_set(VCSZ, VDEC, scale_y - 1);		
		enx_reg_set(VCSZ, VSIZE, input_height / 2);

		enx_reg_set(VCOFFS, Offset, line_stride >> 2);

	} else if (avia_gt_chip(GTX)) {

//		gtx_reg_set(VCSP, HPOS, ((BLANK_TIME - VIDCAP_PIPEDELAY) + input_x) / 2); // Verify VIDCAP_PIPEDELAY for GTX
		gtx_reg_set(VCSP, HPOS, (96 + input_x) / 2); 
		gtx_reg_set(VCSP, OVOFFS, (scale_y - 1) / 2);
		gtx_reg_set(VCSP, EVPOS, 21 + (input_y / 2));
	
		gtx_reg_set(VCS, HDEC, scale_x - 1);
		gtx_reg_set(VCS, HSIZE, input_width / 2);
		gtx_reg_set(VCS, VDEC, scale_y - 1);
		gtx_reg_set(VCS, VSIZE, input_height / 2);

	}
    
	// If scale_y is even and greater then zero we get better results if we capture only the even fields
	// than if we scale down both fields
	if ((scale_y > 0) && (!(scale_y & 0x01))) {

		if (avia_gt_chip(ENX)) {
	
			enx_reg_set(VCSZ, B, 0);   								// Even-only fields
			enx_reg_set(VCSTR, STRIDE, line_stride >> 2);

		} else if (avia_gt_chip(GTX)) {

			gtx_reg_set(VCS, B, 0);                                  // Even-only fields
			
		}
	
	} else {

		if (avia_gt_chip(ENX)) {

			enx_reg_set(VCSZ, B, 1);   								// Both fields
			enx_reg_set(VCSTR, STRIDE, (line_stride * 2) >> 2);
	    
		} else if (avia_gt_chip(GTX)) {
		
			gtx_reg_set(VCS, B, 1);	// Both fields
			
		}
		
	}

	if (avia_gt_alloc_irq(capture_irq, avia_gt_capture_interrupt) < 0)
		return -EIO;

	if (avia_gt_chip(ENX)) {

		enx_reg_set(VCSA1, Addr, capt_buf_addr >> 2);
		//enx_reg_set(VCSA2, Addr, (capt_buf_addr + (capture_width * capture_height)) >> 2);

		enx_reg_set(VCSA1, E, 1);

		dprintk("avia_gt_capture: HDEC=%d, HSIZE=%d, VDEC=%d, VSIZE=%d, B=%d, STRIDE=%d\n", enx_reg_s(VCSZ)->HDEC, enx_reg_s(VCSZ)->HSIZE, enx_reg_s(VCSZ)->VDEC, enx_reg_s(VCSZ)->VSIZE, enx_reg_s(VCSZ)->B, enx_reg_s(VCSTR)->STRIDE);
		dprintk("avia_gt_capture: VCSA1->Addr=0x%X, VCSA2->Addr=0x%X, Delta=%d\n", enx_reg_s(VCSA1)->Addr, enx_reg_s(VCSA2)->Addr, enx_reg_s(VCSA2)->Addr - enx_reg_s(VCSA1)->Addr);

	} else if (avia_gt_chip(GTX)) {    

		gtx_reg_set(VCSA, Addr, capt_buf_addr >> 1);
//		gtx_reg_set(VPSA, Addr, (capt_buf_addr + (capture_width * capture_height)) >> 1);

//		rw(VSCP)=(48<<17)|(22<<1);
		gtx_reg_set(VCSA, E, 1);
	    
		dprintk("gtx_capture: HDEC=%d, HSIZE=%d, VDEC=%d, VSIZE=%d, B=%d, STRIDE=%d\n", gtx_reg_s(VCS)->HDEC, gtx_reg_s(VCS)->HSIZE, gtx_reg_s(VCS)->VDEC, gtx_reg_s(VCS)->VSIZE, gtx_reg_s(VCS)->B, line_stride / 4);
		dprintk("gtx_capture: HPOS=%d, OVOFFS=%d, EVPOS=%d\n", gtx_reg_s(VCSP)->HPOS, gtx_reg_s(VCSP)->OVOFFS, gtx_reg_s(VCSP)->EVPOS);
		dprintk("gtx_capture: VCSA->Addr=0x%X, VPSA->Addr=0x%X, Delta=%d\n", gtx_reg_s(VCSA)->Addr, gtx_reg_s(VCSA)->Addr, gtx_reg_s(VCSA)->Addr - gtx_reg_s(VPSA)->Addr);
		
	}
    
	capture_busy = 1;
	captured_frames = 0;
    
	if (capture_buffer)
		*capture_buffer = (unsigned char *)(capt_buf_addr);
	
	if (stride)
		*stride = line_stride;
	
	return 0;
	
}

void avia_gt_capture_stop(void)
{

	if (capture_busy) {
    
		dprintk("avia_gt_capture: capture_stop\n");
    
		if (avia_gt_chip(ENX))
			enx_reg_set(VCSA1, E, 0);
		else if (avia_gt_chip(GTX))
			gtx_reg_set(VCSA, E, 0);

		avia_gt_free_irq(capture_irq);
    
		capture_busy = 0;
	
	}

}

int avia_gt_capture_set_output_size(unsigned short width, unsigned short height, unsigned char pig)
{

	if (capture_busy)
		return -EBUSY;
	
	output_width = width;
	output_height = height;	
	
	return 0;
    
}

int avia_gt_capture_set_input_pos(unsigned short x, unsigned short y, unsigned char pig)
{

	if (capture_busy)
		return -EBUSY;

	input_x = x;
	input_y = y;

	return 0;
    
}

int avia_gt_capture_set_input_size(unsigned short width, unsigned short height, unsigned char pig)
{

	if (capture_busy)
		return -EBUSY;

	input_width = width;
	input_height = height;	

	return 0;
    
}

void avia_gt_capture_reset(unsigned char reenable)
{

    if (avia_gt_chip(ENX))
	enx_reg_set(RSTR0, VIDC, 1);		
    else if (avia_gt_chip(GTX))
	gtx_reg_set(RR0, VCAP, 1);

    if (reenable) {

	if (avia_gt_chip(ENX))
	    enx_reg_set(RSTR0, VIDC, 0);		
	else if (avia_gt_chip(GTX))
	    gtx_reg_set(RR0, VCAP, 0);

    }
    
}

int __init avia_gt_capture_init(void)
{

	printk("avia_gt_capture: $Id: avia_gt_capture.c,v 1.29 2003/07/24 01:59:21 homar Exp $\n");

	gt_info = avia_gt_get_info();

	if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {
    
		printk("avia_gt_capture: Unsupported chip type\n");

		return -EIO;
		    
	}
    
	if (avia_gt_chip(ENX))
		capture_irq = ENX_IRQ_VL1;
	else if (avia_gt_chip(GTX))
		capture_irq = GTX_IRQ_VL1;

	avia_gt_capture_reset(1);

	if (avia_gt_chip(ENX)) {
	
		enx_reg_set(VCP, U, 0);		// Using squashed mode
		enx_reg_set(VCSTR, B, 0);	// Hardware double buffering
		enx_reg_set(VCSZ, F, 1);	// Filter

		enx_reg_set(VLI1, F, 1);	
		enx_reg_set(VLI1, E, 1);	
		enx_reg_set(VLI1, LINE, 0);	
    
	} else if (avia_gt_chip(GTX)) {
    
		gtx_reg_set(VCS, B, 0);		// Hardware double buffering
		gtx_reg_set(VCS, F, 1);		// Filter

		gtx_reg_set(VLI1, F, 1);	
		gtx_reg_set(VLI1, E, 1);	
		gtx_reg_set(VLI1, LINE, 0);

	}

	return 0;
    
}

void __exit avia_gt_capture_exit(void)
{

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

