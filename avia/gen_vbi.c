/*
 *   gen_vbi.c - vbi driver for eNX/GTX (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2001 Florian Schirmer (jolt@tuxbox.org)
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

#include <dbox/avia_gt.h>

#include <ost/demux.h>
#include <dbox/gtx-dmx.h>
#include <dbox/avia_vbi.h>

#ifndef CONFIG_DEVFS_FS
#error no devfs
#endif

static devfs_handle_t devfs_handle;
static int active_vtxt_pid = -1;

#ifdef MODULE
MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("eNX/GTX-VBI driver");
#endif

static dmx_ts_feed_t* feed_vtxt;
static dmx_demux_t *dmx_demux;

static int avia_vbi_stop_vtxt(void)
{

#ifdef ENX
    enx_reg_w(RSTR0) |= (1 << 21);
#endif    
#ifdef GTX
    rh(RR1) |= (1 << 7);
#endif    

    if (active_vtxt_pid >= 0) {

	if (feed_vtxt->stop_filtering(feed_vtxt) < 0) {
  
	    printk("avia_vbi: error while stoping vtxt feed\n");  
	    return -EIO;
	    
	}    
	
	active_vtxt_pid = -1;
  
    }

    return 0;

}

static int avia_vbi_start_vtxt(unsigned long pid)
{

    struct timespec timeout;

    avia_vbi_stop_vtxt();

#ifdef ENX
    enx_reg_w(CFGR0) &= ~(1 << 3);
#endif    
#ifdef GTX
    rh(CR1) &= ~(1 << 3);
#endif    

    if (feed_vtxt->set(feed_vtxt, pid, 188 * 10, 188 * 10, 0, timeout) < 0) {
  
	printk("avia_vbi: error while setting vtxt feed\n");  
	return -EIO;
  
    }

    if (feed_vtxt->start_filtering(feed_vtxt) < 0) {
  
	printk("avia_vbi: error while starting vtxt feed\n");  
	return -EIO;
  
    }
    
    active_vtxt_pid = pid;

#ifdef ENX
    enx_reg_w(RSTR0) &= ~(1 << 21);
    enx_reg_h(TCNTL) |= (1 << 15);
    enx_reg_h(TCNTL) |= (1 << 9);
#endif
#ifdef GTX
    rh(RR1) &= ~(1 << 7);
    rh(TTCR) |= (1 << 14);
    rh(TTCR) |= (1 << 9);
#endif
    
    return 0;

}

static loff_t avia_vbi_llseek(struct file *file, loff_t offset, int origin)
{

    return -ESPIPE;
  
}

static int avia_vbi_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{

    switch (cmd) {
    
	case AVIA_VBI_START_VTXT:
	
	    printk("avia_vbi: start_vtxt (pid = 0x%X)\n", (int)arg);
	    avia_vbi_start_vtxt(arg);
	    
	break;
	
	case AVIA_VBI_STOP_VTXT:
	
	    printk("avia_vbi: stop_vtxt (pid = 0x%X)\n", active_vtxt_pid);
	    avia_vbi_stop_vtxt();
	    
	break;
	
	default:
	
	    printk("avia_vbi: unknown ioctl\n");
	    
	break;
	
    }

    return 0;
	
}

static int avia_vbi_open(struct inode *inode, struct file *file)
{

    MOD_INC_USE_COUNT;
	
    return 0;
	
}

static int avia_vbi_release(struct inode *inode, struct file *file)
{

    MOD_DEC_USE_COUNT;
	
    return 0;
	
}

static struct file_operations avia_vbi_fops = {

    owner:	THIS_MODULE,
    llseek:	avia_vbi_llseek,
    ioctl:	avia_vbi_ioctl,
    open:	avia_vbi_open,
    release:	avia_vbi_release,
	
};

int dmx_ts_callback(__u8* buffer1, size_t buffer1_length, __u8* buffer2, size_t buffer2_length, dmx_ts_feed_t *source, dmx_success_t success)
{

    return 0;

}

static int __init init_vbi(void)
{

    struct list_head *dmx_list;

    printk(KERN_INFO "avia_vbi: version (" __DATE__ "-" __TIME__ ")\n");

    devfs_handle = devfs_register(NULL, "dbox/vbi0", DEVFS_FL_DEFAULT, 0, 0, S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, &avia_vbi_fops, NULL);

    if (!devfs_handle) {
  
        printk("avia_vbi: error - can't register devfs handle\n");  
	return -EIO;
    
    }

    dmx_list = dmx_get_demuxes();
  
    if (!dmx_list) {

        printk("avia_vbi: error - no demux found\n");  
	return -EIO;
  
    }
  
    dmx_demux = DMX_DIR_ENTRY(dmx_list->next);
    printk("avia_vbi: got demux %s - %s\n", dmx_demux->vendor, dmx_demux->model);
  
    if (dmx_demux->allocate_ts_feed(dmx_demux, &feed_vtxt, dmx_ts_callback, TS_PACKET, DMX_TS_PES_TELETEXT) < 0) {
  
	printk("avia_vbi: error while allocating vtxt feed\n");  
	return -EIO;
  
    }

    return 0;

}

static void __exit cleanup_vbi(void)
{

    printk(KERN_INFO "avia_vbi: unloading\n");

    avia_vbi_stop_vtxt();
    dmx_demux->release_ts_feed(dmx_demux, feed_vtxt);

    devfs_unregister (devfs_handle);

}

module_init(init_vbi);
module_exit(cleanup_vbi);
