/*
 * $Id: avia_gt_fb_core.c,v 1.48 2003/07/24 01:59:21 homar Exp $
 *
 * AViA eNX/GTX framebuffer driver (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2000-2001 Felix "tmbinc" Domke (tmbinc@gmx.net)
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
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb32.h>
#include <linux/kd.h>
#include <linux/vt_kern.h>

#include <linux/dvb/avia/avia_gt_fb.h>
#include "avia_gt.h"
#include "avia_gt_gv.h"

static sAviaGtInfo *gt_info = NULL;
static int console_transparent = 0;

/* 720x576, 8 bpp */	       // TODO: NTSC
static struct fb_var_screeninfo default_var = {
	.xres = 720,
	.yres = 576,
	.xres_virtual = 720,
	.yres_virtual = 576,
	.xoffset = 0,
	.yoffset = 0,
	.bits_per_pixel = 8,
	.grayscale = 0,
	.red = { .offset = 0, .length = 0, .msb_right = 0 },
	.green = { .offset = 0, .length = 0, .msb_right = 0 },
	.blue = { .offset = 0, .length = 0, .msb_right = 0 },
	.transp = { .offset = 0, .length = 0, .msb_right = 0 },
	.nonstd = 0,
	.activate = FB_ACTIVATE_NOW,
	.height = -1,
	.width = -1,
	.accel_flags = 0,
	.pixclock = 20000,
	.left_margin = 64,
	.right_margin = 64,
	.upper_margin = 32,
	.lower_margin = 32,
	.hsync_len = 64,
	.vsync_len = 2,
	.sync = 0,
	.vmode = FB_VMODE_INTERLACED
};


struct gtxfb_info {
	struct fb_info_gen gen;
	unsigned char *videobase;
	int offset;
	unsigned int videosize;
	unsigned char *pvideobase;
};

struct gtxfb_par {
	// 4, 8, 16
	int bpp;
	// 0: 720 or 640 pixels per line 1: 360 or 320 pixels per line
	int lowres;
	// 0: 480 or 576 lines interlaced 1: 240 or 288 lines non-interlaced
	int interlaced;
	// calculated of the above stuff
	int xres, yres;
	int stride;
};

static struct gtxfb_info fb_info;
static struct gtxfb_par current_par;
static int current_par_valid = 0;
static struct display disp;

static char default_fontname[40] = { 0 };

#ifdef FBCON_HAS_CFB4
static u16 fbcon_cfb4_cmap[16];
#endif
#ifdef FBCON_HAS_CFB8
static u16 fbcon_cfb8_cmap[16];
#endif
#ifdef FBCON_HAS_CFB16
static u16 fbcon_cfb16_cmap[16];
#endif

static int avia_gt_fb_encode_fix(struct fb_fix_screeninfo *fix, const void *fb_par, struct fb_info_gen *info);
static int avia_gt_fb_decode_var(const struct fb_var_screeninfo *var, void *fb_par, struct fb_info_gen *info);
static int avia_gt_fb_encode_var(struct fb_var_screeninfo *var, const void *fb_par, struct fb_info_gen *info);
static void avia_gt_fb_get_par(void *fb_par, struct fb_info_gen *info);
static void avia_gt_fb_set_par(const void *fb_par, struct fb_info_gen *info);
static int avia_gt_fb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue, u_int *transp, struct fb_info *info);
static int avia_gt_fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue, u_int transp, struct fb_info *info);
static int avia_gt_fb_blank(int blank, struct fb_info_gen *info);
static void avia_gt_fb_set_disp(const void *fb_par, struct display *disp, struct fb_info_gen *info);

static
void avia_gt_fb_detect(void)
{
}

