/*
 *   gen-fb.c - AViA GTX framebuffer driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2001 Felix "tmbinc" Domke (tmbinc@gmx.net)
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
 *   $Log: avia_gt_fb.c,v $
 *   Revision 1.9  2001/09/13 17:07:00  field
 *   Fixed ENX-Framebuffer position bug
 *
 *   Revision 1.8  2001/07/13 17:08:23  McClean
 *   fix framebuffer screenposition bug
 *
 *   Revision 1.7  2001/06/09 18:49:55  tmbinc
 *   fixed gtx-setcolreg.
 *
 *   Revision 1.6  2001/05/27 20:47:51  TripleDES
 *
 *   fixed the transparency for fb-console but this works not very fine with other modes (no black) :(  ...only that the fb-console runs, again ;)
 *   -
 *
 *   Revision 1.5  2001/05/26 22:22:24  TripleDES
 *
 *   fixed the grey-backgrund
 *
 *   Revision 1.4  2001/05/04 21:07:55  fnbrd
 *   Rand gefixed.
 *
 *   Revision 1.3  2001/04/30 21:51:38  tmbinc
 *   fixed setcolreg for eNX
 *
 *   Revision 1.2  2001/04/22 13:56:35  tmbinc
 *   other philips- (and maybe sagem?) fixes
 *
 *   Revision 1.1  2001/04/20 01:21:33  Jolt
 *   Final Merge :-)
 *
 *   Revision 1.11  2001/03/23 08:00:30  gillem
 *   - adjust xpos of fb
 *
 *   Revision 1.10  2001/03/08 01:15:14  tmbinc
 *   smem_length 1MB now, transparent color, defaults to dynaclut.
 *
 *   Revision 1.9  2001/02/11 16:32:08  tmbinc
 *   fixed viewport position
 *
 *   Revision 1.8  2001/02/04 20:46:14  tmbinc
 *   improved framebuffer.
 *
 *   Revision 1.7  2001/01/31 17:17:46  tmbinc
 *   Cleaned up avia drivers. - tmb
 *
 *   $Revision: 1.9 $
 *
 */

 /*
    This is the improved FB.
    Report bugs as usual.
    
    CLUTs completely untested. (just saw: they work.)

    There were other attempts to rewrite this driver, but i don't
    know the state of this work.
    
    roh suxx.
 */

#define TCR_COLOR 0xFC0F
#define BLEVEL		0x20

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
#include <linux/fb.h>
#include <linux/init.h>
#include <video/fbcon.h>
#include <video/fbcon-cfb16.h>

#ifdef GTX
#include "dbox/gtx.h"
#endif // GTX

#ifdef ENX
#include "dbox/enx.h"
#endif // ENX

#define RES_X           720
#define RES_Y           576

static int debug=0;

#ifdef MODULE
MODULE_PARM(debug,"i");
#endif

#define dprintk(fmt,args...) if(debug) printk( fmt,## args)

static unsigned char* gtxmem;
static unsigned char* gtxreg;

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

struct gtxfb_info
{
  struct fb_info_gen gen;

  void *videobase;
  int offset;
  u32 videosize, pvideobase;
};

struct gtxfb_par
{
  int pal;              // 1 - PAL, 0 - NTSC
  int bpp;              // 4, 8, 16
  int lowres;           // 0: 720 or 640 pixels per line 1: 360 or 320 pixels per line
  int interlaced;       // 0: 480 or 576 lines interlaced 1: 240 or 288 lines non-interlaced
  int xres, yres;       // calculated of the above stuff
  int stride;
};

static struct gtxfb_info fb_info;
static struct gtxfb_par current_par;
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
  struct gtxfb_par *par=(struct gtxfb_par *)fb_par;
  strcpy(fix->id, "AViA eNX/GTX Framebuffer");
  fix->type=FB_TYPE_PACKED_PIXELS;
  fix->type_aux=0;
  
  if (par->bpp!=16)
    fix->visual=FB_VISUAL_PSEUDOCOLOR;
  else
    fix->visual=FB_VISUAL_TRUECOLOR;

  fix->line_length=par->stride;
  fix->smem_start=(unsigned long)fb_info.pvideobase;
  fix->smem_len=1024*1024;                            // fix->line_length*par->yres;
  fix->mmio_start=(unsigned long)fb_info.pvideobase;  // gtxmem;
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
  struct gtxfb_par *par=(struct gtxfb_par *)fb_par;
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
  struct gtxfb_par *par=(struct gtxfb_par *)fb_par;
  var->xres=par->xres;
  var->yres=par->yres;
  var->bits_per_pixel=par->bpp;
  if (par->pal==0)              // ob man sich auf diese timing values sooo verlassen sollte ;)
  {
    var->left_margin=116;
    var->right_margin=21;
    var->upper_margin=18;
    var->lower_margin=5;
  } else
  {
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
  struct gtxfb_par *par=(struct gtxfb_par *)fb_par;
  if (current_par_valid)
    *par=current_par;
  else
    gtx_decode_var(&default_var, par, info);
}

