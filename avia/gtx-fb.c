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

#include "gtx.h"

#define RES_X           720
#define RES_Y           576

static void InitGraphics(int pal)
{
  rh(VCR)=0x340; // decoder sync. HSYNC polarity einstellen? low vs. high active?
  rh(VHT)=pal?858:852;
  rh(VLT)=pal?(623|(21<<11)):(523|(18<<11));

  rw(GMR)=(3<<30)|                 // enable 16bit rgb
          (0<<29)|                 // highres
          (0<<28)|                 // no strange filter
          (0<<27)|                 // disable cursor
          (0<<26)|                 // interlace
          (3<<24)|                 // filter chroma
          (4<<20)|                 // transparency level                HACK!
          (0<<16)|                 // "            "
          (RES_X*2);               // stride: RES_X*2 bytes

  rh(CCR)=0x7FFF;                  // white cursor
  rw(GVSA)=0;                      // dram start address
  rh(GVP)=0;
  rw(GVS)=(RES_X<<16)|(RES_Y);

  VCR_SET_HP(2);
  VCR_SET_FP(2);
  GVP_SET_COORD(127,43);
//  GVS_SET_XSZ(740);
//  GVS_SET_YSZ(622);


  rw(VBR)=0;                       // disable background..
}


static struct fb_var_screeninfo default_var = {
    /* 768x576, 16 bpp */               // TODO: NTSC
    RES_X, RES_Y, RES_X, RES_Y, 0, 0, 16, 0,
    {10, 5, 0},          // 1 5 5 5
    {5, 5, 0}, 
    {0, 5, 0}, 
    {15, 1, 0},
    0, 0, -1, -1, 0, 20000, 64, 64, 32, 32, 64, 2,
    0, FB_VMODE_INTERLACED
};


struct gtxfb_info
{
  struct fb_info_gen gen;
};

struct gtxfb_par
{
  int pal;     // 1 - PAL, 0 - NTSC
};

static struct gtxfb_info fb_info;
static struct gtxfb_par current_par;
static int current_par_valid=0;
static struct display disp;
static struct fb_var_screeninfo default_var;

static void *videobase;
static u32 videosize, pvideobase;

static union {
#ifdef FBCON_HAS_CFB16
    u16 cfb16[16];
#endif
} fbcon_cmap;

static void gtx_detect(void);
static int gtx_encode_fix(struct fb_fix_screeninfo *fix, struct gtxfb_par *par,
                          const struct fb_info *info);
static int gtx_decode_var(struct fb_var_screeninfo *var, struct gtxfb_par *par,
                          const struct fb_info *info);
static void gtx_encode_var(struct fb_var_screeninfo *var, struct gtxfb_par *par,
                          const struct fb_info *info);
static void gtx_get_par(struct gtxfb_par *par, const struct fb_info *info);
static void gtx_set_par(struct gtxfb_par *par, const struct fb_info *info);
static int gtx_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                         u_int *transp, struct fb_info *info);
static int gtx_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                         u_int transp, struct fb_info *info);
static int gtx_blank(int blank_mode, const struct fb_info *info);
static int gtx_pan_display(struct fb_var_screeninfo *var,
                           struct gtxfb_par *par, const struct fb_info *info);
static void gtx_set_disp(const void *par, struct display *disp,
                         struct fb_info_gen *info);
                         

static void gtx_detect(void)
{
  struct gtxfb_par par;
 // par.x=RES_X;
 // par.y=RES_Y;
  int offset;
  offset=gtx_allocate_dram(RES_X*RES_Y*2, 1);
  videobase=gtxmem+offset;
//  videosize=RES_X*RES_Y*2;
  videosize=2*1024*1024;
  pvideobase=GTX_PHYSBASE+offset;

  par.pal=1;
  
  gtx_set_par(&par, 0);
  gtx_encode_var(&default_var, &par, 0);
}
    
    
static int gtx_encode_fix(struct fb_fix_screeninfo *fix, struct gtxfb_par *par,
                          const struct fb_info *info)
{
  memset(fix, 0, sizeof(struct fb_fix_screeninfo));
 
  strcpy(fix->id, "AViA GTX Framebuffer");
  fix->smem_start=pvideobase;
  fix->smem_len=videosize;
  fix->type=FB_TYPE_PACKED_PIXELS;
  fix->type_aux=0;
  fix->visual=FB_VISUAL_TRUECOLOR;
  fix->xpanstep=0;
  fix->ypanstep=1;
  fix->ywrapstep=0;
  fix->line_length=RES_X*2;
  return 0;
}

