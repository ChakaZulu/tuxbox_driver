/*
 * $Id: avia_gt_pig.c,v 1.38 2003/07/24 01:59:22 homar Exp $
 *
 * pig driver for AViA eNX/GTX (dbox-II-project)
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
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>

#include "avia_gt.h"
#include <linux/dvb/avia/avia_gt_capture.h>

#define MAX_PIG_COUNT 2

#define CAPTURE_WIDTH 720
#define CAPTURE_HEIGHT 576

static sAviaGtInfo *gt_info = NULL;
static unsigned char pig_busy[MAX_PIG_COUNT] = {0, 0};
static unsigned char *pig_buffer[MAX_PIG_COUNT] = {NULL, NULL};
static unsigned char pig_count = 0;
static unsigned short pig_stride[MAX_PIG_COUNT] = {0, 0};

int avia_gt_pig_hide(unsigned char pig_nr)
{

    dprintk("avia_gt_pig_hide (pig_nr=%d)\n", pig_nr);

    if (pig_nr >= pig_count)
	return -ENODEV;

    if (pig_busy[pig_nr]) {

	if (avia_gt_chip(ENX))
	    enx_reg_set(VPSA1, E, 0);
	else if (avia_gt_chip(GTX))
	    gtx_reg_set(VPSA, E, 0);

	avia_gt_capture_stop(1);
    
	pig_busy[pig_nr] = 0;
	
    }

    return 0;
    
}

int avia_gt_pig_set_pos(unsigned char pig_nr, unsigned short x, unsigned short y)
{

	dprintk("avia_gt_pig_set_pos (x=%d, y=%d)\n", x ,y);
    
	if (pig_nr >= pig_count)
		return -ENODEV;

	if (avia_gt_chip(ENX)) {
    
		enx_reg_set(VPP1, HPOS, 63 + (x / 2));
		enx_reg_set(VPP1, VPOS, 21 + (y / 2));
	
	} else if (avia_gt_chip(GTX)) {
    
		gtx_reg_set(VPP, HPOS, 36 + (x / 2)); //- (gtx_reg_s(VPS)->S ? 3 : 0);
		gtx_reg_set(VPP, VPOS, 20 + (y / 2));
	
	}
    
	return 0;
    
}

int avia_gt_pig_set_stack(unsigned char pig_nr, unsigned char stack_order)
{

	dprintk("avia_gt_pig_set_stack (pig_nr=%d, stack_order=%d)\n", pig_nr, stack_order);

	if (pig_nr >= pig_count)
		return -ENODEV;
	
	if (avia_gt_chip(ENX))
		enx_reg_set(VPSO1, SO, stack_order);
	else if (avia_gt_chip(GTX))
		gtx_reg_set(VPS, P, !!stack_order);
	
	return 0;
    
}

int avia_gt_pig_set_size(unsigned char pig_nr, unsigned short width, unsigned short height, unsigned char stretch)
{

	int result = 0;

	dprintk("avia_gt_pig_set_size (width=%d, height=%d, stretch=%d)\n", width, height, stretch);
    
	if (pig_nr >= pig_count)
		return -ENODEV;
	
	if (pig_busy[pig_nr])
		return -EBUSY;
	
	result = avia_gt_capture_set_output_size(width, height, 1);
    
	if (result < 0)
		return result;

	if (avia_gt_chip(ENX)) {

		enx_reg_set(VPSZ1, WIDTH, width / 2);
		enx_reg_set(VPSZ1, S, stretch);
		enx_reg_set(VPSZ1, HEIGHT, height / 2);

		dprintk("avia_gt_pig: WIDTH=0x%X, S=0x%X, HEIGHT=0x%X\n", enx_reg_s(VPSZ1)->WIDTH, enx_reg_s(VPSZ1)->S, enx_reg_s(VPSZ1)->HEIGHT);
	
	} else if (avia_gt_chip(GTX)) {

		gtx_reg_set(VPS, WIDTH, width / 2);
 		gtx_reg_set(VPS, S, stretch);
		gtx_reg_set(VPS, HEIGHT, height / 2);

		dprintk("avia_gt_pig: WIDTH=0x%X, S=0x%X, HEIGHT=0x%X\n", gtx_reg_s(VPS)->WIDTH, gtx_reg_s(VPS)->S, gtx_reg_s(VPS)->HEIGHT);
		
	}
    
	return 0;
    
}

int avia_gt_pig_show(unsigned char pig_nr)
{

	dprintk("avia_gt_pig_show (pig_nr=%d)\n", pig_nr);

	if (pig_nr >= pig_count)
		return -ENODEV;
	
	if (pig_busy[pig_nr])
		return -EBUSY;

	if (avia_gt_capture_set_input_pos(0, 0, 1) < 0)
		return -EBUSY;
	if (avia_gt_capture_set_input_size(CAPTURE_WIDTH, CAPTURE_HEIGHT, 1) < 0)
		return -EBUSY;
	if (avia_gt_capture_start(&pig_buffer[pig_nr], &pig_stride[pig_nr], 1) < 0)
		return -EBUSY;

	dprintk("avia_gt_pig: buffer=0x%X, stride=0x%X\n", (unsigned int)pig_buffer[pig_nr], pig_stride[pig_nr]);

	if (avia_gt_chip(ENX)) {

		enx_reg_16(VPSTR1) = 0;
			
		if(((unsigned int)(pig_stride[pig_nr])) < 240)
			enx_reg_16(VPSTR1) |= ((unsigned int)(pig_stride[pig_nr]));
		else
			enx_reg_16(VPSTR1) |= ((((unsigned int)(pig_stride[pig_nr])) * 2) & 0x7FF);

		enx_reg_16(VPSTR1) |= 0;				// Enable hardware double buffering
    
		enx_reg_set(VPSZ1, P, 0);
    
		enx_reg_set(VPSA1, Addr, ((unsigned int)pig_buffer[pig_nr]) >> 2);	// Set buffer address (for non d-buffer mode)
    
		enx_reg_set(VPOFFS1, OFFSET, ((unsigned int)(pig_stride[pig_nr])) >> 2);

		enx_reg_set(VPP1, U, 0);
		enx_reg_set(VPP1, F, 0);
        
		enx_reg_set(VPSA1, E, 1);
	
	} else if (avia_gt_chip(GTX)) {

		gtx_reg_set(VPO, OFFSET, (((unsigned int)(pig_stride[pig_nr])) / 2));
		gtx_reg_set(VPO, STRIDE, ((unsigned int)(pig_stride[pig_nr])) >> 1);
		gtx_reg_set(VPO, B, 0);                                                              // Enable hardware double buffering
	        
		gtx_reg_set(VPSA, Addr, ((unsigned int)(pig_buffer[pig_nr])) >> 1);                  // Set buffer address (for non d-buffer mode)
		
		gtx_reg_set(VPP, F, 0);

		gtx_reg_set(VPSA, E, 1);
    
	}
    
	pig_busy[pig_nr] = 1;
    
	return 0;
    
}

int __init avia_gt_pig_init(void)
{

	unsigned char pig_nr;

	printk("avia_gt_pig: $Id: avia_gt_pig.c,v 1.38 2003/07/24 01:59:22 homar Exp $\n");

	gt_info = avia_gt_get_info();
    
	if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {
	
		printk("avia_gt_pig: Unsupported chip type\n");
		
 		return -EIO;
			
	}
			        
	if (avia_gt_chip(ENX))
		pig_count = 2;
	else if (avia_gt_chip(GTX))
		pig_count = 1;

	if (avia_gt_chip(ENX)) {
   
		enx_reg_set(RSTR0, PIG1, 1);
		enx_reg_set(RSTR0, PIG2, 1);
		enx_reg_set(RSTR0, PIG1, 0);
		enx_reg_set(RSTR0, PIG2, 0);

	} else if (avia_gt_chip(GTX)) {

		gtx_reg_set(RR0, PIG, 1);
		gtx_reg_set(RR0, PIG, 0);
    
	}
    
	for (pig_nr = 0; pig_nr < pig_count; pig_nr++) {
    
		avia_gt_pig_set_pos(pig_nr, 150, 50);
		avia_gt_pig_set_size(pig_nr, CAPTURE_WIDTH / 4, CAPTURE_HEIGHT / 4, 0);

//		avia_gt_pig_set_pos(pig_nr, 365, 362);
//		avia_gt_pig_set_size(pig_nr, 180, 144, 0);
//		avia_gt_pig_set_size(pig_nr, 256, 208, 0);
//		avia_gt_pig_set_stack(pig_nr, 1);

//		avia_gt_pig_show(pig_nr);
	
	}
    
	return 0;
    
}

void __exit avia_gt_pig_exit(void)
{

	avia_gt_pig_hide(0);

	if (avia_gt_chip(ENX)) {

		enx_reg_set(RSTR0, PIG1, 1);
		enx_reg_set(RSTR0, PIG2, 1);
    
	} else if (avia_gt_chip(GTX)) {

		gtx_reg_set(RR0, PIG, 1);

	}    
    
}

#if defined(STANDALONE)
module_init(avia_gt_pig_init);
module_exit(avia_gt_pig_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("AViA eNX/GTX PIG driver");
MODULE_LICENSE("GPL");
#endif

EXPORT_SYMBOL(avia_gt_pig_hide);
EXPORT_SYMBOL(avia_gt_pig_set_pos);
EXPORT_SYMBOL(avia_gt_pig_set_size);
EXPORT_SYMBOL(avia_gt_pig_set_stack);
EXPORT_SYMBOL(avia_gt_pig_show);

