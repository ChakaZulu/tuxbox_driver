/*
 * $Id: avia_gt_gv.c,v 1.34 2003/08/01 17:31:22 obi Exp $
 *
 * AViA eNX/GTX graphic viewport driver (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2000-2002 Florian Schirmer (jolt@tuxbox.org)
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

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/system.h>

#include "avia_gt.h"
#include "avia_gt_gv.h"
#include "avia_gt_accel.h"

static u16 input_height = 576;
static u8 input_mode = AVIA_GT_GV_INPUT_MODE_RGB16;
static sAviaGtInfo *gt_info = NULL;
static u16 input_width = 720;
static u16 output_x = 0;
static u16 output_y = 0;

u8 avia_gt_get_bpp(void);
void avia_gt_gv_set_stride(void);

void avia_gt_gv_copyarea(u16 src_x, u16 src_y, u16 width, u16 height, u16 dst_x, u16 dst_y)
{
	u16 bpp = avia_gt_get_bpp();
	u16 line;
	u16 stride = avia_gt_gv_get_stride();

	for (line = 0; line < height; line++)
		avia_gt_accel_copy(AVIA_GT_MEM_GV_OFFS + (src_y + line) * stride + src_x * bpp, AVIA_GT_MEM_GV_OFFS + (dst_y + line) * stride + dst_x * bpp, width * bpp, 0);
}

void avia_gt_gv_cursor_hide(void)
{
	if (avia_gt_chip(ENX))
		enx_reg_set(GMR1, C, 0);
	else if (avia_gt_chip(GTX))
		gtx_reg_set(GMR, C, 0);
}

void avia_gt_gv_cursor_show(void)
{
	if (avia_gt_chip(ENX))
		enx_reg_set(GMR1, C, 1);
	else if (avia_gt_chip(GTX))
		gtx_reg_set(GMR, C, 1);
}

u8 avia_gt_get_bpp(void) 
{
	switch (input_mode) {
	case AVIA_GT_GV_INPUT_MODE_RGB4:
		return 1;
	case AVIA_GT_GV_INPUT_MODE_RGB8:
		return 1;
	case AVIA_GT_GV_INPUT_MODE_RGB16:
		return 2;
	case AVIA_GT_GV_INPUT_MODE_RGB32:
		return 4;
	default:
		return 2;
	}
}

void avia_gt_gv_get_clut(u8 clut_nr, u32 *transparency, u32 *red, u32 *green, u32 *blue)
{
	u32 val = 0;

	if (avia_gt_chip(ENX)) {
		enx_reg_16(CLUTA) = clut_nr;

		mb();

		val = enx_reg_32(CLUTD);

		if (transparency)
			 *transparency = ((val & 0xFF000000) >> 24);
		if (red)
			 *red = ((val & 0x00FF0000) >> 16);
		if (green)
			 *green = ((val & 0x0000FF00) >> 8);
		if (blue)
			 *blue = (val & 0x000000FF);
	}
	else if (avia_gt_chip(GTX)) {
		gtx_reg_set(CLTA, Addr, clut_nr);

		mb();

#define TCR_COLOR 0xFC0F

		val = gtx_reg_16(CLTD);

		if (val == TCR_COLOR) {
			 if (transparency)
				*transparency = 255;
			 if (red)
				*red = 0;
			 if (green)
				*green = 0;
			 if (blue)
				*blue = 0;
		}
		else {
			 //if (transparency)
				//*transparency = (val & 0x8000) ? BLEVEL : 0;
			 if (red)
				*red = (val & 0x7C00) << 9;
			 if (green)
				*green = (val & 0x03E0) << 14;
			 if (blue)
				*blue = (val & 0x001F) << 19;
		}
	}
}

void avia_gt_gv_get_info(u8 **gv_mem_phys, u8 **gv_mem_lin, u32 *gv_mem_size)
{
	if (avia_gt_chip(ENX)) {
		if (gv_mem_phys)
			 *gv_mem_phys = (u8 *)(ENX_MEM_BASE + AVIA_GT_MEM_GV_OFFS);
	}
	else if (avia_gt_chip(GTX)) {
		if (gv_mem_phys)
			 *gv_mem_phys = (u8 *)(GTX_MEM_BASE + AVIA_GT_MEM_GV_OFFS);
	}

	if (gv_mem_lin)
		*gv_mem_lin = gt_info->mem_addr + AVIA_GT_MEM_GV_OFFS;

	if (gv_mem_size)
		*gv_mem_size = AVIA_GT_MEM_GV_SIZE;
}

u16 avia_gt_gv_get_stride(void)
{
	u16 stride = 0;

	if (avia_gt_chip(ENX))
		stride = enx_reg_s(GMR1)->STRIDE << 2;
	else if (avia_gt_chip(GTX))
		stride = gtx_reg_s(GMR)->STRIDE << 1;

	return stride;
}

void avia_gt_gv_hide(void)
{
	if (avia_gt_chip(ENX))
		enx_reg_set(GMR1, GMD, AVIA_GT_GV_INPUT_MODE_OFF);
	else if (avia_gt_chip(GTX))
		gtx_reg_set(GMR, GMD, AVIA_GT_GV_INPUT_MODE_OFF);
}

void avia_gt_gv_set_blevel(u8 class0, u8 class1)
{
	if ((class0 > 0x08) && (class0 != 0x0F))
		return;

	if ((class1 > 0x08) && (class1 != 0x0F))
		return;

	if (avia_gt_chip(ENX)) {
		if (class0 == 0x0F)
			enx_reg_set(GBLEV1, BLEV10, 0xFF);
		else
			enx_reg_set(GBLEV1, BLEV10, class0 << 4);

		if (class1 == 0x0F)
			enx_reg_set(GBLEV1, BLEV11, 0xFF);
		else
			enx_reg_set(GBLEV1, BLEV11, class1 << 4);
	}
	else if (avia_gt_chip(GTX)) {
		gtx_reg_set(GMR, BLEV0, class0);
		gtx_reg_set(GMR, BLEV1, class1);
	}
}

void avia_gt_gv_set_clut(u8 clut_nr, u32 transparency, u32 red, u32 green, u32 blue)
{
	if (avia_gt_chip(ENX)) {
		transparency >>= 8;
		red >>= 8;
		green >>= 8;
		blue >>= 8;

		enx_reg_16(CLUTA) = clut_nr;

		mb();

		enx_reg_32(CLUTD) = ((transparency << 24) | (red << 16) | (green << 8) | (blue));
	}
	else if (avia_gt_chip(GTX)) {

		transparency >>= 8;
		red >>= 11;
		green >>= 11;
		blue >>= 11;

		gtx_reg_16(CLTA) = clut_nr;

		mb();

#define TCR_COLOR 0xFC0F
		if (transparency >= 0x80) {	  // full transparency
			gtx_reg_16(CLTD) = TCR_COLOR;
		}
		else {
			if (!transparency)
				transparency = 1;
			else
				transparency = 0;

			gtx_reg_16(CLTD) = (transparency << 15) | (red << 10) | (green << 5) | (blue);
		}
	}
}

int avia_gt_gv_set_input_mode(u8 mode)
{
	printk(KERN_INFO "avia_gt_gv: set_input_mode (mode=%d)\n", mode);

	input_mode = mode;

	// Since mode changed, we have to recalculate some stuff
	avia_gt_gv_set_stride();

	return 0;
}

int avia_gt_gv_set_input_size(u16 width, u16 height)
{
	printk(KERN_INFO "avia_gt_gv: set_input_size (width=%d, height=%d)\n", width, height);

	if (width == 720) {
		if (avia_gt_chip(ENX)) {
			enx_reg_set(GMR1, L, 0);
			enx_reg_set(GMR1, F, 0);
		}
		else if (avia_gt_chip(GTX)) {
			 gtx_reg_set(GMR, L, 0);
			 gtx_reg_set(GMR, F, 0);
		}
	}
	else if (width == 640) {
		/*
		 * F = 0
		 * 1 would stretch 640x480 to 720x576
		 * this allows seeing a full screen console on tv
		 */
		if (avia_gt_chip(ENX)) {
			enx_reg_set(GMR1, L, 0);
			enx_reg_set(GMR1, F, 0);
		}
		else if (avia_gt_chip(GTX)) {
			gtx_reg_set(GMR, L, 0);
			gtx_reg_set(GMR, F, 0);
		}
	}
	else if (width == 360) {
		if (avia_gt_chip(ENX)) {
			enx_reg_set(GMR1, L, 1);
			enx_reg_set(GMR1, F, 0);
		}
		else if (avia_gt_chip(GTX)) {
			gtx_reg_set(GMR, L, 1);
			gtx_reg_set(GMR, F, 0);
		}
	}
	else if (width == 320) {
		if (avia_gt_chip(ENX)) {
			enx_reg_set(GMR1, L, 1);
			enx_reg_set(GMR1, F, 1);
		}
		else if (avia_gt_chip(GTX)) {
			gtx_reg_set(GMR, L, 1);
			gtx_reg_set(GMR, F, 1);
		}
	}
	else {
		return -EINVAL;
	}

	if ((height == 576) || (height == 480)) {
		if (avia_gt_chip(ENX))
			enx_reg_set(GMR1, I, 0);
		else if (avia_gt_chip(GTX))
			gtx_reg_set(GMR, I, 0);
	}
	else if ((height == 288) || (height == 240)) {
		if (avia_gt_chip(ENX))
			enx_reg_set(GMR1, I, 1);
		else if (avia_gt_chip(GTX))
			gtx_reg_set(GMR, I, 1);
	}
	else {
		return -EINVAL;
	}

	input_height = height;
	input_width = width;

	// Since width changed, we have to recalculate some stuff
	avia_gt_gv_set_pos(output_x, output_y);
	avia_gt_gv_set_stride();

	return 0;
}