static
int avia_gt_fb_encode_fix(struct fb_fix_screeninfo *fix, const void *fb_par, struct fb_info_gen *info)
{
	struct gtxfb_par *par = (struct gtxfb_par *)fb_par;

	strcpy(fix->id, "AViA eNX/GTX Framebuffer improved version");

	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->type_aux = 0;

	if (par->bpp < 16)
		fix->visual = FB_VISUAL_PSEUDOCOLOR;
	else
		fix->visual = FB_VISUAL_TRUECOLOR;

	fix->line_length = par->stride;
	fix->smem_start = (unsigned long)fb_info.pvideobase;
	fix->smem_len = 1024 * 1024;	/* FIXME: AVIA_GT_MEM_GV_SIZE? */

	fix->xpanstep = 0;
	fix->ypanstep = 0;
	fix->ywrapstep = 0;

	if (avia_gt_chip(GTX)) {
		fix->accel = FB_ACCEL_CCUBE_AVIA_GTX;
		fix->mmio_start = (unsigned long)GTX_REG_BASE;
		fix->mmio_len = 0x10000; /* FIXME: GTX_REG_SIZE? */
	}
	else if (avia_gt_chip(ENX)) {
		fix->accel = FB_ACCEL_CCUBE_AVIA_ENX;
		fix->mmio_start = (unsigned long)ENX_REG_BASE;
		fix->mmio_len = 0x10000; /* FIXME: ENX_REG_SIZE? */
	}

	return 0;
}

static
int avia_gt_fb_decode_var(const struct fb_var_screeninfo *var, void *fb_par, struct fb_info_gen *info)
{
	struct gtxfb_par *par = (struct gtxfb_par *)fb_par;
	int yres;

	switch (var->bits_per_pixel) {
	case 4:
	case 8:
	case 16:
		break;
	case 32:
		if (info->info.fix.accel == FB_ACCEL_CCUBE_AVIA_GTX)
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	par->bpp = var->bits_per_pixel;

	yres = var->yres;

	if (var->xres < 640)
		par->lowres = 1;
	else
		par->lowres = 0;

	if (var->yres >= 480) {
		par->interlaced = 1;
		yres >>= 1;
	}
	else {
		par->interlaced = 0;
	}

	par->xres = var->xres;
	par->yres = var->yres;
	par->stride = (var->xres * par->bpp) / 8;

	return 0;
}

static
int avia_gt_fb_encode_var(struct fb_var_screeninfo *var, const void *fb_par, struct fb_info_gen *info)
{
	struct gtxfb_par *par = (struct gtxfb_par *)fb_par;

	var->xres = par->xres;
	var->yres = par->yres;
	var->bits_per_pixel = par->bpp;
	var->left_margin = 126;
	var->right_margin = 18;
	var->upper_margin = 21;
	var->lower_margin = 5;
	var->hsync_len = 0;
	var->vsync_len = 0;
	var->sync = 0;

	var->xres_virtual = var->xres;
	var->yres_virtual = var->yres;

	var->xoffset = 0;
	var->yoffset = 0;

	var->red.length = 5;
	var->red.msb_right = 0;
	var->red.offset = 10;

	var->green.length = 5;
	var->green.msb_right = 0;
	var->green.offset = 5;

	var->blue.length = 5;
	var->blue.msb_right = 0;
	var->blue.offset = 0;

	var->transp.length = 1;
	var->transp.msb_right = 0;
	var->transp.offset = 15;

	var->grayscale = 0;

	var->xoffset = var->yoffset = 0;
	var->pixclock = 0;
	var->nonstd = 0;
	var->activate = 0;
	var->height = var->width - 1;
	var->accel_flags = 0;

	return 0;
}

static
void avia_gt_fb_get_par(void *fb_par, struct fb_info_gen *info)
{
	struct gtxfb_par *par = (struct gtxfb_par *)fb_par;

	if (current_par_valid)
		*par = current_par;
	else
		avia_gt_fb_decode_var(&default_var, par, info);
}

static
void avia_gt_fb_set_par(const void *fb_par, struct fb_info_gen *info)
{
	struct gtxfb_par *par = (struct gtxfb_par *)fb_par;

	switch (par->bpp) {
	case 4:
		avia_gt_gv_set_input_mode(AVIA_GT_GV_INPUT_MODE_RGB4);
		break;
	case 8:
		avia_gt_gv_set_input_mode(AVIA_GT_GV_INPUT_MODE_RGB8);
		break;
	case 16:
		avia_gt_gv_set_input_mode(AVIA_GT_GV_INPUT_MODE_RGB16);
		break;
	case 32:
		avia_gt_gv_set_input_mode(AVIA_GT_GV_INPUT_MODE_RGB32);
		break;
	default:
		printk(KERN_CRIT "avia_gt_fb: invalid bpp: %u\n", par->bpp);
		return;
	}

	avia_gt_gv_set_blevel(0x5D, 0);
	avia_gt_gv_set_pos(0, 0);
	avia_gt_gv_set_input_size(par->xres, par->yres);

	avia_gt_gv_set_size(par->xres, par->yres);
	avia_gt_gv_show();

	current_par = *par;
	current_par_valid = 1;
}

static
int avia_gt_fb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue, u_int *transp, struct fb_info *info)
{
	if (regno > 255)
		return 1;

	avia_gt_gv_get_clut(regno, transp, red, green, blue);

	return 0;
}

