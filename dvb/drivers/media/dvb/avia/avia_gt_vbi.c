/*
 * $Id: avia_gt_vbi.c,v 1.26 2003/08/01 17:31:22 obi Exp $
 *
 * vbi driver for AViA eNX/GTX (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2001-2002 Florian Schirmer (jolt@tuxbox.org)
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "avia_gt.h"
#include "avia_gt_vbi.h"

//#define VBI_IRQ

static sAviaGtInfo *gt_info;

static
void avia_gt_vbi_reset(int reenable)
{
	avia_gt_reg_set(RSTR0, TTX, 1);

	if (reenable)
		avia_gt_reg_set(RSTR0, TTX, 0);
}

void avia_gt_vbi_start(void)
{
	avia_gt_vbi_reset(1);

	avia_gt_reg_set(TCNTL, GO, 1);
}

void avia_gt_vbi_stop(void)
{
	avia_gt_reg_set(TCNTL, GO, 0);
}

#ifdef VBI_IRQ
static
void avia_gt_vbi_irq(u16 irq)
{
	u8 tt_error = 0;
	u8 tt_pts = 0;

	if (avia_gt_chip(ENX)) {
		tt_error = enx_reg_s(TSTATUS)->E;
		tt_pts = enx_reg_s(TSTATUS)->R;
	}
	else if (avia_gt_chip(GTX)) {
		tt_error = gtx_reg_s(TSTATUS)->E;
		tt_pts = gtx_reg_s(TSTATUS)->R;
	}

	avia_gt_reg_set(TSTATUS, E, 0);
	avia_gt_reg_set(TSTATUS, R, 0);

	if (tt_error) {
		printk(KERN_INFO "avia_gt_vbi: error in TS stream\n");
		avia_gt_vbi_stop();
		avia_gt_vbi_start();
	}

	if (tt_pts)
		printk(KERN_INFO "avia_gt_vbi: got pts\n");
}
#endif

int __init avia_gt_vbi_init(void)
{
	printk(KERN_INFO "avia_gt_vbi: $Id: avia_gt_vbi.c,v 1.26 2003/08/01 17:31:22 obi Exp $\n");

	gt_info = avia_gt_get_info();

	if (!avia_gt_supported_chipset(gt_info))
		return -ENODEV;

#ifdef VBI_IRQ
	if (avia_gt_alloc_irq(gt_info->irq_tt, avia_gt_vbi_irq)) {
		printk("avia_gt_vbi: unable to get vbi interrupt\n");
		return -EIO;
	}
#endif

	avia_gt_reg_set(CFGR0, TCP, 0);

	avia_gt_vbi_reset(0);

	return 0;
}

void __exit avia_gt_vbi_exit(void)
{
	avia_gt_vbi_stop();
	avia_gt_vbi_reset(0);
#ifdef VBI_IRQ
	avia_gt_free_irq(gt_info->irq_tt);
#endif
}

#if defined(STANDALONE)
module_init(avia_gt_vbi_init);
module_exit(avia_gt_vbi_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("AViA eNX/GTX VBI driver");
MODULE_LICENSE("GPL");
#endif

EXPORT_SYMBOL(avia_gt_vbi_start);
EXPORT_SYMBOL(avia_gt_vbi_stop);
