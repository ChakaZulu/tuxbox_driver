#ifndef __GTP_FB_H
#define __GTP_FB_H

extern void gtp_fb_init(void);
extern void gtp_fb_set_param(int pal, int bpp, int lowres, int interlaced, int xres, int yres, int stride);
extern void *gtp_fb_mem_lin(void);
extern void *gtp_fb_mem_phys(void);
extern void gtp_fb_getcolreg(unsigned int regno, unsigned int *red, unsigned int *green, unsigned int *blue, unsigned int *transp);
extern void gtp_fb_setcolreg(int regno, int red, int green, int blue, int transp);

#endif
