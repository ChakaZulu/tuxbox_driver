/*
 *   gtp-fb.c - AViA framebuffer driver (dbox-II-project)
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
 *   $Log: gtp-fb.c,v $
 *   Revision 1.2  2001/04/19 23:32:27  Jolt
 *   Merge Part II
 *
 *   Revision 1.1  2001/04/17 22:57:34  Jolt
 *   Merged framebuffer
 *
 *   Revision 1.0  2001/04/17 08:00:30  Jolt
 *   - initial import
 *
 *   $Revision: 1.2 $
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
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
#include <linux/fb.h>
#include <linux/init.h>
#include <video/fbcon.h>
#include <video/fbcon-cfb16.h>

#include "gtp-core.h"
#include "gtp-fb.h"

#define RES_X           720
#define RES_Y           576

static struct fb_var_screeninfo default_var = {
    /* 720x576, 16 bpp */               // TODO: NTSC
    RES_X, RES_Y, RES_X, RES_Y,         /* W,H, W, H (virtual) load xres,xres_virtual*/
    0, 0,                               /* virtual -> visible no offset */
    16, 0,                              /* depth -> load bits_per_pixel */
    {10, 5, 0},                         /* ARGB 1555 */
    {5, 5, 0}, 
    {0, 5, 0}, 
    {15, 1, 0},                         /* transparency */
    0,                                  /* standard pixel format */
    FB_ACTIVATE_NOW,
    -1, -1, 
    0,                                  /* accel flags */
    20000, 64, 64, 32, 32, 64, 2, 0,    /* sync stuff */
    FB_VMODE_INTERLACED
};

struct gtp_fb_info
{
  struct fb_info_gen gen;

  sGtpDev gtp_dev;
  u32 videosize;
};

struct gtp_fb_par
{
  int pal;              // 1 - PAL, 0 - NTSC
  int bpp;              // 4, 8, 16
  int lowres;           // 0: 720 or 640 pixels per line 1: 360 or 320 pixels per line
  int interlaced;       // 0: 480 or 576 lines interlaced 1: 240 or 288 lines non-interlaced
  int xres, yres;       // calculated of the above stuff
  int stride;
};

static struct gtp_fb_info fb_info;
static struct gtp_fb_par current_par;
static int current_par_valid=0;
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

static void gtx_detect(void);
static int gtx_encode_fix(struct fb_fix_screeninfo *fix, const void *fb_par,
                        struct fb_info_gen *info);
static int gtx_decode_var(const struct fb_var_screeninfo *var, void *fb_par,
                        struct fb_info_gen *info);
static int gtx_encode_var(struct fb_var_screeninfo *var, const void *fb_par,
                        struct fb_info_gen *info);
static void gtx_get_par(void *fb_par, struct fb_info_gen *info);
static void gtx_set_par(const void *fb_par, struct fb_info_gen *info);
static int gtx_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                u_int *transp, struct fb_info *info);
static int gtx_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                u_int transp, struct fb_info *info);
static int gtx_blank(int blank, struct fb_info_gen *info);
static void gtx_set_disp(const void *fb_par, struct display *disp,
                struct fb_info_gen *info);

int gtx_setup(char*);
int gtx_init(void);
void gtx_cleanup(struct fb_info *info);

static void gtx_detect(void)
{
  return;
}
    
static int gtx_encode_fix(struct fb_fix_screeninfo *fix, const void *fb_par,
                          struct fb_info_gen *info)
{

  struct gtp_fb_par *par=(struct gtp_fb_par *)fb_par;
  
  strcpy(fix->id, "AViA Framebuffer");
  fix->type=FB_TYPE_PACKED_PIXELS;
  fix->type_aux=0;
  
  if (par->bpp!=16)
    fix->visual=FB_VISUAL_PSEUDOCOLOR;
  else
    fix->visual=FB_VISUAL_TRUECOLOR;

  fix->line_length=par->stride;
  fix->smem_start=(unsigned long)fb_info.gtp_dev.fb_mem_phys;
  fix->smem_len=1024*1024;                            // fix->line_length*par->yres;
  fix->mmio_start=(unsigned long)fb_info.gtp_dev.fb_mem_lin; 
  fix->mmio_len=0x410000;
  
  fix->xpanstep=0;
  fix->ypanstep=0;
  fix->ywrapstep=0;
  
  fix->accel=0;

  return 0;
}