static int gtx_decode_var(struct fb_var_screeninfo *var, struct gtxfb_par *par,
                          const struct fb_info *info)
{
  par->pal=1;  // PAL, TODO
  return 0;
}
static void gtx_encode_var(struct fb_var_screeninfo *var, struct gtxfb_par *par,
                          const struct fb_info *info)
{
  *var=default_var;     // TODO
}

static void gtx_get_par(struct gtxfb_par *par, const struct fb_info *info)
{
  *par=current_par;
}
                          
static void gtx_set_par(struct gtxfb_par *par, const struct fb_info *info)
{
  current_par=*par;
  current_par_valid=1;
}

static int gtx_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                         u_int *transp, struct fb_info *info)
{
  return 1;
}

static int gtx_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                         u_int transp, struct fb_info *info)
{
  if(regno<16)
  {
//    switch(current_par.bpp)
    switch (16)
    {
#ifdef FBCON_HAS_CFB16
    case 16:
      fbcon_cmap.cfb16[regno] =
                ((red   & 0xf800)      ) |
                ((green & 0xfc00) >>  5) |
                ((blue  & 0xf800) >> 11);
      break;
#endif
    }
  }
  return 0;
}

static int gtx_blank(int blank_mode, const struct fb_info *info)
{
    return 0;
}


static int gtx_pan_display(struct fb_var_screeninfo *var,
                           struct gtxfb_par *par, const struct fb_info *info)
{
  /*
   *  Pan (or wrap, depending on the 'vmode' field) the display using the
   *  'offset' and 'y-offset' fields of the 'Var' structure.
   *  If the values don't fit, return -EINVAL.
   */
  printk("gtx_pan_display();\n");
  return 0;
}

static void gtx_set_disp(const void *par, struct display *disp,
                         struct fb_info_gen *info)
{
  /*
   *  Fill in a pointer with the virtual address of the mapped frame buffer.
   *  Fill in a pointer to appropriate low level text console operations (and
   *  optionally a pointer to help data) for the video mode ar' of your
   *  video hardware. These can be generic software routines, or hardware
   *  accelerated routines specifically tailored for your hardware.
   *  If you don't have any appropriate operations, you must fill in a
   *  pointer to dummy operations, and there will be no text output.
   */
  disp->screen_base=videobase;
#ifdef FBCON_HAS_CFB16
  disp->dispsw = &fbcon_cfb16;
  disp->dispsw_data = fbcon_cmap.cfb16;
#else
  #error you unrule.
#endif
}


struct fbgen_hwswitch gtx_switch = {
    gtx_detect, gtx_encode_fix, gtx_decode_var, gtx_encode_var, gtx_get_par,
    gtx_set_par, gtx_setcolreg, gtx_getcolreg, gtx_pan_display, gtx_blank,
    gtx_set_disp
};

static struct fb_ops gtxfb_ops = {
        owner:          THIS_MODULE,
        fb_get_fix:     fbgen_get_fix,
        fb_get_var:     fbgen_get_var,
        fb_set_var:     fbgen_set_var,
        fb_get_cmap:    fbgen_get_cmap,
        fb_set_cmap:    fbgen_set_cmap,
        fb_pan_display: fbgen_pan_display,
//        fb_ioctl:       gtxfb_ioctl,   /* optional */
};

    /*
     *  Initialization
     */

int __init gtxfb_init(void)
{
  fb_info.gen.fbhw = &gtx_switch;
  strcpy(fb_info.gen.info.modename, "AViA GTX Framebuffer");
  fb_info.gen.info.node = -1;
  fb_info.gen.info.flags = FBINFO_FLAG_DEFAULT;
  fb_info.gen.info.fbops = &gtxfb_ops;
  fb_info.gen.info.disp = &disp;
  fb_info.gen.info.changevar = NULL;
  fb_info.gen.info.switch_con = &fbgen_switch;
  fb_info.gen.info.updatevar = &fbgen_update_var;
  fb_info.gen.info.blank = &fbgen_blank;
  fb_info.gen.parsize=sizeof(struct gtxfb_par);
  fb_info.gen.fbhw=&gtx_switch;
  fb_info.gen.fbhw->detect();
    
  fbgen_get_var(&disp.var, -1, &fb_info.gen.info);
  disp.var.activate = FB_ACTIVATE_NOW;
  fbgen_do_set_var(&disp.var, 1, &fb_info.gen);
  fbgen_set_disp(-1, &fb_info.gen);
  fbgen_install_cmap(0, &fb_info.gen);
  
  InitGraphics(1);

  if (register_framebuffer(&fb_info.gen.info) < 0) return -EINVAL;

  printk(KERN_INFO "fb%d: %s frame buffer device\n", 
         GET_FB_IDX(fb_info.gen.info.node), fb_info.gen.info.modename);

  return 0;
}

void gtxfb_close(void)
{
  unregister_framebuffer((struct fb_info*)&fb_info);
}

