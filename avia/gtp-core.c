/*
 *   gtp-core.c - GTP core driver (dbox-II-project)
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
 *   $Log: gtp-core.c,v $
 *   Revision 1.1  2001/04/19 23:32:27  Jolt
 *   Merge Part II
 *
 *   Revision 1.1  2001/04/17 23:56:34  Jolt
 *   - initial release
 *
 *   $Revision: 1.1 $
 *
 */

#define __KERNEL_SYSCALLS__

#include <linux/string.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <linux/fcntl.h>
#include <linux/vmalloc.h>
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

#include "gtp-core.h"

#ifdef MODULE
MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("GTP core driver");

static int debug = 0;
static char *ucode = 0;

MODULE_PARM(debug, "i");

#endif

sGtpDev used_gtp_dev;
int used_gtp_dev_nr = 0;

sGtpFb used_gtp_fb;
int used_gtp_fb_nr = 0;

int gtp_dev_register(sGtpDev *gtp_dev) {

  if (used_gtp_dev_nr) // busy - we can handle just a single gtp - ugly hack :-)
    return 0;
    
  if (!gtp_dev)
    return 0;

  memcpy(&used_gtp_dev, gtp_dev, sizeof(sGtpDev));
  used_gtp_dev_nr = 1;
  
  if (used_gtp_fb_nr > 0)
    used_gtp_fb.attach(gtp_dev);
  
  printk("[gtp-core] device register ok (name = %s)\n", gtp_dev->name);

  return used_gtp_dev_nr;

}

void gtp_dev_release(int gtp_dev_nr) {

  if (gtp_dev_nr != used_gtp_dev_nr)
    return;

  printk("[gtp-core] dev release (gtp_dev_nr = 0x%X)\n", gtp_dev_nr);

  if (used_gtp_fb_nr > 0)
    used_gtp_fb.detach(&used_gtp_dev);

  used_gtp_dev_nr = 0;    
  memset(&used_gtp_dev, 0, sizeof(sGtpDev));

}

int gtp_fb_register(sGtpFb *gtp_fb) {

  if (used_gtp_fb_nr) // busy - we can handle just a single fb - ugly hack :-)
    return 0;
    
  if (!gtp_fb)
    return 0;

  memcpy(&used_gtp_fb, gtp_fb, sizeof(sGtpFb));
  used_gtp_fb_nr = 1;
  
  printk("[gtp-core] fb register ok (name = %s)\n", gtp_fb->name);

  if (used_gtp_dev_nr > 0)
    used_gtp_fb.attach(&used_gtp_dev);

  return used_gtp_fb_nr;

}

void gtp_fb_release(int gtp_fb_nr) {

  if (gtp_fb_nr != used_gtp_fb_nr)
    return;

  printk("[gtp-core] fb release (gtp_fb_nr = 0x%X)\n", gtp_fb_nr);

  used_gtp_fb_nr = 0;    
  memset(&used_gtp_fb, 0, sizeof(sGtpFb));
  
}

EXPORT_SYMBOL(gtp_dev_register);
EXPORT_SYMBOL(gtp_dev_release);
EXPORT_SYMBOL(gtp_fb_register);
EXPORT_SYMBOL(gtp_fb_release);

#ifdef MODULE

int init_module(void) {

  return 0;

}

void cleanup_module(void) {

}

#endif
