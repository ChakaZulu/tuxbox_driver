/*
 *   avia_gt_vbi.c - vbi driver for AViA eNX/GTX (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2001-2002 Florian Schirmer (jolt@tuxbox.org)
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
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <linux/devfs_fs_kernel.h>

#include <ost/demux.h>

#include <dbox/avia_gt.h>
#include <dbox/avia_gt_dmx.h>
#include <dbox/avia_gt_vbi.h>

#ifndef CONFIG_DEVFS_FS
#error no devfs
#endif

static devfs_handle_t devfs_handle;
static int active_vtxt_pid = -1;
static sAviaGtInfo *gt_info;

#ifdef MODULE
MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("eNX/GTX-VBI driver");
#endif

static dmx_ts_feed_t* feed_vtxt;
static dmx_demux_t *dmx_demux;

static int avia_gt_vbi_stop_vtxt(void)
{

    if (avia_gt_chip(ENX))
	enx_reg_s(TCNTL)->GO = 0;
    else if (avia_gt_chip(GTX))
	rh(TTCR) &= ~(1 << 9);

    if (active_vtxt_pid >= 0) {

	if (feed_vtxt->stop_filtering(feed_vtxt) < 0) {
  
	    printk("avia_gt_vbi: error while stoping vtxt feed\n");  
	    return -EIO;
	    
	}    
	
	active_vtxt_pid = -1;
  
    }

    return 0;

}

static int avia_gt_vbi_start_vtxt(unsigned long pid)
{

    struct timespec timeout;

    avia_gt_vbi_stop_vtxt();

    if (avia_gt_chip(ENX))
	enx_reg_s(TCNTL)->GO = 1;
    else if (avia_gt_chip(GTX))
	rh(TTCR) |= (1 << 9);

    if (feed_vtxt->set(feed_vtxt, pid, 188 * 10, 188 * 10, 0, timeout) < 0) {
  
	printk("avia_gt_vbi: error while setting vtxt feed\n");  
	return -EIO;
  
    }

    if (feed_vtxt->start_filtering(feed_vtxt) < 0) {
  
	printk("avia_gt_vbi: error while starting vtxt feed\n");  
	return -EIO;
  
    }
    
    active_vtxt_pid = pid;

    return 0;

}

static int avia_gt_vbi_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{

    switch (cmd) {
    
	case AVIA_VBI_START_VTXT:
	
	    printk("avia_gt_vbi: start_vtxt (pid = 0x%X)\n", (int)arg);
	    
	    return avia_gt_vbi_start_vtxt(arg);
	    
	break;
	
	case AVIA_VBI_STOP_VTXT:
	
	    printk("avia_gt_vbi: stop_vtxt)\n");
	    
	    return avia_gt_vbi_stop_vtxt();
	    
	break;
	
	default:
	
	    printk("avia_gt_vbi: unknown ioctl\n");
	    
	break;
	
    }

    return 0;
	
}

static struct file_operations avia_gt_vbi_fops = {

    owner:	THIS_MODULE,
    ioctl:	avia_gt_vbi_ioctl,
	
};

int dmx_ts_callback(__u8* buffer1, size_t buffer1_length, __u8* buffer2, size_t buffer2_length, dmx_ts_feed_t *source, dmx_success_t success)
{

    return 0;

}

static int __init avia_gt_vbi_init(void)
{

    struct list_head *dmx_list;

    printk("avia_gt_vbi: $Id: avia_gt_vbi.c,v 1.6 2002/04/22 17:40:01 Jolt Exp $\n");

    gt_info = avia_gt_get_info();
    
    if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {
	
	printk("avia_gt_vbi: Unsupported chip type\n");
		
	return -EIO;
			
    }

    if (avia_gt_chip(ENX)) {

	enx_reg_s(RSTR0)->TTX = 1;
	enx_reg_s(RSTR0)->TTX = 0;
	enx_reg_s(CFGR0)->TCP = 0;
	enx_reg_s(TCNTL)->PE = 1;

    } else if (avia_gt_chip(GTX)) {

	gtx_reg_s(RR1)->TTX = 1;
	gtx_reg_s(RR1)->TTX = 0;
	rh(CR1) &= ~(1 << 3);
	rh(TTCR) |= (1 << 14);
	rh(TTCR) |= (1 << 9);

    }
			        
    devfs_handle = devfs_register(NULL, "dbox/vbi0", DEVFS_FL_DEFAULT, 0, 0, S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, &avia_gt_vbi_fops, NULL);

    if (!devfs_handle) {
  
        printk("avia_gt_vbi: error - can't register devfs handle\n");  
	return -EIO;
    
    }

    dmx_list = dmx_get_demuxes();
  
    if (!dmx_list) {

        printk("avia_gt_vbi: error - no demux found\n");  
	return -EIO;
  
    }
  
    dmx_demux = DMX_DIR_ENTRY(dmx_list->next);
    printk("avia_gt_vbi: got demux %s - %s\n", dmx_demux->vendor, dmx_demux->model);
  
    if (dmx_demux->allocate_ts_feed(dmx_demux, &feed_vtxt, dmx_ts_callback, TS_PACKET, DMX_TS_PES_TELETEXT) < 0) {
  
	printk("avia_gt_vbi: error while allocating vtxt feed\n");  
	
	return -EIO;
  
    }

    return 0;

}

static void __exit avia_gt_vbi_exit(void)
{

    avia_gt_vbi_stop_vtxt();
    
    dmx_demux->release_ts_feed(dmx_demux, feed_vtxt);

    devfs_unregister (devfs_handle);

    if (avia_gt_chip(ENX))
	enx_reg_s(RSTR0)->TTX = 1;
    else if (avia_gt_chip(GTX))
	gtx_reg_s(RR1)->TTX = 1;

}

#ifdef MODULE
module_init(avia_gt_vbi_init);
module_exit(avia_gt_vbi_exit);
#endif