static
int avia_gt_fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue, u_int transp, struct fb_info *info)
{
	unsigned short vc_text = 0;

	if (regno > 255)
		return 1;

	if (info->display_fg)
		vc_text = (vt_cons[info->display_fg->vc_num]->vc_mode == KD_TEXT);

	switch (current_par.bpp) {
#ifdef FBCON_HAS_CFB4
	case 4:
		if (regno == 0 && console_transparent && vc_text)
			avia_gt_gv_set_clut(0, 0xffff, 0, 0, 0);
		else
			avia_gt_gv_set_clut(regno, transp, red, green, blue);
		break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:
		if ((regno == 0) && console_transparent && vc_text)
			avia_gt_gv_set_clut(0, 0xFFFF, 0, 0, 0);
		else
			avia_gt_gv_set_clut(regno, transp, red, green, blue);
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		red >>= 11;
		green >>= 11;
		blue >>= 11;
		transp >>= 15;

		if ((regno == 0) && console_transparent && vc_text)
			fbcon_cfb16_cmap[0] = 0xfc0f;

		if (regno < 16)
			fbcon_cfb16_cmap[regno] = (transp << 15) | (red << 10) | (green << 5) | (blue);
		break;
#endif
	default:
		return 1;
	}

	return 0;
}

static
int avia_gt_fb_blank(int blank, struct fb_info_gen *info)
{
	return 0;
}

static
void avia_gt_fb_set_disp(const void *fb_par, struct display *disp, struct fb_info_gen *info)
{
	struct gtxfb_par *par = (struct gtxfb_par *)fb_par;

	disp->screen_base = (char *)fb_info.videobase;

	switch (par->bpp) {
#ifdef FBCON_HAS_CFB4
	case 4:
		disp->dispsw = &fbcon_cfb4;
		disp->dispsw_data = &fbcon_cfb4_cmap;
		break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:
		disp->dispsw = &fbcon_cfb8;
		disp->dispsw_data = &fbcon_cfb8_cmap;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		disp->dispsw = &fbcon_cfb16;
		disp->dispsw_data = &fbcon_cfb16_cmap;
		break;
#endif
	default:
		disp->dispsw = &fbcon_dummy;
		break;
	}

#ifdef CONFIG_FBCON_SHIFT
	disp->shift_x = 4;
	disp->shift_y = 2;
#endif /* CONFIG_FBCON_SHIFT */

	disp->scrollmode = SCROLL_YREDRAW;
}

struct fbgen_hwswitch avia_gt_fb_switch = {
	.blank = avia_gt_fb_blank,
	.decode_var = avia_gt_fb_decode_var,
	.detect = avia_gt_fb_detect,
	.encode_fix = avia_gt_fb_encode_fix,
	.encode_var = avia_gt_fb_encode_var,
	.get_par = avia_gt_fb_get_par,
	.getcolreg = avia_gt_fb_getcolreg,
	.set_disp = avia_gt_fb_set_disp,
	.set_par = avia_gt_fb_set_par,
	.setcolreg = avia_gt_fb_setcolreg,
};

