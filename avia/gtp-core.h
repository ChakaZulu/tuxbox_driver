#ifndef __GTP_CORE_H
#define __GTP_CORE_H

typedef struct {

  char *name;
  
  void *fb_mem_lin;
  void *fb_mem_phys;
  unsigned int fb_mem_size;

  void (*fb_clut_get)(unsigned int regno, unsigned int *red, unsigned int *green, unsigned int *blue, unsigned int *transp);
  void (*fb_clut_set)(unsigned int regno, unsigned int red, unsigned int green, unsigned int blue, unsigned int transp);
  void (*fb_param_set)(unsigned int pal, unsigned int bpp, unsigned int lowres, unsigned int interlaced, unsigned int xres, unsigned int yres, unsigned int stride);

} sGtpDev;

typedef struct {

  char *name;
  
  int (*attach)(sGtpDev *gtp_dev);
  void (*detach)(sGtpDev *gtp_dev);

} sGtpFb;

extern int gtp_dev_register(sGtpDev *gtp_dev);
extern void gtp_dev_release(int gtp_dev_nr);
extern int gtp_fb_register(sGtpFb *gtp_fb);
extern void gtp_fb_release(int gtp_fb_nr);

#endif
