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
 *   Revision 1.12  2002/04/25 22:10:38  Jolt
 *   FB cleanup
 *
 *   Revision 1.11  2002/04/25 21:09:02  Jolt
 *   Fixes/Cleanups
 *
 *   Revision 1.10  2002/04/24 21:38:13  Jolt
 *   Framebuffer cleanups
 *
 *   Revision 1.9  2002/04/24 19:56:00  Jolt
 *   GV driver updates
 *
 *   Revision 1.8  2002/04/24 08:01:00  obi
 *   more gtx support
 *
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
 *   $Revision: 1.12 $
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

void avia_gt_gv_get_clut(unsigned char clut_nr, unsigned int *transparency, unsigned int *red, unsigned int *green, unsigned int *blue)
{

    unsigned int val;															  

    if (avia_gt_chip(ENX)) {

	enx_reg_16(CLUTA) = clut_nr;
	
	mb();
	
	val = enx_reg_16(CLUTD);

	if (transparency)
	    *transparency = ((val & 0xFF000000) >> 24);
												  
	if (red)
	    *red = ((val & 0x00FF0000) >> 16);
													      
	if (green)
	    *green = ((val & 0x0000FF00) >> 8);
														    
	if (blue)
	    *blue = (val & 0x000000FF);
													
    } else if (avia_gt_chip(GTX)) {

	rh(CLTA) = clut_nr;

	mb();

#define TCR_COLOR 0xFC0F

	val = rh(CLTD);
	
	if (val == TCR_COLOR) {
	
	    if (transparency)
		*transparency = 255;
	    
	    if (red)
		*red = 0;
	
	    if (green)
	        *green = 0;

	    if (blue)		
		*blue = 0;
		
	} else {
	
	    //if (transparency)
	//	*transparency = (val & 0x8000) ? BLEVEL : 0;

	    if (red)
		*red = ((val & 0x7C00) >> 10) << 19;
		
	    if (green)
	        *green = ((val & 0x3E0) >> 5) << 19;

	    if (blue)
		*blue = (val & 0x1F) << 19;

        }
	
    }

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

unsigned short avia_gt_gv_get_stride(void)
{

    unsigned short stride;

    if (avia_gt_chip(ENX))
        stride = enx_reg_s(GMR1)->STRIDE << 2;
    else if (avia_gt_chip(GTX))
    	stride = gtx_reg_s(GMR)->STRIDE << 1;

    return stride;

}

void avia_gt_gv_hide(void)
{

    if (avia_gt_chip(ENX))
	enx_reg_s(GMR1)->GMD = AVIA_GT_GV_INPUT_MODE_OFF;
    else if (avia_gt_chip(GTX))
	gtx_reg_s(GMR)->GMD = AVIA_GT_GV_INPUT_MODE_OFF;
    
}

void avia_gt_gv_set_blevel(unsigned char class0, unsigned char class1)
{

    if (avia_gt_chip(ENX)) {
    
	enx_reg_s(GBLEV1)->BLEV10 = class0 * 0x80 / 0xFF;
	enx_reg_s(GBLEV1)->BLEV11 = class1 * 0x80 / 0xFF;
	
    } else if (avia_gt_chip(GTX)) {

	gtx_reg_s(GMR)->BLEV0 = class0 * 0x08 / 0xFF;
	gtx_reg_s(GMR)->BLEV1 = class1 * 0x08 / 0xFF;

    }    
    
}

void avia_gt_gv_set_clut(unsigned char clut_nr, unsigned int transparency, unsigned int red, unsigned int green, unsigned int blue)
{

    if (avia_gt_chip(ENX)) {

        transparency >>= 8;
	red >>= 8;
	green >>= 8;
        blue >>= 8;
	
	enx_reg_16(CLUTA) = clut_nr;
	
	mb();
	
	enx_reg_32(CLUTD) = ((transparency << 24) | (blue << 16) | (green << 8) | (red));
	          
    } else if (avia_gt_chip(GTX)) {

	transparency >>= 8;
	red >>= 11;
	green >>= 11;
	blue >>= 11;
	
	rh(CLTA) = clut_nr;

	mb();

	if (transparency >= 0x80) {             // full transparency

#define TCR_COLOR 0xFC0F

	    rh(CLTD) = TCR_COLOR;
	    
	} else {
	
	    if (!transparency)
		transparency = 1;
	    else
		transparency = 0;

	    rh(CLTD) = (transparency << 15) | (red << 10) | (green << 5) | (blue);
	    
	}
	
    }

}

int avia_gt_gv_set_input_mode(unsigned char mode)
{

    printk("avia_gt_gv: set_input_mode (mode=%d)\n", mode);

    input_mode = mode;
    
    // Since mode changed, we have to recalculate some stuff
    avia_gt_gv_set_stride();
    
    return 0;
	
}

int avia_gt_gv_set_input_size(unsigned short width, unsigned short height) {

    printk("avia_gt_gv: set_input_size (width=%d, height=%d)\n", width, height);

    if (width == 720) {
   
	if (avia_gt_chip(ENX)) {
		
	    enx_reg_s(GMR1)->L = 0;
	    enx_reg_s(GMR1)->F = 0;

	} else if (avia_gt_chip(GTX)) {

	    gtx_reg_s(GMR)->L = 0;
	    gtx_reg_s(GMR)->F = 0;

	}
	
    } else if (width == 640) {
   
	if (avia_gt_chip(ENX)) {
    
	    enx_reg_s(GMR1)->L = 0;
	    enx_reg_s(GMR1)->F = 1;

	} else if (avia_gt_chip(GTX)) {

	    gtx_reg_s(GMR)->L = 0;
	    gtx_reg_s(GMR)->F = 1;

	}
	
    } else if (width == 360) {
   
	if (avia_gt_chip(ENX)) {
    
	    enx_reg_s(GMR1)->L = 1;
	    enx_reg_s(GMR1)->F = 0;

	} else if (avia_gt_chip(GTX)) {

	    gtx_reg_s(GMR)->L = 1;
	    gtx_reg_s(GMR)->F = 0;

	}
	
    } else if (width == 320) {
   
	if (avia_gt_chip(ENX)) {
    
	    enx_reg_s(GMR1)->L = 1;
	    enx_reg_s(GMR1)->F = 1;

	} else if (avia_gt_chip(GTX)) {

	    gtx_reg_s(GMR)->L = 1;
	    gtx_reg_s(GMR)->F = 1;

	}
	
    } else {
    
	return -EINVAL;
	
    }

    if ((height == 576) || (height == 480)) {
	    
	if (avia_gt_chip(ENX))
	    enx_reg_s(GMR1)->I = 0;
	else if (avia_gt_chip(GTX))
	    gtx_reg_s(GMR)->I = 0;

    } else if ((height == 288) || (height == 240)) {
	    
	if (avia_gt_chip(ENX))
	    enx_reg_s(GMR1)->I = 1;
	else if (avia_gt_chip(GTX))
	    gtx_reg_s(GMR)->I = 1;

    } else {
	    
	return -EINVAL;

    }
    
    input_height = height;
    input_width = width;
    
    // Since width changed, we have to recalculate some stuff
    avia_gt_gv_set_pos(output_x, output_y);
    avia_gt_gv_set_stride();
    
    return 0;
    
}

int avia_gt_gv_set_pos(unsigned short x, unsigned short y) {

    unsigned char input_div;

#define BLANK_TIME		132
#define ENX_VID_PIPEDELAY	16
#define GTX_VID_PIPEDELAY	5
#define GFX_PIPEDELAY		3
    
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

    if (avia_gt_chip(ENX)) {
	    
	enx_reg_s(GVP1)->SPP = (((BLANK_TIME - ENX_VID_PIPEDELAY) + x) * 8) % input_div;
	enx_reg_s(GVP1)->XPOS = ((((BLANK_TIME - ENX_VID_PIPEDELAY) + x) * 8) / input_div) - GFX_PIPEDELAY;
	enx_reg_s(GVP1)->YPOS = 42 + y;

    } else if (avia_gt_chip(GTX)) {

	gtx_reg_s(GVP)->SPP = (((BLANK_TIME - GTX_VID_PIPEDELAY) + x) * 8) % input_div;
	gtx_reg_s(GVP)->XPOS = ((((BLANK_TIME - GTX_VID_PIPEDELAY) + x) * 8) / input_div) - GFX_PIPEDELAY;
	gtx_reg_s(GVP)->YPOS = 42 + y;

    }
    
    output_x = x;
    output_y = y;
    
    return 0;
    
}

void avia_gt_gv_set_size(unsigned short width, unsigned short height) {

    if (avia_gt_chip(ENX)) {
	    
	enx_reg_s(GVSZ1)->IPP = 0;
	enx_reg_s(GVSZ1)->XSZ = width;
	enx_reg_s(GVSZ1)->YSZ = height;

    } else if (avia_gt_chip(GTX)) {

	gtx_reg_s(GVS)->IPS = 0;
	gtx_reg_s(GVS)->XSZ = width;
	gtx_reg_s(GVS)->YSZ = height;

    }

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
	enx_reg_s(GMR1)->STRIDE = ((input_width * input_bpp) + 3) >> 2;
    else if (avia_gt_chip(GTX))
	gtx_reg_s(GMR)->STRIDE = ((input_width * input_bpp) + 1) >> 1;
    
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

    printk("avia_gt_gv: $Id: avia_gt_gv.c,v 1.12 2002/04/25 22:10:38 Jolt Exp $\n");

    gt_info = avia_gt_get_info();
    
    if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {
	
        printk("avia_gv_pig: Unsupported chip type\n");
		
        return -EIO;
			
    }
			        
    if (avia_gt_chip(ENX)) {
    
	enx_reg_s(RSTR0)->GFIX = 1;
	enx_reg_s(RSTR0)->GFIX = 0;
	
    } else if (avia_gt_chip(GTX)) {

	gtx_reg_s(RR0)->GV = 1;
	gtx_reg_s(RR0)->GV = 0;
	
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

	gtx_reg_s(GMR)->CFT = 0;

	gtx_reg_s(GMR)->BLEV1 = 0x00;
	gtx_reg_s(GMR)->BLEV0 = 0x02;

	gtx_reg_s(TCR)->R = 0x00;
	gtx_reg_s(TCR)->G = 0x00;
	gtx_reg_s(TCR)->B = 0x00;
	gtx_reg_s(TCR)->E = 1;

	gtx_reg_s(GVSA)->Addr = AVIA_GT_MEM_GV_OFFS >> 1;

    }

    return 0;
    
}

void __exit avia_gt_gv_exit(void)
{

//    avia_gt_gv_hide();
    
    if (avia_gt_chip(ENX))
	enx_reg_s(RSTR0)->GFIX = 1;
    else if (avia_gt_chip(GTX))
	gtx_reg_s(RR0)->GV = 1;

}

#ifdef MODULE
EXPORT_SYMBOL(avia_gt_gv_get_clut);
EXPORT_SYMBOL(avia_gt_gv_get_info);
EXPORT_SYMBOL(avia_gt_gv_set_blevel);
EXPORT_SYMBOL(avia_gt_gv_set_clut);
EXPORT_SYMBOL(avia_gt_gv_set_input_mode);
EXPORT_SYMBOL(avia_gt_gv_set_input_size);
EXPORT_SYMBOL(avia_gt_gv_set_pos);
EXPORT_SYMBOL(avia_gt_gv_set_size);
EXPORT_SYMBOL(avia_gt_gv_show);
#endif

#if defined(MODULE) && defined(STANDALONE)
module_init(avia_gt_gv_init);
module_exit(avia_gt_gv_exit);
#endif