static int avia_gt_fb_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg, int con, struct fb_info *info)
{
	fb_copyarea copyarea;

	switch (cmd) {
	case AVIA_GT_GV_COPYAREA:
		if (copy_from_user(&copyarea, (void *)arg, sizeof(copyarea)))
			return -EFAULT;
			
		avia_gt_gv_copyarea(copyarea.sx, copyarea.sy, copyarea.width, copyarea.height, copyarea.dx, copyarea.dy);
		break;

	case AVIA_GT_GV_SET_BLEV:
		avia_gt_gv_set_blevel((arg >> 8) & 0xFF, arg & 0xFF);
		break;

	case AVIA_GT_GV_SET_POS:
		avia_gt_gv_set_pos((arg >> 16) & 0xFFFF, arg & 0xFFFF);
		break;

	case AVIA_GT_GV_HIDE:
		avia_gt_gv_hide();
		break;

	case AVIA_GT_GV_SHOW:
		avia_gt_gv_show();
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static struct fb_ops avia_gt_fb_ops = {
	.owner = THIS_MODULE,
	.fb_open = NULL,
	.fb_release = NULL,
	.fb_get_fix = fbgen_get_fix,
	.fb_get_var = fbgen_get_var,
	.fb_set_var = fbgen_set_var,
	.fb_get_cmap = fbgen_get_cmap,
	.fb_set_cmap = fbgen_set_cmap,
	.fb_pan_display = NULL,
	.fb_ioctl = avia_gt_fb_ioctl,
	.fb_mmap = NULL,
	.fb_rasterimg = NULL,
};

int __init avia_gt_fb_init(void)
{
	printk(KERN_INFO "avia_gt_fb: $Id: avia_gt_fb_core.c,v 1.48 2003/07/24 01:59:21 homar Exp $\n");

	gt_info = avia_gt_get_info();

	if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {
		printk(KERN_ERR "avia_gt_fb: Unsupported chip type\n");
		return -EIO;
	}

	avia_gt_gv_get_info(&fb_info.pvideobase, &fb_info.videobase, &fb_info.videosize);

	fb_info.offset = AVIA_GT_MEM_GV_OFFS;

	fb_info.gen.info.node = NODEV;
	fb_info.gen.info.flags = FBINFO_FLAG_DEFAULT;
	fb_info.gen.info.fbops = &avia_gt_fb_ops;
	fb_info.gen.info.disp = &disp;
	fb_info.gen.info.changevar = NULL;
	fb_info.gen.info.switch_con = &fbgen_switch;
	fb_info.gen.info.updatevar = &fbgen_update_var;
	fb_info.gen.info.blank = &fbgen_blank;
	strcpy(fb_info.gen.info.fontname, default_fontname);
	fb_info.gen.parsize = sizeof(struct gtxfb_par);
	fb_info.gen.fbhw = &avia_gt_fb_switch;

	strcpy(fb_info.gen.info.modename, "AViA eNX/GTX Framebuffer");

	fbgen_get_var(&disp.var, -1, &fb_info.gen.info);
	disp.var.activate = FB_ACTIVATE_NOW;
	fbgen_do_set_var(&disp.var, 1, &fb_info.gen);
	fbgen_set_disp(-1, &fb_info.gen);
	fbgen_install_cmap(0, &fb_info.gen);

	strcpy(fb_info.gen.info.fontname, "SUN8x16");

	if (register_framebuffer(&fb_info.gen.info) < 0)
		return -EINVAL;

	printk(KERN_INFO "avia_gt_fb: fb%d: %s frame buffer device\n",
		GET_FB_IDX(fb_info.gen.info.node), fb_info.gen.info.modename);

	avia_gt_gv_show();

	return 0;
}

void __exit avia_gt_fb_exit(void)
{
	unregister_framebuffer(&fb_info.gen.info);
}

MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("AViA eNX/GTX framebuffer driver");
MODULE_LICENSE("GPL");
MODULE_PARM(console_transparent, "i");
MODULE_PARM_DESC(console_transparent, "1: black in text mode is transparent");

module_init(avia_gt_fb_init);
module_exit(avia_gt_fb_exit);