static int gtx_decode_var(const struct fb_var_screeninfo *var, void *fb_par,
                          struct fb_info_gen *info)
{
  struct gtp_fb_par *par=(struct gtp_fb_par *)fb_par;
  int yres;
  
  if ((var->bits_per_pixel != 4) && (var->bits_per_pixel != 8) && (var->bits_per_pixel != 16))
    return -EINVAL;
    
  par->bpp=var->bits_per_pixel;
  
  if (var->xres_virtual != var->xres ||
      var->yres_virtual != var->yres ||
      var->nonstd)
  {
    printk("unmatched vres. %d x %d vs %d x %d  -> %d\n", var->xres_virtual, var->yres_virtual, var->xres, var->yres, var->nonstd);
    return -EINVAL;
  }
  
  yres=var->yres;
  
  if (var->xres < 640)
    par->lowres=1;
  else
    par->lowres=0;
    
  if (var->yres >= 480)
  {
    par->interlaced=1;
    yres>>=1;
  } else
    par->interlaced=0;
  
  if (yres==240)
    par->pal=0;
  else if (yres==288)
    par->pal=1;
  else
  {
    printk("invalid yres: %d\n", var->yres);
    return -EINVAL;
  }
  
  par->xres=var->xres;
  par->yres=var->yres;

  if (par->bpp==4)
    par->stride=var->xres/2;
  else if (par->bpp==8)
    par->stride=var->xres;
  else if (par->bpp==16)
    par->stride=var->xres*2;
              
  return 0;
}

static int gtx_encode_var(struct fb_var_screeninfo *var, const void *fb_par,
                           struct fb_info_gen *info)
{

  struct gtp_fb_par *par=(struct gtp_fb_par *)fb_par;

  var->xres=par->xres;
  var->yres=par->yres;
  var->bits_per_pixel=par->bpp;
  
  if (par->pal==0)              // ob man sich auf diese timing values sooo verlassen sollte ;)
  {
  
    var->left_margin=116;
    var->right_margin=21;
    var->upper_margin=18;
    var->lower_margin=5;
    
  } else  {
  
    var->left_margin=126;
    var->right_margin=18;
    var->upper_margin=21;
    var->lower_margin=5;
    
  }
  
  var->hsync_len=var->vsync_len=0;              // TODO
  var->sync=0;      
  
  var->xres_virtual=var->xres;
  var->yres_virtual=var->yres;
  var->xoffset=var->yoffset=0;

  var->red.offset=10;
  var->green.offset=5;
  var->blue.offset=0;
  var->transp.offset=15;
  var->grayscale=0;
  var->red.length=var->green.length=var->blue.length=5;
  var->transp.length=1;
  var->red.msb_right=var->green.msb_right=var->blue.msb_right=var->transp.msb_right=0;
  
  var->xoffset=var->yoffset=0;
  var->pixclock=0;
  var->nonstd=0;
  var->activate=0;
  var->height=var->width-1;
  var->accel_flags=0;

  return 0;
  
}

static void gtx_get_par(void *fb_par, struct fb_info_gen *info)
{

  struct gtp_fb_par *par=(struct gtp_fb_par *)fb_par;
  
  if (current_par_valid)
    *par=current_par;
  else
    gtx_decode_var(&default_var, par, info);
    
}

static void gtx_set_par(const void *fb_par, struct fb_info_gen *info)
{

  struct gtp_fb_par *par=(struct gtp_fb_par *)fb_par;

  fb_info.gtp_dev.fb_param_set(par->pal, par->bpp, par->lowres, par->interlaced, par->xres, par->yres, par->stride);
  
  current_par = *par;
  current_par_valid=1;
  
}


static int gtx_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                u_int *transp, struct fb_info *info)
{

  if (regno > 255)
    return 1;

  fb_info.gtp_dev.fb_clut_get(regno, red, green, blue, transp);
  
  return 0;
  
}

static int gtx_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                u_int transp, struct fb_info *info)
{

  if (regno>255)
    return 0;
  
  red>>=11;
  green>>=11;
  blue>>=11;
  transp=!!transp;
  
  fb_info.gtp_dev.fb_clut_set(regno, red, green, blue, transp);

#ifdef FBCON_HAS_CFB16
  if (regno<16)
    fbcon_cfb16_cmap[regno]=(transp<<15)|(red<<10)|(green<<5)|(blue);
#endif

  return 0;
  
}