int avia_gt_gv_set_pos(u16 x, u16 y)
{
	u8 input_div = 0;

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
		enx_reg_set(GVP1, SPP, (((BLANK_TIME - ENX_VID_PIPEDELAY) + x) * 8) % input_div);
		enx_reg_set(GVP1, XPOS, ((((BLANK_TIME - ENX_VID_PIPEDELAY) + x) * 8) / input_div) - GFX_PIPEDELAY);
		enx_reg_set(GVP1, YPOS, 42 + y);
	}
	else if (avia_gt_chip(GTX)) {
		gtx_reg_set(GVP, SPP, (((BLANK_TIME - GTX_VID_PIPEDELAY) + x) * 8) % input_div);
		//gtx_reg_set(GVP, XPOS, ((((BLANK_TIME - GTX_VID_PIPEDELAY) + x) * 8) / input_div) - GFX_PIPEDELAY);
		gtx_reg_set(GVP, XPOS, ((((BLANK_TIME - GTX_VID_PIPEDELAY - 55) + x) * 8) / input_div) - GFX_PIPEDELAY);	//FIXME
		gtx_reg_set(GVP, YPOS, 42 + y);
	}

	output_x = x;
	output_y = y;

	return 0;
}

void avia_gt_gv_set_size(u16 width, u16 height)
{
	if (avia_gt_chip(ENX)) {
		enx_reg_set(GVSZ1, IPP, 0);
		enx_reg_set(GVSZ1, XSZ, width);
		enx_reg_set(GVSZ1, YSZ, height);
	}
	else if (avia_gt_chip(GTX)) {
		gtx_reg_set(GVS, IPS, 0);
		gtx_reg_set(GVS, XSZ, width);
		gtx_reg_set(GVS, YSZ, height);
	}
}