static void gtx_set_par(const void *fb_par, struct fb_info_gen *info)
{
  struct gtxfb_par *par=(struct gtxfb_par *)fb_par;
  int val,i;
	int div,rem;

#ifdef GTX

  rh(VCR)=0x340; // decoder sync. HSYNC polarity einstellen? low vs. high active?
  rh(VHT)=par->pal?858:852;
  rh(VLT)=par->pal?(623|(21<<11)):(523|(18<<11));

  switch (par->bpp)
  {
  case 4:
    val=1<<30; break;
  case 8:
    val=2<<30; break;
  case 16:
    val=3<<30; break;
  }

  if (par->lowres)
    val|=1<<29;

  if (!par->pal)
    val|=1<<28;                         // NTSC square filter. TODO: do we need this?

                // TODO: cursor
  if (!par->interlaced)
    val|=1<<26;

  val|=3<<24;                           // chroma filter. evtl. average oder decimate, bei text
  val|=0<<20;                           // BLEV1 = 8/8
  val|=2<<16;                           // BLEV2 = 6/8	-> BLEVEL
  val|=par->stride;

  rw(GMR)=val;
  
  rh(CCR)=0x7FFF;                  // white cursor
  rw(GVSA)=fb_info.offset;                      // dram start address
  rh(GVP)=0;

  VCR_SET_HP(2);
  VCR_SET_FP(0);
  val=par->pal?127:117;
  val*=8;

  if (par->lowres)
	{
		if (rw(GMR)&(1<<28))
			div=18;
		else
			div=16;
	}
	else
	{
		if (rw(GMR)&(1<<28))
			div=9;
		else
			div=8;
	}

	rem = val-((val/div)*div);
	val/=div;

	// ???
  //val-=(3+20);						//PFUSCHING BY MCCLEAN!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  val -= 55;
  GVP_SET_COORD(val, (par->pal?42:36));                 // TODO: NTSC?

	/* set SPP */
	rw(GVP) |= (rem<<27);

  rh(GFUNC)=0x10;			// enable dynamic clut
  rh(TCR)=TCR_COLOR;                       // ekelhaftes rosa als transparent

                                        // DEBUG: TODO: das ist nen kleiner hack hier.
/*  if (par->lowres)
    GVS_SET_XSZ(par->xres*2);
  else */
	GVS_SET_XSZ(par->xres);

  if (par->interlaced)
    GVS_SET_YSZ(par->yres);
  else
    GVS_SET_YSZ(par->yres*2);

//	rw(GVS)|=(0<<27);

  rw(VBR)=0;                       // disable background..

#endif // GTX

#ifdef ENX

#define ENX_VCR_SET_HP(X)    enx_reg_h(VCR) = ((enx_reg_h(VCR)&(~(3<<10))) | ((X&3)<<10))
#define ENX_VCR_SET_FP(X)    enx_reg_h(VCR) = ((enx_reg_h(VCR)&(~(3<<8 ))) | ((X&3)<<8 ))
#define ENX_GVP_SET_SPP(X)   enx_reg_w(GVP1) = ((enx_reg_w(GVP1)&(~(0x01F<<27))) | ((X&0x1F)<<27))
#define ENX_GVP_SET_X(X)     enx_reg_w(GVP1) = ((enx_reg_w(GVP1)&(~(0x3FF<<16))) | ((X&0x3FF)<<16))
#define ENX_GVP_SET_Y(X)     enx_reg_w(GVP1) = ((enx_reg_w(GVP1)&(~0x3FF))|(X&0x3FF))
#define ENX_GVP_SET_COORD(X,Y) ENX_GVP_SET_X(X); ENX_GVP_SET_Y(Y)


#define ENX_GVS_SET_IPS(X)   enx_reg_w(GVSZ1) = ((enx_reg_w(GVSZ1)&0xFC000000) | ((X&0x3f)<<27))
#define ENX_GVS_SET_XSZ(X)   enx_reg_w(GVSZ1) = ((enx_reg_w(GVSZ1)&(~(0x3FF<<16))) | ((X&0x3FF)<<16))
#define ENX_GVS_SET_YSZ(X)   enx_reg_w(GVSZ1) = ((enx_reg_w(GVSZ1)&(~0x3FF))|(X&0x3FF))


  enx_reg_w(VBR)=0;

  enx_reg_h(VCR)=0x040|(1<<13);
  enx_reg_h(BALP)=0;
  enx_reg_h(VHT)=(par->pal?857:851)|0x5000;
  enx_reg_h(VLT)=par->pal?(623|(21<<11)):(523|(18<<11));

//  Field: auskommentiert, behebt FB-Positions-Bug!
//  enx_reg_h(VAS)=par->pal?63:58;

	val=0;
	if (par->lowres)
		val|=1<<31;

	if (!par->pal)
		val|=1<<30;                         // NTSC square filter. TODO: do we need this?

	if (!par->interlaced)
		val|=1<<29;

  val|=1<<26;                           // chroma filter. evtl. average oder decimate, bei text

  switch (par->bpp)
  {
	case 4:
		val|=2<<20; break;
	case 8:
		val|=6<<20; break;
	case 16:
		val|=3<<20; break;
	case 32:
		val|=7<<20; break;
  }

  val|=par->stride;

  enx_reg_w(TCR1)=0x1000000;
  enx_reg_w(TCR2)=0x0FF007F;	// disabled - we don't need since we have 7bit true alpha

	enx_reg_h(P1VPSA)=0;
	enx_reg_h(P2VPSA)=0;
	enx_reg_h(P1VPSO)=0;
	enx_reg_h(P2VPSO)=0;
	enx_reg_h(VMCR)=0;
	enx_reg_h(G1CFR)=1;
	enx_reg_h(G2CFR)=1;

	enx_reg_w(GMR1)=val;
	enx_reg_w(GMR2)=0;
  enx_reg_h(GBLEV1)=BLEVEL;
  enx_reg_h(GBLEV2)=0;
//JOLT  enx_reg_h(CCR)=0x7FFF;                  // white cursor
	enx_reg_w(GVSA1)=fb_info.offset; 	// dram start address
	enx_reg_h(GVP1)=0;

//  dprintk("Framebuffer: val: 0x%08x\n", val);

//  Field: changed according to ppc-boot (fixes position)
//  ENX_GVP_SET_COORD(129,43);                 // TODO: NTSC?
  ENX_GVP_SET_COORD(113,42);

  ENX_GVP_SET_SPP(63);
//  ENX_GVP_SET_COORD(90,43);                 // TODO: NTSC?
                                        // DEBUG: TODO: das ist nen kleiner hack hier.
/*  if (lowres)
    ENX_GVS_SET_XSZ(xres);
  else
    ENX_GVS_SET_XSZ(xres*2);*/

  ENX_GVS_SET_IPS(32);
  ENX_GVS_SET_XSZ(RES_X);
  ENX_GVS_SET_YSZ(RES_Y);

/*  if (interlaced)
    ENX_GVS_SET_YSZ(yres);
  else
    ENX_GVS_SET_YSZ(yres*2);*/

#endif // ENX

  current_par = *par;
  current_par_valid=1;
}