static int gtx_blank(int blank, struct fb_info_gen *info)
{
                // TODO: blank evtl. mit dem scartswitch (KILLRGB)
  return 0;
}

static void gtx_set_disp(const void *fb_par, struct display *disp,
                         struct fb_info_gen *info)
{
  struct gtp_fb_par *par=(struct gtp_fb_par *)fb_par;
   
  disp->screen_base=(char*)fb_info.gtp_dev.fb_mem_lin;
  switch (par->bpp)
  {
#if 1
#ifdef FBCON_HAS_CFB4
  case 4:
    disp->dispsw=&fbcon_cfb4;
    disp->dispsw_data=&fbcon_cfb4_cmap;
    break;
#endif
#ifdef FBCON_HAS_CFB8
  case 8:
    disp->dispsw=&fbcon_cfb8;
    disp->dispsw_data=&fbcon_cfb8_cmap;
    break;
#endif
#ifdef FBCON_HAS_CFB16
  case 16:
    disp->dispsw=&fbcon_cfb16;
    disp->dispsw_data=&fbcon_cfb16_cmap;
    break;
#endif
#endif
  default:
    disp->dispsw=&fbcon_dummy;
  }
  disp->scrollmode=SCROLL_YREDRAW;
}

struct fbgen_hwswitch gtx_switch = {
    gtx_detect, 
    gtx_encode_fix,
    gtx_decode_var,
    gtx_encode_var,
    gtx_get_par,
    gtx_set_par,
    gtx_getcolreg,
    gtx_setcolreg,
    0,
    gtx_blank,
    gtx_set_disp,
};

static struct fb_ops gtxfb_ops = {
        owner:          THIS_MODULE,
        fb_get_fix:     fbgen_get_fix,
        fb_get_var:     fbgen_get_var,
        fb_set_var:     fbgen_set_var,
        fb_get_cmap:    fbgen_get_cmap,
        fb_set_cmap:    fbgen_set_cmap,
};

    /*
     *  Initialization
     */

int gtp_attach(sGtpDev *gtp_dev)
{

  memcpy(&fb_info.gtp_dev, gtp_dev, sizeof(sGtpDev));

  fb_info.gen.info.node = -1;
  fb_info.gen.info.flags = FBINFO_FLAG_DEFAULT;
  fb_info.gen.info.fbops = &gtxfb_ops;
  fb_info.gen.info.disp = &disp;
  fb_info.gen.info.changevar = NULL;
  fb_info.gen.info.switch_con = &fbgen_switch;
  fb_info.gen.info.updatevar = &fbgen_update_var;
  fb_info.gen.info.blank = &fbgen_blank;
  strcpy(fb_info.gen.info.fontname, default_fontname);
  fb_info.gen.parsize=sizeof(struct gtp_fb_par);
  fb_info.gen.fbhw=&gtx_switch;
  fb_info.gen.fbhw->detect();

  strcpy(fb_info.gen.info.modename, "GTP Framebuffer");

  fbgen_get_var(&disp.var, -1, &fb_info.gen.info);
  disp.var.activate = FB_ACTIVATE_NOW;
  fbgen_do_set_var(&disp.var, 1, &fb_info.gen);
  fbgen_set_disp(-1, &fb_info.gen);
  fbgen_install_cmap(0, &fb_info.gen);

  if (register_framebuffer(&fb_info.gen.info) < 0)
    return -EINVAL;

  printk(KERN_INFO "fb%d: %s frame buffer device\n", 
         GET_FB_IDX(fb_info.gen.info.node), fb_info.gen.info.modename);


// Ummm .... :-?

//#ifdef MODULE
//  atomic_set(&THIS_MODULE->uc.usecount, 1);
//#endif

  return 0;
}

void gtp_detach(sGtpDev *gtp_dev)
{
  unregister_framebuffer((struct fb_info*)&fb_info);
}

sGtpFb gtp_fb;
int gtp_fb_nr;

#ifdef MODULE

int init_module(void)
{

  gtp_fb.name = "GTP Generic Framebuffer";
  gtp_fb.attach = gtp_attach;
  gtp_fb.detach = gtp_detach;

  gtp_fb_nr = gtp_fb_register(&gtp_fb);

  return (gtp_fb_nr <= 0);
  
}

void cleanup_module(void)
{

  gtp_fb_release(gtp_fb_nr);

}

#endif
