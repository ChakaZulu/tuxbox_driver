/*
 *   avia_gt_gv.c - AViA eNX/GTX graphic viewport driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2002 Florian Schirmer (jolt@tuxbox.org)
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
 *   $Log: avia_gt_gv.c,v $
 *   Revision 1.7  2002/04/22 17:40:01  Jolt
 *   Major cleanup
 *
 *   Revision 1.6  2002/04/21 14:36:07  Jolt
 *   Merged GTX fb support
 *
 *   Revision 1.5  2002/04/16 13:58:16  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.4  2002/04/15 19:32:44  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.3  2002/04/15 10:40:50  Jolt
 *   eNX/GTX
 *
 *   Revision 1.2  2002/04/14 18:14:08  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.1  2001/11/01 18:19:09  Jolt
 *   graphic viewport driver added
 *
 *
 *   $Revision: 1.7 $
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/malloc.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <linux/init.h>

#include <dbox/avia_gt.h>
#include <dbox/avia_gt_gv.h>

unsigned short input_height = 576;
unsigned char input_mode = AVIA_GT_GV_INPUT_MODE_RGB16;
static sAviaGtInfo *gt_info;
unsigned short input_width = 720;
unsigned short output_x = 0;
unsigned short output_y = 0;

void avia_gt_gv_set_stride(void);

void avia_gt_gv_cursor_hide(void)
{

    if (avia_gt_chip(ENX))
	enx_reg_s(GMR1)->C = 0;
    else if (avia_gt_chip(GTX))
	gtx_reg_s(GMR)->C = 0;
    
}

void avia_gt_gv_cursor_show(void)
{

    if (avia_gt_chip(ENX))
	enx_reg_s(GMR1)->C = 1;
    else if (avia_gt_chip(GTX))
	gtx_reg_s(GMR)->C = 1;
    
}

void avia_gt_gv_get_info(unsigned char **gv_mem_phys, unsigned char **gv_mem_lin, unsigned int *gv_mem_size)
{

    if (avia_gt_chip(ENX)) {
    
	if (gv_mem_phys)
	    *gv_mem_phys = (unsigned char *)(ENX_MEM_BASE + AVIA_GT_MEM_GV_OFFS);
	
    } else if (avia_gt_chip(GTX)) {

        if (gv_mem_phys)
	    *gv_mem_phys = (unsigned char *)(GTX_MEM_BASE + AVIA_GT_MEM_GV_OFFS);
	    
    }
	
    if (gv_mem_lin)
	*gv_mem_lin = gt_info->mem_addr + AVIA_GT_MEM_GV_OFFS;
	
    if (gv_mem_size)
	*gv_mem_size = AVIA_GT_MEM_GV_SIZE;
	
}

unsigned short avia_gt_gv_get_stride(void) {

    return enx_reg_s(GMR1)->STRIDE << 2;
    
}

void avia_gt_gv_hide(void) {

    if (avia_gt_chip(ENX))
	enx_reg_s(GMR1)->GMD = AVIA_GT_GV_INPUT_MODE_OFF;
    else if (avia_gt_chip(GTX))
	gtx_reg_s(GMR)->GMD = AVIA_GT_GV_INPUT_MODE_OFF;
    
}

int avia_gt_gv_set_input_mode(unsigned char mode) {

    if (input_mode != AVIA_GT_GV_INPUT_MODE_OFF)
	return -EBUSY;

    input_mode = mode;
    
    // Since mode changed, we have to recalculate some stuff
    avia_gt_gv_set_stride();
    
    return 0;
	
}

int avia_gt_gv_set_input_size(unsigned short width, unsigned short height) {

    if (width == 720) {
    
	enx_reg_s(GMR1)->L = 0;
	enx_reg_s(GMR1)->F = 0;
	
    } else if (width == 640) {
    
	enx_reg_s(GMR1)->L = 0;
	enx_reg_s(GMR1)->F = 1;
	
    } else if (width == 360) {
    
	enx_reg_s(GMR1)->L = 1;
	enx_reg_s(GMR1)->F = 0;
	
    } else if (width == 320) {
    
	enx_reg_s(GMR1)->L = 1;
	enx_reg_s(GMR1)->F = 1;
	
    } else {
    
	return -EINVAL;
	
    }

    if ((height == 576) || (height == 480)) 
	enx_reg_s(GMR1)->I = 0;
    else if ((height == 288) || (height == 240)) 
	enx_reg_s(GMR1)->I = 1;
    else 
	return -EINVAL;
    
    input_height = height;
    input_width = width;
    
    // Since width changed, we have to recalculate some stuff
    avia_gt_gv_set_pos(output_x, output_y);
    avia_gt_gv_set_stride();
    
    return 0;
    
}

int avia_gt_gv_set_pos(unsigned short x, unsigned short y) {

    unsigned char input_div;

#define BLANK_TIME	132
#define VID_PIPEDELAY	16
#define GFX_PIPEDELAY	3
    
    if (input_width == 720)
	input_div = 8;
    else if (input_width == 640)
	input_div = 9;
    else if (input_width == 360)
	input_div = 16;
    else if (input_width == 320)
	input_div = 18;
    else
	return -EINVAL;

    enx_reg_s(GVP1)->SPP = (((BLANK_TIME - VID_PIPEDELAY) + x) * 8) % input_div;
    enx_reg_s(GVP1)->XPOS = ((((BLANK_TIME - VID_PIPEDELAY) + x) * 8) / input_div) - GFX_PIPEDELAY;
    enx_reg_s(GVP1)->YPOS = 42 + y;
    
    output_x = x;
    output_y = y;
    
    return 0;
    
}

void avia_gt_gv_set_size(unsigned short width, unsigned short height) {

    enx_reg_s(GVSZ1)->IPP = 0;
    enx_reg_s(GVSZ1)->XSZ = width;
    enx_reg_s(GVSZ1)->YSZ = height;
    
}

void avia_gt_gv_set_stride(void) {

    unsigned char input_bpp = 2;

    switch(input_mode) {
    
	case AVIA_GT_GV_INPUT_MODE_RGB4:
	
	    input_bpp = 1;
	    
	break;
	case AVIA_GT_GV_INPUT_MODE_RGB8:
	
	    input_bpp = 1;
	    
	break;
	case AVIA_GT_GV_INPUT_MODE_RGB16:
	
	    input_bpp = 2;
	    
	break;
	case AVIA_GT_GV_INPUT_MODE_RGB32:
	
	    input_bpp = 4;
	
	break;
	
    }

    if (avia_gt_chip(ENX))
	enx_reg_s(GMR1)->STRIDE = (((input_width * input_bpp) + 3) & ~3) >> 2;
    else if (avia_gt_chip(GTX))
	gtx_reg_s(GMR)->STRIDE = (((input_width * input_bpp) + 1) & ~1) >> 1;
    
}

int avia_gt_gv_show(void) {

    switch(input_mode) {
    
	case AVIA_GT_GV_INPUT_MODE_OFF:
	
	    if (avia_gt_chip(ENX))
    	        enx_reg_s(GMR1)->GMD = 0x00;
	    else if (avia_gt_chip(GTX))
		gtx_reg_s(GMR)->GMD = 0x00;
	    
	break;
	case AVIA_GT_GV_INPUT_MODE_RGB4:
	
	    if (avia_gt_chip(ENX))
    	        enx_reg_s(GMR1)->GMD = 0x02;
	    else if (avia_gt_chip(GTX))
		gtx_reg_s(GMR)->GMD = 0x01;
	    
	break;
	case AVIA_GT_GV_INPUT_MODE_RGB8:
	
	    if (avia_gt_chip(ENX))
    	        enx_reg_s(GMR1)->GMD = 0x06;
	    else if (avia_gt_chip(GTX))
		gtx_reg_s(GMR)->GMD = 0x02;
	    
	break;
	case AVIA_GT_GV_INPUT_MODE_RGB16:
	
	    if (avia_gt_chip(ENX))
    	        enx_reg_s(GMR1)->GMD = 0x03;
	    else if (avia_gt_chip(GTX))
		gtx_reg_s(GMR)->GMD = 0x03;
	    
	break;
	case AVIA_GT_GV_INPUT_MODE_RGB32:
	
	    if (avia_gt_chip(ENX))
    	        enx_reg_s(GMR1)->GMD = 0x07;
	    else if (avia_gt_chip(GTX))
	        return -EINVAL;
		
	break;
	default:
	
	    return -EINVAL;
	
	break;
	
    }

    return 0;
    
}

int avia_gt_gv_init(void)
{

    printk("avia_gt_gv: $Id: avia_gt_gv.c,v 1.7 2002/04/22 17:40:01 Jolt Exp $\n");

    gt_info = avia_gt_get_info();
    
    if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {
	
        printk("avia_gv_pig: Unsupported chip type\n");
		
        return -EIO;
			
    }
			        
    if (avia_gt_chip(ENX)) {
    
	//enx_reg_s(RSTR0) &= ~(1 << );	// TODO: which one?
	
    } else if (avia_gt_chip(GTX)) {
    
    
    }
    
    //avia_gt_gv_hide();
    avia_gt_gv_cursor_hide();
    avia_gt_gv_set_pos(0, 0);
    avia_gt_gv_set_input_size(720, 576);
    avia_gt_gv_set_size(720, 576);
    
    if (avia_gt_chip(ENX)) {
    
	//enx_reg_s(GMR1)->P = 1;
	enx_reg_s(GMR1)->S = 1;
	enx_reg_s(GMR1)->B = 0;
	//enx_reg_s(GMR1)->BANK = 1;
    
	enx_reg_s(GBLEV1)->BLEV11 = 0x00;
	enx_reg_s(GBLEV1)->BLEV10 = 0x20;
    
	enx_reg_s(TCR1)->Red = 0x00;
	enx_reg_s(TCR1)->Green = 0x00;
	enx_reg_s(TCR1)->Blue = 0x00;
	enx_reg_s(TCR1)->E = 1;

	enx_reg_s(GVSA1)->Addr = AVIA_GT_MEM_GV_OFFS >> 2;

    } else if (avia_gt_chip(GTX)) {
    
    }
    
    return 0;
    
}

void __exit avia_gt_gv_exit(void)
{

//    avia_gt_gv_hide();
    
    if (avia_gt_chip(ENX)) {
    
	//enx_reg_w(RSTR0) |= (1 << );	// TODO: which one?
	
    } else if (avia_gt_chip(GTX)) {
    
	
    }
    
}

#ifdef MODULE
EXPORT_SYMBOL(avia_gt_gv_get_info);
EXPORT_SYMBOL(avia_gt_gv_set_input_mode);
EXPORT_SYMBOL(avia_gt_gv_show);
#endif

#if defined(MODULE) && defined(STANDALONE)
module_init(avia_gt_gv_init);
module_exit(avia_gt_gv_exit);
#endif
