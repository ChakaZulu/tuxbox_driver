/*
 * tuxbox_hardware_dbox2.c - TuxBox hardware info - dbox2
 *
 * Copyright (C) 2003 Florian Schirmer <jolt@tuxbox.org>
 *                    Bastian Blank <waldi@tuxbox.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Id: tuxbox_hardware_dbox2.c,v 1.7 2005/12/03 23:00:57 carjay Exp $
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/config.h>
#ifdef CONFIG_MTD
#include <linux/mtd/mtd.h>
#endif
#include <asm/io.h>

#include "tuxbox_internal.h"
#include <tuxbox/hardware_dbox2.h>
#include <tuxbox/info_dbox2.h>

extern struct proc_dir_entry *proc_bus_tuxbox;
struct proc_dir_entry *proc_bus_tuxbox_dbox2 = NULL;

tuxbox_dbox2_gt_t tuxbox_dbox2_gt;
tuxbox_dbox2_mid_t tuxbox_dbox2_mid;


/* 
	If this is ever going to be a kernel built-in we 
	could make sure this was called before MTD so no
	need to	create an unnecessary dependency.
 */

#if defined(MODULE) && defined(CONFIG_MTD)
static struct mtd_info *bmon_part_info;
static char *bmonname = "BR bootloader";

static void mtd_add(struct mtd_info *info)
{
	if (!strncmp(bmonname,info->name,strlen(bmonname))) {
		/* we have a match, store this data */
		bmon_part_info = info;
	}
}

static void mtd_remove(struct mtd_info *info)
{ /* unused, but cannot be NULL */
}

static struct mtd_notifier mtd = {
	.add = mtd_add,
	.remove = mtd_remove
};

static int flash_read_mid(void)
{
	/* 
		If we let MTD manage the flash we cannot simply access
		it (because MTD might have put it in a state that is not
		giving any usable output).
		
		So we need to register ourselves as a user and properly ask
		for permission to access the bootloader.
	*/

	int ret = 0;
	size_t retlen;
	char mid;

	register_mtd_user(&mtd);
	
	if (!bmon_part_info) {
		printk(KERN_ERR "tuxbox: BMon partition \"%s\" not found\n",bmonname);
		ret = -EIO;
		goto frm_exit;
	}

	if (bmon_part_info->size!=0x20000) {
		printk(KERN_ERR "tuxbox: BMon partition has unexpected size %d\n",bmon_part_info->size);
		ret = -EIO;
		goto frm_exit;
	}

	ret = bmon_part_info->read(bmon_part_info, 0x1FFE0, 1, &retlen, &mid);
	if (ret) {
		printk(KERN_ERR "tuxbox: BMon partition read error\n");
		goto frm_exit;
	}
	
	if (!retlen) {
		printk(KERN_ERR "tuxbox: BMon partition read returned 0 bytes\n");
		ret = -EIO;
		goto frm_exit;
	}

	tuxbox_dbox2_mid = mid;

frm_exit:
	unregister_mtd_user(&mtd);
	return ret;
}

#else /* CONFIG_MTD */

static int flash_read_mid(void)
{
	unsigned char *conf = (unsigned char *) ioremap (0x1001FFE0, 0x20);
	if (!conf) {
		printk(KERN_ERR "tuxbox: Could not remap memory\n");
		return -EIO;
	}

	tuxbox_dbox2_mid = conf[0];
	iounmap (conf);
	
	return 0;
}

#endif

static int vendor_read (void)
{
	if (flash_read_mid()<0)
		return -EIO;

	switch (tuxbox_dbox2_mid) {
		case TUXBOX_DBOX2_MID_NOKIA:
			tuxbox_vendor = TUXBOX_VENDOR_NOKIA;
			tuxbox_dbox2_gt = TUXBOX_DBOX2_GT_GTX;
			break;

		case TUXBOX_DBOX2_MID_PHILIPS:
			tuxbox_vendor = TUXBOX_VENDOR_PHILIPS;
			tuxbox_dbox2_gt = TUXBOX_DBOX2_GT_ENX;
			break;

		case TUXBOX_DBOX2_MID_SAGEM:
			tuxbox_vendor = TUXBOX_VENDOR_SAGEM;
			tuxbox_dbox2_gt = TUXBOX_DBOX2_GT_ENX;
			break;
	}

	return 0;
}


int tuxbox_hardware_read (void)
{
	int ret;

	tuxbox_model = TUXBOX_MODEL_DBOX2;
	tuxbox_submodel = TUXBOX_SUBMODEL_DBOX2;

	if ((ret = vendor_read ()))
		return ret;

	tuxbox_capabilities = TUXBOX_HARDWARE_DBOX2_CAPABILITIES;

	return 0;
}

int tuxbox_hardware_proc_create (void)
{
	if (!(proc_bus_tuxbox_dbox2 = proc_mkdir ("dbox2", proc_bus_tuxbox)))
		goto error;

	if (tuxbox_proc_create_entry ("gt", 0444, proc_bus_tuxbox_dbox2, &tuxbox_dbox2_gt, &tuxbox_proc_read, NULL))
		goto error;

	if (tuxbox_proc_create_entry ("mid", 0444, proc_bus_tuxbox_dbox2, &tuxbox_dbox2_mid, &tuxbox_proc_read, NULL))
		goto error;

	return 0;

error:
	printk("tuxbox: Could not create /proc/bus/tuxbox/dbox2\n");
	return -ENOENT;
}

void tuxbox_hardware_proc_destroy (void)
{
	remove_proc_entry ("gt", proc_bus_tuxbox_dbox2);
	remove_proc_entry ("mid", proc_bus_tuxbox_dbox2);

	remove_proc_entry ("dbox2", proc_bus_tuxbox);
}

