/*
 *   enx_gv.c - AViA eNX graphic viewport driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2001 Florian Schirmer (jolt@tuxbox.org)
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
 *   $Log: enx_gv.c,v $
 *   Revision 1.1  2001/11/01 18:19:09  Jolt
 *   graphic viewport driver added
 *
 *
 *   $Revision: 1.1 $
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
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
#include <linux/init.h>

#include <dbox/avia_gv.h>
#include <dbox/enx.h>
#include <dbox/enx_gv.h>

#define ENX_MEM_GV_OFFSET	ENX_FB_OFFSET
#define ENX_MEM_GV_SIZE		(720 * 576 * 4)

unsigned short input_height = 576;
unsigned char input_mode = AVIA_GV_INPUT_MODE_RGB32;
unsigned short input_width = 720;
unsigned short output_x = 0;
unsigned short output_y = 0;

void enx_gv_set_stride(void);

void enx_gv_cursor_hide(void) {

    enx_reg_s(GMR1)->C = 0;
}

void enx_gv_cursor_show(void) {

    enx_reg_s(GMR1)->C = 1;
}

void enx_gv_get_info(unsigned char **gv_mem_phys, unsigned char **gv_mem_lin, unsigned int *gv_mem_size) {

    if (gv_mem_phys)
	*gv_mem_phys = (unsigned char *)(ENX_MEM_BASE + ENX_MEM_GV_OFFSET);
	
    if (gv_mem_lin)
	*gv_mem_lin = enx_get_mem_addr() + ENX_MEM_GV_OFFSET;
	
    if (gv_mem_size)
	*gv_mem_size = ENX_MEM_GV_SIZE;
}

unsigned short enx_gv_get_stride(void) {

    return enx_reg_s(GMR1)->STRIDE << 2;
}

void enx_gv_hide(void) {

    enx_reg_s(GMR1)->GMD = AVIA_GV_INPUT_MODE_OFF;
}

void enx_gv_set_input_mode(unsigned char mode) {

    if (input_mode != AVIA_GV_INPUT_MODE_OFF)
	enx_reg_s(GMR1)->GMD = mode;

    // Since mode changed, we have to recalculate some stuff
    enx_gv_set_stride();
	
    input_mode = mode;
}

int enx_gv_set_input_size(unsigned short width, unsigned short height) {

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

    if ((height == 576) || (height == 480)) {
	enx_reg_s(GMR1)->I = 0;
    } else if ((height == 288) || (height == 240)) {
	enx_reg_s(GMR1)->I = 1;
    } else {
	return -EINVAL;
    }
    
    input_height = height;
    input_width = width;
    
    // Since width changed, we have to recalculate some stuff
    enx_gv_set_pos(output_x, output_y);
    enx_gv_set_stride();
    
    return 0;
}

int enx_gv_set_pos(unsigned short x, unsigned short y) {

    unsigned char input_div;

#define BLANK_TIME	132
#define VID_PIPEDELAY	16
#define GFX_PIPEDELAY	3
    
    if (input_width == 720) {
	input_div = 8;
    } else if (input_width == 640) {
	input_div = 9;
    } else if (input_width == 360) {
	input_div = 16;
    } else if (input_width == 320) {
	input_div = 18;
    } else {
	return -EINVAL;
    }

    enx_reg_s(GVP1)->SPP = (((BLANK_TIME - VID_PIPEDELAY) + x) * 8) % input_div;
    enx_reg_s(GVP1)->XPOS = ((((BLANK_TIME - VID_PIPEDELAY) + x) * 8) / input_div) - GFX_PIPEDELAY;
    enx_reg_s(GVP1)->YPOS = 42 + y;
    
    output_x = x;
    output_y = y;
    
    return 0;
}

void enx_gv_set_size(unsigned short width, unsigned short height) {

    enx_reg_s(GVSZ1)->IPP = 0;
    enx_reg_s(GVSZ1)->XSZ = width;
    enx_reg_s(GVSZ1)->YSZ = height;
}

void enx_gv_set_stride(void) {

    unsigned char input_bpp = 4;

    switch(input_mode) {
	case AVIA_GV_INPUT_MODE_RGB4:
	    input_bpp = 1;
	break;
	case AVIA_GV_INPUT_MODE_RGB8:
	    input_bpp = 1;
	break;
	case AVIA_GV_INPUT_MODE_RGB16:
	    input_bpp = 2;
	break;
	case AVIA_GV_INPUT_MODE_RGB32:
	    input_bpp = 4;
	break;
    }

    enx_reg_s(GMR1)->STRIDE = (((input_width * input_bpp) + 3) & ~3) >> 2;
}

void enx_gv_show(void) {

    enx_reg_s(GMR1)->GMD = input_mode;
}

static int enx_gv_init(void)
{
    printk("$Id: enx_gv.c,v 1.1 2001/11/01 18:19:09 Jolt Exp $\n");
    
    //enx_reg_w(RSTR0) &= ~(1 << );	// TODO: which one?
    
    //enx_gv_hide();
    enx_gv_cursor_hide();
    enx_gv_set_pos(0, 0);
    enx_gv_set_input_size(720, 576);
    enx_gv_set_size(720, 576);
    
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

    // It's time for enx_malloc() :-(
    enx_reg_s(GVSA1)->Addr = ENX_MEM_GV_OFFSET >> 2;
    printk("enx_gv: GVSA1=0x08%X ENX_FB=0x%08X\n", enx_reg_32(GVSA1), ENX_MEM_GV_OFFSET);
    
    return 0;
}

static void __exit enx_gv_cleanup(void)
{
    printk("enx_gv: cleanup\n");

//    enx_gv_hide();
    
    //enx_reg_w(RSTR0) |= (1 << );	// TODO: which one?
}

#ifdef MODULE
module_init(enx_gv_init);
module_exit(enx_gv_cleanup);
#endif