static int gtx_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                u_int *transp, struct fb_info *info)
{
  u16 val;
  if (regno>255)
    return 1;

#ifdef GTX

  rh(CLTA)=regno;
  mb();
  // ARRR RRGG GGGB BBBB
  // 8000 i
  // 7c00 r
  // 03e0 g
  // 001F b
  val=rh(CLTD);
  if (val==TCR_COLOR)
  {
  	*red=*green=*blue=0;
  	*transp=255;
  } else
  {
	  *red=((val&0x7C00)>>10)<<19;
	  *green=((val&0x3E0)>>5)<<19;
	  *blue=(val&0x1F)       <<19;
	  *transp=(val&0x8000)?BLEVEL:0;
	}
#endif

#ifdef ENX

  enx_reg_h(CLUTA)=regno;
  mb();
  val=enx_reg_h(CLUTD);

  if (transp)
    *transp = ((val & 0xFF000000) >> 24);

  if (red)
    *red = ((val & 0x00FF0000) >> 16);
    
  if (green)    
    *green = ((val & 0x0000FF00) >> 8);
    
  if (blue)
    *blue = (val & 0x000000FF);

#endif  
  
  return 0;
}

static int gtx_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                u_int transp, struct fb_info *info)
{
  if (regno>255)
    return 0;
  

#ifdef GTX

  red>>=11;
  green>>=11;
  blue>>=11;
  transp>>=8;
  
  rh(CLTA)=regno;
  mb();
  if (transp>=0x80)		// full transparency
  {
  	rh(CLTD)=TCR_COLOR;
  } else
  {
  	if (!transp)
			transp=1;
		else
			transp=0;
		rh(CLTD)=(transp<<15)|(red<<10)|(green<<5)|(blue);
	}
  
#endif // GTX

#ifdef ENX

  red>>=8;
  green>>=8;
  blue>>=8;
  transp>>=8;
  
  enx_reg_h(CLUTA) = regno;
  mb();
  enx_reg_w(CLUTD) = ((transp << 24) | (blue << 16) | (green << 8) | (red));

  red>>=3;
  green>>=3;
  blue>>=3;

#endif // ENX
  
#ifdef FBCON_HAS_CFB16
  if (regno<16)
    fbcon_cfb16_cmap[regno]=((!!transp)<<15)|(red<<10)|(green<<5)|(blue);
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
  struct gtxfb_par *par=(struct gtxfb_par *)fb_par;
   
  disp->screen_base=(char*)fb_info.videobase;
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

int __init gtxfb_init(void)
{
#ifdef GTX
  gtxmem=gtx_get_mem();
  gtxreg=gtx_get_reg();

  fb_info.videosize=1*1024*1024;                // TODO: moduleparm?
//  fb_info.offset=gtx_allocate_dram(fb_info.videosize, 1);

  fb_info.offset=1*1024*1024;
  fb_info.videobase=gtxmem+fb_info.offset;
  fb_info.pvideobase=GTX_PHYSBASE+fb_info.offset;
#endif

#ifdef ENX
  gtxmem=enx_get_mem_addr();
  gtxreg=enx_get_reg_addr();

  fb_info.videosize=1*1024*1024;                // TODO: moduleparm?
//  fb_info.offset=gtx_allocate_dram(fb_info.videosize, 1);

  fb_info.offset=ENX_FB_OFFSET;
  fb_info.videobase=gtxmem+fb_info.offset;
  fb_info.pvideobase=ENX_MEM_BASE+fb_info.offset;
#endif

  fb_info.gen.info.node = -1;
  fb_info.gen.info.flags = FBINFO_FLAG_DEFAULT;
  fb_info.gen.info.fbops = &gtxfb_ops;
  fb_info.gen.info.disp = &disp;
  fb_info.gen.info.changevar = NULL;
  fb_info.gen.info.switch_con = &fbgen_switch;
  fb_info.gen.info.updatevar = &fbgen_update_var;
  fb_info.gen.info.blank = &fbgen_blank;
  strcpy(fb_info.gen.info.fontname, default_fontname);
  fb_info.gen.parsize=sizeof(struct gtxfb_par);
  fb_info.gen.fbhw=&gtx_switch;
  fb_info.gen.fbhw->detect();

  strcpy(fb_info.gen.info.modename, "AViA eNX/GTX Framebuffer");

  fbgen_get_var(&disp.var, -1, &fb_info.gen.info);
  disp.var.activate = FB_ACTIVATE_NOW;
  fbgen_do_set_var(&disp.var, 1, &fb_info.gen);
  fbgen_set_disp(-1, &fb_info.gen);
  fbgen_install_cmap(0, &fb_info.gen);

  if (register_framebuffer(&fb_info.gen.info) < 0)
    return -EINVAL;

  printk(KERN_INFO "fb%d: %s frame buffer device\n", 
         GET_FB_IDX(fb_info.gen.info.node), fb_info.gen.info.modename);

#ifdef MODULE
  atomic_set(&THIS_MODULE->uc.usecount, 1);
#endif
  return 0;
}

void gtxfb_close(void)
{
  unregister_framebuffer((struct fb_info*)&fb_info);
}

#ifdef MODULE

int init_module(void)
{
  dprintk("Framebuffer: $Id: avia_gt_fb.c,v 1.9 2001/09/13 17:07:00 field Exp $\n");
  return gtxfb_init();
}
void cleanup_module(void)
{
  gtxfb_close();
}
EXPORT_SYMBOL(cleanup_module);

#endif
