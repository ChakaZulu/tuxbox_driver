/*
 *   info.c - d-Box Hardware info
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
 *   $Log: info.c,v $
 *   Revision 1.1  2001/03/28 23:33:31  tmbinc
 *   added /proc/bus/info.
 *
 *
 *   $Revision: 1.1 $
 *
 */

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
#include <linux/proc_fs.h>
#include <dbox/info.h>

	/* ich weiss dass dieses programm suckt, aber was soll man machen ... */

int fe=0;
static struct dbox_info_struct info;

#ifdef CONFIG_PROC_FS

static int info_proc_init(void);
static int info_proc_cleanup(void);

static int read_bus_info(char *buf, char **start, off_t offset, int len,
												int *eof , void *private);

#else /* undef CONFIG_PROC_FS */

#define info_proc_init() 0
#define info_proc_cleanup() 0

#endif /* CONFIG_PROC_FS */
                                              

static int dbox_info_init(void)
{
	unsigned char *conf=(unsigned char*)ioremap(0x1001FFE0, 0x20);
	if (!conf)
	{
		printk(KERN_ERR "couldn't remap memory for getting device info.\n");
		return -1;
	}
	memset(info.dsID, 0, 8);		// todo
	info.mID=conf[0];
	info.fe=fe;
	if (info.mID==3)								// das suckt hier, aber ich kenn keinen besseren weg.
	{
		info.feID=0;
		info.fpID=0x52;
		info.enxID=3;
		info.gtxID=-1;
		info.hwREV=fe?0x21:0x41;
		info.fpREV=0x23;
		info.demod=fe?DBOX_DEMOD_VES1993:DBOX_DEMOD_AT76C651;
	}	else if (info.mID==1)				// nokia
	{
		info.feID=fe?0xdd:0x7a;
		info.fpID=0x5a;
		info.enxID=-1;
		info.gtxID=0xB;
		info.fpREV=0x81;
		info.hwREV=0x5;
		info.demod=fe?DBOX_DEMOD_VES1893:DBOX_DEMOD_VES1820;
	}
	iounmap(conf);
	printk(KERN_DEBUG "mID: %02x feID: %02x fpID: %02x enxID: %02x gtxID: %02x hwREV: %02x fpREV: %02x\n",
		info.mID, info.feID, info.fpID, info.enxID, info.gtxID, info.hwREV, info.fpREV);
	info_proc_init();
	return 0;
}

int dbox_get_info(struct dbox_info_struct *dinfo)
{
	memcpy(dinfo, &info, sizeof(info));
	return 0;
}

int dbox_get_info_ptr(struct dbox_info_struct **dinfo)
{
	if (!dinfo)
		return -EFAULT;
	*dinfo=&info;
	return 0;
}

#ifdef CONFIG_PROC_FS

static int read_bus_info(char *buf, char **start, off_t offset, int len,
												int *eof , void *private)
{
	return sprintf(buf, "mID:\t%02x\nfeID:\t%02x\nfpID:\t%02x\nenxID:\t%02x\ngtxID:\t%02x\nhwREV:\t%02x\nfpREV:\t%02x\n",
		info.mID, info.feID, info.fpID, info.enxID, info.gtxID, info.hwREV, info.fpREV);
	return len;
}

int info_proc_init(void)
{
	struct proc_dir_entry *proc_bus_info;

	if (! proc_bus) {
		printk("info.o: /proc/bus/ does not exist");
 		return -ENOENT;
  }

	proc_bus_info = create_proc_entry("dbox", 0, proc_bus);

	if (!proc_bus_info)
	{
		printk("info.o: Could not create /proc/bus/box");
		return -ENOENT;
	}

	proc_bus_info->read_proc = &read_bus_info;
	proc_bus_info->write_proc = 0;
	proc_bus_info->owner = THIS_MODULE;
	return 0;
}

int info_proc_cleanup(void)
{
	remove_proc_entry("dbox", proc_bus);
	return 0;
}
#endif /* def CONFIG_PROC_FS */

#ifdef MODULE
MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("d-Box info");
MODULE_PARM(fe, "i");
EXPORT_SYMBOL(dbox_get_info);
EXPORT_SYMBOL(dbox_get_info_ptr);

int init_module(void)
{
	return dbox_info_init();
}

void cleanup_module(void)
{
	info_proc_cleanup();
	return;
}

EXPORT_SYMBOL(cleanup_module);

#endif