void avia_gt_gv_set_stride(void)
{
	if (avia_gt_chip(ENX))
		enx_reg_set(GMR1, STRIDE, ((input_width * avia_gt_get_bpp()) + 3) >> 2);
	else if (avia_gt_chip(GTX))
		gtx_reg_set(GMR, STRIDE, ((input_width * avia_gt_get_bpp()) + 1) >> 1);
}

int avia_gt_gv_show(void)
{
	switch (input_mode) {
	case AVIA_GT_GV_INPUT_MODE_OFF:
		if (avia_gt_chip(ENX))
			enx_reg_set(GMR1, GMD, 0x00);
		else if (avia_gt_chip(GTX))
			gtx_reg_set(GMR, GMD, 0x00);
		break;
	case AVIA_GT_GV_INPUT_MODE_RGB4:
		if (avia_gt_chip(ENX))
			enx_reg_set(GMR1, GMD, 0x02);
		else if (avia_gt_chip(GTX))
			gtx_reg_set(GMR, GMD, 0x01);
		break;
	case AVIA_GT_GV_INPUT_MODE_RGB8:
		if (avia_gt_chip(ENX))
			enx_reg_set(GMR1, GMD, 0x06);
		else if (avia_gt_chip(GTX))
			gtx_reg_set(GMR, GMD, 0x02);
		break;
	case AVIA_GT_GV_INPUT_MODE_RGB16:
		if (avia_gt_chip(ENX))
			enx_reg_set(GMR1, GMD, 0x03);
		else if (avia_gt_chip(GTX))
			gtx_reg_set(GMR, GMD, 0x03);
		break;
	case AVIA_GT_GV_INPUT_MODE_RGB32:
		if (avia_gt_chip(ENX))
			enx_reg_set(GMR1, GMD, 0x07);
		else if (avia_gt_chip(GTX))
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int avia_gt_gv_init(void)
{
	printk(KERN_INFO "avia_gt_gv: $Id: avia_gt_gv.c,v 1.34 2003/08/01 17:31:22 obi Exp $\n");

	gt_info = avia_gt_get_info();

	if (!avia_gt_supported_chipset(gt_info))
		return -ENODEV;

	avia_gt_reg_set(RSTR0, GFIX, 1);
	avia_gt_reg_set(RSTR0, GFIX, 0);

	avia_gt_gv_cursor_hide();
#ifdef WORKAROUND_MEMORY_TIMING
	udelay(100);
#endif /* WORKAROUND_MEMORY_TIMING */
	avia_gt_gv_set_pos(0, 0);
#ifdef WORKAROUND_MEMORY_TIMING
	udelay(100);
#endif /* WORKAROUND_MEMORY_TIMING */
	avia_gt_gv_set_input_size(720, 576);
#ifdef WORKAROUND_MEMORY_TIMING
	udelay(100);
#endif /* WORKAROUND_MEMORY_TIMING */
	avia_gt_gv_set_size(720, 576);

	if (avia_gt_chip(ENX)) {
#ifdef WORKAROUND_MEMORY_TIMING
		udelay(1000);
#endif /* WORKAROUND_MEMORY_TIMING */

		//enx_reg_set(GMR1, P, 1);
		enx_reg_set(GMR1, S, 1);
		enx_reg_set(GMR1, B, 0);
		//enx_reg_set(GMR1, BANK, 1);

		//enx_reg_set(BALP, AlphaOut, 0x00);
		//enx_reg_set(BALP, AlphaIn, 0x00);

		enx_reg_set(G1CFR, CFT, 0x1);
		enx_reg_set(G2CFR, CFT, 0x1);

		enx_reg_set(GBLEV1, BLEV11, 0x00);
		enx_reg_set(GBLEV1, BLEV10, 0x20);

#ifdef WORKAROUND_MEMORY_TIMING
		udelay(1000);
#endif /* WORKAROUND_MEMORY_TIMING */

		// schwarzer consolen hintergrund nicht transpartent
		enx_reg_set(TCR1, E, 0x1);
		enx_reg_set(TCR1, Red, 0xFF);
		enx_reg_set(TCR1, Green, 0x00);
		enx_reg_set(TCR1, Blue, 0x7F);

		// disabled - we don't need since we have 7bit true alpha
		enx_reg_set(TCR2, E, 0x0);
		enx_reg_set(TCR2, Red, 0xFF);
		enx_reg_set(TCR2, Green, 0x00);
		enx_reg_set(TCR2, Blue, 0x7F);

#ifdef WORKAROUND_MEMORY_TIMING
		udelay(1000);
#endif /* WORKAROUND_MEMORY_TIMING */

		enx_reg_set(VBR, E, 0x0);
		enx_reg_set(VBR, Y, 0x00);
		enx_reg_set(VBR, Cr, 0x00);
		enx_reg_set(VBR, Cb, 0x00);

		enx_reg_set(VCR, D, 0x1);
/*    Chroma Sense: do not activate , otherwise red and blue will be swapped */
/*		enx_reg_set(VCR, C, 0x1); */

		enx_reg_set(VMCR, FFM, 0x0);

		enx_reg_set(GVSA1, Addr, AVIA_GT_MEM_GV_OFFS >> 2);
	}
	else if (avia_gt_chip(GTX)) {
		// chroma filter. evtl. average oder decimate, bei text
		gtx_reg_set(GMR, CFT, 0x3);
		gtx_reg_set(GMR, BLEV1, 0x00);
		gtx_reg_set(GMR, BLEV0, 0x02);

		// ekelhaftes rosa als transparent
		gtx_reg_set(TCR, E, 0x1);
		gtx_reg_set(TCR, R, 0x1F);
		gtx_reg_set(TCR, G, 0x00);
		gtx_reg_set(TCR, B, 0x0F);

		gtx_reg_set(VHT, Width, 858);
		gtx_reg_set(VLT, VBI, 21); // NTSC = 18, PAL = 21
		gtx_reg_set(VLT, Lines, 623);

		// white cursor
		gtx_reg_set(CCR, R, 0x1F);
		gtx_reg_set(CCR, G, 0x1F);
		gtx_reg_set(CCR, B, 0x1F);

		// decoder sync. HSYNC polarity einstellen? low vs. high active?
		gtx_reg_set(VCR, HP, 0x2);
		gtx_reg_set(VCR, FP, 0x0);
		gtx_reg_set(VCR, D, 0x1);

		// enable dynamic clut
		gtx_reg_set(GFUNC, D, 0x1);

		// disable background
		gtx_reg_set(VBR, E, 0x0);
		gtx_reg_set(VBR, Y, 0x00);
		gtx_reg_set(VBR, Cr, 0x00);
		gtx_reg_set(VBR, Cb, 0x00);

		gtx_reg_set(GVSA, Addr, AVIA_GT_MEM_GV_OFFS >> 1);
	}

	return 0;
}

void __exit avia_gt_gv_exit(void)
{
	avia_gt_reg_set(RSTR0, GFIX, 1);
}

#if defined(STANDALONE)
module_init(avia_gt_gv_init);
module_exit(avia_gt_gv_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("AViA eNX/GTX graphics viewport driver");
MODULE_LICENSE("GPL");
#endif

EXPORT_SYMBOL(avia_gt_gv_copyarea);
EXPORT_SYMBOL(avia_gt_gv_get_clut);
EXPORT_SYMBOL(avia_gt_gv_get_info);
EXPORT_SYMBOL(avia_gt_gv_set_blevel);
EXPORT_SYMBOL(avia_gt_gv_set_clut);
EXPORT_SYMBOL(avia_gt_gv_set_input_mode);
EXPORT_SYMBOL(avia_gt_gv_set_input_size);
EXPORT_SYMBOL(avia_gt_gv_set_pos);
EXPORT_SYMBOL(avia_gt_gv_set_size);
EXPORT_SYMBOL(avia_gt_gv_hide);
EXPORT_SYMBOL(avia_gt_gv_show);
