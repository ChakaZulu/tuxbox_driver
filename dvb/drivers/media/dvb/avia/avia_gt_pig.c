/*
 *   enx_pig.c - pig driver for AViA eNX (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2001 Florian Schirmer <jolt@tuxbox.org>
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
#include <linux/devfs_fs_kernel.h>

#ifndef CONFIG_DEVFS_FS
#error no devfs
#endif

#include <dbox/enx.h>
#include <dbox/enx_capture.h>
#include <dbox/avia_pig.h>

static int pig_open(struct inode *inode, struct file *file);
static ssize_t pig_read(struct file *file, char *buf, size_t count, loff_t *offset);
static int pig_release(struct inode *inode, struct file *file);
static int pig_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);

static void enable_pig(void);
static void disable_pig(void);


extern void enx_pig_set_pos(unsigned char pig_nr, unsigned short x, unsigned short y);
extern void enx_pig_set_size(unsigned char pig_nr, unsigned short width, unsigned short height, unsigned char stretch);



#define CAPTURE_WIDTH 720
#define CAPTURE_HEIGHT 576

//#define SCALE_HFACTOR 2
//#define SCALE_VFACTOR 2

/*#define SCALE_HFACTOR 4
#define SCALE_VFACTOR 8
#define CAPTURE_WIDTH (160 * SCALE_HFACTOR)
#define CAPTURE_HEIGHT (72 * SCALE_VFACTOR)
*/

//#define PIG_WIDTH (CAPTURE_WIDTH / SCALE_HFACTOR)
//#define PIG_HEIGHT (CAPTURE_HEIGHT / SCALE_VFACTOR)
#define PIG_WIDTH 160
#define PIG_HEIGHT 72

static devfs_handle_t devfs_handle1;
static devfs_handle_t devfs_handle2;

static struct file_operations pig_fops = {
	owner:  	THIS_MODULE,
	read:   	pig_read,
	ioctl:  	pig_ioctl,
	open:   	pig_open,
	release:	pig_release
};

static int pig_open(struct inode *inode, struct file *file)
{
  return 0;
}

static ssize_t pig_read(struct file *file, char *buf, size_t count, loff_t *offset)
{
  return 0;
}

static int pig_release(struct inode *inode, struct file *file)
{
  return 0;
}

static int pig_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    switch(cmd) {
    
	case AVIA_PIG_SET_POS:

	    enx_pig_set_pos(0, (unsigned short)((arg >> 16) & 0xFFFF), (unsigned short)(arg & 0xFFFF));

	break;

	case AVIA_PIG_SET_SIZE:

	    enx_pig_set_size(0, (unsigned short)((arg >> 16) & 0xFFFF), (unsigned short)(arg & 0xFFFF), 0);

	break;
    
    }

    return 0;
}

void enx_pig_set_pos(unsigned char pig_nr, unsigned short x, unsigned short y)
{

#define PIG_PIPEDELAY 8
#define BLANK_TIME 132

    enx_reg_s(VPP1)->HPOS = ((BLANK_TIME - PIG_PIPEDELAY) + x) / 2;
    enx_reg_s(VPP1)->VPOS = 21 + y;

}

void enx_pig_set_size(unsigned char pig_nr, unsigned short width, unsigned short height, unsigned char stretch)
{
    enx_reg_s(VPSZ1)->WIDTH = width / 2;
    enx_reg_s(VPSZ1)->S = stretch;
    enx_reg_s(VPSZ1)->HEIGHT = height / 2;
    
    printk("enx_pig: WIDTH=0x%X, S=0x%X, HEIGHT=0x%X\n", enx_reg_s(VPSZ1)->WIDTH, enx_reg_s(VPSZ1)->S, enx_reg_s(VPSZ1)->HEIGHT);
}

static void disable_pig(void)
{
    printk("enx_pig: disable pig\n");
    enx_reg_w(VPSA1) = 0;
    
    enx_reg_w(RSTR0) |= (1 << 7);

    enx_capture_stop();
}

static void enable_pig(void)
{
    unsigned char *buf_addr;
    unsigned short stride;
    
    printk("enx_pig: enable pig\n");
    
    enx_capture_set_input(0, 0, CAPTURE_WIDTH, CAPTURE_HEIGHT);
    enx_capture_set_output(PIG_WIDTH, PIG_HEIGHT);
    enx_capture_start(&buf_addr, &stride);
    
    printk("enx_pig: buffer=0x%X, stride=0x%X\n", buf_addr, stride);

    enx_reg_w(RSTR0) &= ~(1 << 7);						// Take video pig out of reset

    enx_reg_w(VPOFFS1) = 0;
//    enx_reg_w(VPOFFS1) |= ((SCALED_HSIZE * SCALED_VSIZE / 4) & 0x1FFFFC);

    enx_reg_h(VPSO1) = 2;							// Set stack order

    enx_reg_h(VPSTR1) = 0;				
    enx_reg_h(VPSTR1) |= (((stride / 4) & 0x7FF) << 2);
    enx_reg_h(VPSTR1) |= 0;							// Enable hardware double buffering
    
    enx_reg_s(VPSZ1)->P = 0;
    
    enx_reg_w(VPSA1) = 0;
    enx_reg_w(VPSA1) |= ((unsigned int)buf_addr & 0xFFFFFC);			// Set buffer address (for non d-buffer mode)

    enx_reg_s(VPP1)->U = 0;
    enx_reg_s(VPP1)->F = 0;
        
    enx_pig_set_pos(0, 0, 0);
    enx_pig_set_size(0, PIG_WIDTH, PIG_HEIGHT, 0);
    
    enx_reg_w(VPSA1) |= 1;							// Enable PIG1
}

static int init_enx_pig(void)
{
    printk("$Id: avia_gt_pig.c,v 1.1 2001/10/23 08:49:35 Jolt Exp $\n");

    devfs_handle1 = devfs_register(NULL, "dbox/pig", DEVFS_FL_DEFAULT, 0, 0,	// <-- last 0 is the minor
				    S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
				    &pig_fops, NULL );
    if (!devfs_handle1)
	return -EIO;

    devfs_handle2 = devfs_register(NULL, "dbox/pig1", DEVFS_FL_DEFAULT, 0, 1,	// <-- last 0 is the minor
				    S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
				    &pig_fops, NULL );

    if (!devfs_handle2) {
	devfs_unregister(devfs_handle1);
	return -EIO;
    }

    enable_pig();

    return 0;
    
}

static void __exit cleanup_enx_pig(void)
{
    disable_pig();
    
    devfs_unregister(devfs_handle2);
    devfs_unregister(devfs_handle1);
}

#ifdef MODULE
module_init(init_enx_pig);
module_exit(cleanup_enx_pig);
#endif
