/*
 * $Id: avia_gt_dvr.c,v 1.15 2003/04/12 06:03:32 obi Exp $
 *
 * (C) 2000-2001 Ronny "3des" Strutz <3des@tuxbox.org>
 * (C) 2002 Florian Schirmer <jolt@tuxbox.org>
 * (C) 2003 Andreas Oberritter <obi@tuxbox.org>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>

#include "avia_av.h"
#include "avia_gt.h"
#include "avia_gt_dmx.h"

static sAviaGtInfo *gt_info;

DECLARE_WAIT_QUEUE_HEAD(avia_gt_dvr_queue_wait);

void avia_gt_dvr_enable(void)
{
	/* disable framer */
	if (avia_gt_chip(ENX))
		enx_reg_set(FC, FE, 0);
	else if (avia_gt_chip(GTX))
		gtx_reg_set(FCR, FE, 0);

	/* enable clip mode for video queue */
	if (avia_gt_chip(ENX))
		enx_reg_set(CFGR0, VCP, 1);
	else if (avia_gt_chip(GTX))
		gtx_reg_set(CR1, VCP, 1);
}

void avia_gt_dvr_disable(void)
{
	/* disable clip mode on video queue */
	if (avia_gt_chip(ENX))
		enx_reg_set(CFGR0, VCP, 0);
	else if (avia_gt_chip(GTX))
		gtx_reg_set(CR1, VCP, 0);

	/* enable framer */
	if (avia_gt_chip(ENX)) {
		enx_reg_set(FC, FE, 1);
		enx_reg_set(FC, FH, 1);
	}
	else if (avia_gt_chip(GTX)) {
		gtx_reg_set(FCR, FE, 1);
		gtx_reg_set(FCR, FH, 1);
	}
}

ssize_t avia_gt_dvr_write(void *buf, size_t count)
{
	sAviaGtDmxQueue *avi_dmx_queue = avia_gt_dmx_get_queue_info(AVIA_GT_DMX_QUEUE_VIDEO);
	u32 queue_size = avi_dmx_queue->info.size(&avi_dmx_queue->info);
	u32 bytes_free = avi_dmx_queue->info.bytes_free(&avi_dmx_queue->info);

	if (count >= queue_size)
		count = queue_size - 1;

	if (bytes_free < count)
		if (wait_event_interruptible(avia_gt_dvr_queue_wait, avi_dmx_queue->info.bytes_free(&avi_dmx_queue->info) >= count) < 0)
			count = min(count, avi_dmx_queue->info.bytes_free(&avi_dmx_queue->info));

	count = avi_dmx_queue->info.put_data(&avi_dmx_queue->info, buf, count, 1);

	avia_gt_dmx_system_queue_set_write_pos(AVIA_GT_DMX_QUEUE_VIDEO, avi_dmx_queue->write_pos);

	return count;
}

void avia_gt_dvr_queue_irq(struct avia_gt_dmx_queue *queue, void *priv_data)
{
	wake_up_interruptible(&avia_gt_dvr_queue_wait);
}

int __init avia_gt_dvr_module_init(void)
{
	printk(KERN_INFO "$Id: avia_gt_dvr.c,v 1.15 2003/04/12 06:03:32 obi Exp $\n");

	gt_info = avia_gt_get_info();

	if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {
		printk(KERN_ERR "%s: unsupported chip type\n", __FILE__);
		return -EIO;
	}

	return 0;
}

void __exit avia_gt_dvr_module_exit(void)
{
}

module_init(avia_gt_dvr_module_init);
module_exit(avia_gt_dvr_module_exit);

MODULE_AUTHOR("Andreas Oberritter <obi@tuxbox.org>");
MODULE_DESCRIPTION("AViA GTX/eNX TS Playback Driver");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(avia_gt_dvr_enable);
EXPORT_SYMBOL(avia_gt_dvr_disable);
EXPORT_SYMBOL(avia_gt_dvr_write);
EXPORT_SYMBOL(avia_gt_dvr_queue_irq);
