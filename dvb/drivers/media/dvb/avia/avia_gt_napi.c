/*
 * $Id: avia_gt_napi.c,v 1.171 2003/01/11 22:45:16 obi Exp $
 * 
 * AViA GTX demux driver (dbox-II-project)
 *
 * Homepage: http://dbox2.elxsi.de
 *
 * Copyright (C) 2000-2001 Felix "tmbinc" Domke (tmbinc@gmx.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#define __KERNEL_SYSCALLS__

#include <linux/string.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <linux/fcntl.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/tqueue.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>

#include "../dvb-core/compat.h"
#include "../dvb-core/demux.h"
#include "../dvb-core/dvb_demux.h"
#include "../dvb-core/dvb_frontend.h"
#include "../dvb-core/dmxdev.h"

#include "avia_napi.h"
#include "avia_gt.h"
#include "avia_gt_dmx.h"
#include "avia_gt_napi.h"
#include "avia_gt_accel.h"
#include "avia_av_napi.h"

static sAviaGtInfo *gt_info = (sAviaGtInfo *)NULL;
static struct dvb_adapter *adapter;
static struct dvb_demux demux;
static dmxdev_t dmxdev;
static dmx_frontend_t fe_hw;
static dmx_frontend_t fe_mem;
static int hw_crc = 1;
static int hw_dmx_ts = 1;
static int hw_dmx_pes = 1;
static int hw_dmx_sec = 0;

static u32 avia_gt_napi_crc32(struct dvb_demux_feed *dvbdmxfeed, const u8 *src, size_t len)
{

	if ((dvbdmxfeed->type == DMX_TYPE_SEC) && (dvbdmxfeed->feed.sec.check_crc))
		return dvbdmxfeed->feed.sec.crc_val;
	else
		return (dvbdmxfeed->feed.sec.crc_val = crc32_le(dvbdmxfeed->feed.sec.crc_val, src, len));

}

static void avia_gt_napi_memcpy(struct dvb_demux_feed *dvbdmxfeed, u8 *dst, const u8 *src, size_t len)
{

	if ((dvbdmxfeed->type == DMX_TYPE_SEC) && (dvbdmxfeed->feed.sec.check_crc)) {

		if ((src > gt_info->mem_addr) && (src < (gt_info->mem_addr + 0x200000)))
			dvbdmxfeed->feed.sec.crc_val = avia_gt_accel_crc32(src - gt_info->mem_addr, len, dvbdmxfeed->feed.sec.crc_val);
		else
			dvbdmxfeed->feed.sec.crc_val = crc32_le(dvbdmxfeed->feed.sec.crc_val, src, len);

	}

	memcpy(dst, (void *)src, len);

}

static struct avia_gt_dmx_queue *avia_gt_napi_queue_alloc(struct dvb_demux_feed *dvbdmxfeed, void (*cb_proc)(struct avia_gt_dmx_queue *, void *))
{

	if (dvbdmxfeed->type == DMX_TYPE_SEC)
		return avia_gt_dmx_alloc_queue_user(NULL, cb_proc, dvbdmxfeed);

	if (dvbdmxfeed->type != DMX_TYPE_TS) {

		printk("avia_gt_napi: strange feed type %d found\n", dvbdmxfeed->type);

		return NULL;

	}

	switch (dvbdmxfeed->pes_type) {

		case DMX_TS_PES_VIDEO:

			return avia_gt_dmx_alloc_queue_video(NULL, cb_proc, dvbdmxfeed);

		case DMX_TS_PES_AUDIO:

			return avia_gt_dmx_alloc_queue_audio(NULL, cb_proc, dvbdmxfeed);

		case DMX_TS_PES_TELETEXT:

			return avia_gt_dmx_alloc_queue_teletext(NULL, cb_proc, dvbdmxfeed);

		case DMX_TS_PES_PCR:
		case DMX_TS_PES_SUBTITLE:
		case DMX_TS_PES_OTHER:

			return avia_gt_dmx_alloc_queue_user(NULL, cb_proc, dvbdmxfeed);

		default:

			return NULL;

	}

}

/*
 * Callback for hardware section filtering
 */


static void avia_gt_napi_queue_callback_section(struct avia_gt_dmx_queue *queue, void *data)
{
	struct dvb_demux_feed *dvbdmxfeed = (struct dvb_demux_feed *) data;
	struct dvb_demux_filter *dvbdmxfilter;
	u32 bytes_avail;
	u32 section_length;
	u32 chunk1;
	u8 neq;
	u8 xor;
	u32 i;
	u32 compare_len;
	u8 copied;
	u32 crc;
	u8 section_header[4096];

	printk("Callback called.\n");

	bytes_avail = queue->bytes_avail(queue);

	while (bytes_avail >= 3)
	{

		/*
		 * Remove padding bytes.
		 */

		while (bytes_avail && (queue->get_data8(queue,1) == 0xFF))
		{
			bytes_avail--;
			queue->get_data8(queue,0);
		}

		/*
		 * We need the length bytes
		 */

		if (bytes_avail < 3)
		{
			break;
		}

		queue->get_data(queue,section_header,3,1);

		section_length = (((section_header[1] & 0x0F) << 8) | section_header[2]) + 3;

		/*
		 * Sections > 4096 bytes don't exist. We are out of sync. Recover by restarting the feed.
		 */

		if (section_length > 4096)
		{
			printk(KERN_ERR "avia_gt_napi: section_length %d > 4096.\n",section_length);
			avia_gt_dmx_set_pid_table(queue->index,1,1,dvbdmxfeed->pid);
			avia_gt_dmx_queue_reset(queue->index);
			avia_gt_dmx_set_pid_table(queue->index,1,0,dvbdmxfeed->pid);
			break;
		}

		/*
		 * Do we have the complete section?
		 */

		if (section_length > bytes_avail)
		{
			break;
		}

		/*
		 * Determine who is interested in the section.
		 */

		compare_len = (section_length < DVB_DEMUX_MASK_MAX) ? section_length : DVB_DEMUX_MASK_MAX;
		dvbdmxfilter = dvbdmxfeed->filter;
		crc = queue->crc32(queue,section_length,0);
		chunk1 = queue->get_buf1_size(queue);

		/*
		 * If we have more than one potential client, copy section here to avoid reading dmx-memory
		 * multiple times.
		 */

		if ( (dvbdmxfilter->next) || (compare_len == section_length) )
		{
			queue->get_data(queue,section_header,section_length,0);
			copied = 1;
		}
		else
		{
			queue->get_data(queue,section_header,compare_len,1);
			copied = 0;
		}

		while (dvbdmxfilter)
		{
			if ( (dvbdmxfilter->feed->feed.sec.check_crc) && crc)
			{
				goto next_client;
			}

			/*
			 * Check wether filter matches.
			 */

			neq = 0;
			for (i = 0; i < compare_len; i++)
			{
				xor = dvbdmxfilter->filter.filter_value[i] ^ section_header[i];
				if (dvbdmxfilter->maskandmode[i] & xor)
				{
					goto next_client;
				}
				neq |= dvbdmxfilter->maskandnotmode[i] & xor;
			}
			if (dvbdmxfilter->doneq && !neq)
			{
				goto next_client;
			}

			/*
			 * Call section callback
			 */

			if (copied)
			{
				dvbdmxfilter->feed->cb.sec(section_header,section_length,NULL,0,&dvbdmxfilter->filter,DMX_OK);
			}
			else if (section_length <= chunk1)
			{
				dvbdmxfilter->feed->cb.sec(gt_info->mem_addr + queue->get_buf1_ptr(queue),section_length,
											NULL,0,&dvbdmxfilter->filter,DMX_OK);
			}
			else
			{
				dvbdmxfilter->feed->cb.sec(gt_info->mem_addr + queue->get_buf1_ptr(queue),chunk1,
							    			gt_info->mem_addr + queue->get_buf2_ptr(queue),section_length - chunk1,
											&dvbdmxfilter->filter,DMX_OK);
			}

			/*
			 * Next section filter.
			 */

next_client:
			dvbdmxfilter = dvbdmxfilter->next;
		}

		/*
		 * Remove handled section from queue.
		 */

		if (!copied)
		{
			queue->get_data(queue,NULL,section_length,0);
		}
		bytes_avail -= section_length;
	}
}

static void avia_gt_napi_queue_callback_generic(struct avia_gt_dmx_queue *queue, void *data)
{

	struct dvb_demux_feed *dvbdmxfeed = (struct dvb_demux_feed *)data;
	u32 bytes_avail;
	u32 chunk1;
	u8 ts_buf[188];

	if ((bytes_avail = queue->bytes_avail(queue)) < 188)
		return;

	if (queue->get_data8(queue, 1) != 0x47) {

		printk("avia_gt_napi: lost sync on queue %d\n", queue->index);

		queue->get_data(queue, NULL, bytes_avail, 0);

		return;

	}

	// Align size on ts paket size
	bytes_avail -= bytes_avail % 188;

	// Does the buffer wrap around?
	if (bytes_avail > queue->get_buf1_size(queue)) {

		chunk1 = queue->get_buf1_size(queue);
		chunk1 -= chunk1 % 188;

		// Do we have at least one complete packet before buffer wraps?
		if (chunk1) {

			dvb_dmx_swfilter_packets(dvbdmxfeed->demux, gt_info->mem_addr + queue->get_buf1_ptr(queue), chunk1 / 188);
			queue->get_data(queue, NULL, chunk1, 0);
			bytes_avail -= chunk1;

		}

		// Handle the wrapped packet
		queue->get_data(queue, ts_buf, 188, 0);
		dvb_dmx_swfilter_packet(dvbdmxfeed->demux, ts_buf);
		bytes_avail -= 188;

	}

	// Remaining packets after the buffer has wrapped
	if (bytes_avail) {

		dvb_dmx_swfilter_packets(dvbdmxfeed->demux, gt_info->mem_addr + queue->get_buf1_ptr(queue), bytes_avail / 188);
		queue->get_data(queue, NULL, bytes_avail, 0);

	}

	return;

}

static void avia_gt_napi_queue_callback_ts_pes(struct avia_gt_dmx_queue *queue, void *data)
{

	struct dvb_demux_feed *dvbdmxfeed = (struct dvb_demux_feed *)data;

	dvbdmxfeed->cb.ts(gt_info->mem_addr + queue->get_buf1_ptr(queue),
					  queue->get_buf1_size(queue),
					  gt_info->mem_addr + queue->get_buf2_ptr(queue),
					  queue->get_buf2_size(queue), &dvbdmxfeed->feed.ts, DMX_OK);

	queue->flush(queue);

}

static int avia_gt_napi_start_feed_generic(struct dvb_demux_feed *dvbdmxfeed)
{

	struct avia_gt_dmx_queue *queue = avia_gt_napi_queue_alloc(dvbdmxfeed, avia_gt_napi_queue_callback_generic);

	if (!queue)
		return -EBUSY;

	dvbdmxfeed->priv = queue;

	if ((dvbdmxfeed->type != DMX_TYPE_SEC) && (dvbdmxfeed->ts_type & TS_DECODER) &&
		((dvbdmxfeed->pes_type == DMX_TS_PES_AUDIO) || (dvbdmxfeed->pes_type == DMX_TS_PES_VIDEO)))
		avia_av_napi_decoder_start(dvbdmxfeed);

	avia_gt_dmx_queue_start(queue->index, AVIA_GT_DMX_QUEUE_MODE_TS, dvbdmxfeed->pid, 0, 0, 0);

	return 0;

}

static int avia_gt_napi_start_feed_ts(struct dvb_demux_feed *dvbdmxfeed)
{

	struct avia_gt_dmx_queue *queue;

	if (!hw_dmx_ts)
		return avia_gt_napi_start_feed_generic(dvbdmxfeed);

	if (!(queue = avia_gt_napi_queue_alloc(dvbdmxfeed, avia_gt_napi_queue_callback_ts_pes)))
		return -EBUSY;

	dvbdmxfeed->priv = queue;

	if ((dvbdmxfeed->type != DMX_TYPE_SEC) && (dvbdmxfeed->ts_type & TS_DECODER) &&
		((dvbdmxfeed->pes_type == DMX_TS_PES_AUDIO) || (dvbdmxfeed->pes_type == DMX_TS_PES_VIDEO)))
		avia_av_napi_decoder_start(dvbdmxfeed);

	avia_gt_dmx_queue_start(queue->index, AVIA_GT_DMX_QUEUE_MODE_TS, dvbdmxfeed->pid, 0, 0, 0);

	return 0;

}

static int avia_gt_napi_start_feed_pes(struct dvb_demux_feed *dvbdmxfeed)
{

	struct avia_gt_dmx_queue *queue;

	if (!hw_dmx_pes)
		return avia_gt_napi_start_feed_generic(dvbdmxfeed);

	if (!(queue = avia_gt_napi_queue_alloc(dvbdmxfeed, avia_gt_napi_queue_callback_ts_pes)))
		return -EBUSY;

	dvbdmxfeed->priv = queue;

	if ((dvbdmxfeed->type != DMX_TYPE_SEC) && (dvbdmxfeed->ts_type & TS_DECODER) &&
		((dvbdmxfeed->pes_type == DMX_TS_PES_AUDIO) || (dvbdmxfeed->pes_type == DMX_TS_PES_VIDEO)))
		avia_av_napi_decoder_start(dvbdmxfeed);

	avia_gt_dmx_queue_start(queue->index, AVIA_GT_DMX_QUEUE_MODE_PES, dvbdmxfeed->pid, 0, 0, 0);

	return 0;

}

static int avia_gt_napi_start_feed_section(struct dvb_demux_feed *dvbdmxfeed)
{
	int filter_index;
	struct avia_gt_dmx_queue *queue;

	if (!hw_dmx_sec)
		return avia_gt_napi_start_feed_generic(dvbdmxfeed);

	/*
	 * Try to allocate hardware section filters.
	 */

	filter_index = avia_gt_dmx_alloc_section_filter(dvbdmxfeed->filter);
	
	if (filter_index < 0)
	{
		return avia_gt_napi_start_feed_generic(dvbdmxfeed);
	}

	/*
	 * Get a queue.
	 */

	if (!(queue = avia_gt_napi_queue_alloc(dvbdmxfeed, avia_gt_napi_queue_callback_section)))
	{
		avia_gt_dmx_free_section_filter(filter_index & 255);
		return -EBUSY;
	}

	dvbdmxfeed->priv = queue;
	queue->hw_sec_index = filter_index & 255;

	avia_gt_dmx_queue_start(queue->index, AVIA_GT_DMX_QUEUE_MODE_SEC8,dvbdmxfeed->pid, 1, filter_index & 255, filter_index >> 8);

	return 0;

}

static int avia_gt_napi_write_to_decoder(struct dvb_demux_feed *dvbdmxfeed, const u8 *buf, size_t count)
{

	printk("avia_gt_napi: write_to_decoder\n");

	return 0;

}

static int avia_gt_napi_start_feed(struct dvb_demux_feed *dvbdmxfeed)
{

	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;
	int result;

	if (!dvbdmx->dmx.frontend)
		return -EINVAL;

	if ((dvbdmxfeed->type != DMX_TYPE_SEC) && (dvbdmxfeed->pes_type == DMX_TS_PES_PCR)) {

		avia_gt_dmx_set_pcr_pid(1, dvbdmxfeed->pid);

		if (!(dvbdmxfeed->ts_type & TS_PACKET))
			return 0;

	}

	switch(dvbdmxfeed->type) {

		case DMX_TYPE_TS:
		case DMX_TYPE_PES:

			if (dvbdmxfeed->ts_type & TS_PAYLOAD_ONLY)
				result = avia_gt_napi_start_feed_pes(dvbdmxfeed);
			else
				result = avia_gt_napi_start_feed_ts(dvbdmxfeed);

			break;

		case DMX_TYPE_SEC:

			result = avia_gt_napi_start_feed_section(dvbdmxfeed);

			break;

		default:

			printk("avia_gt_napi: unknown feed type %d found\n", dvbdmxfeed->type);

			return -EINVAL;

	}

	if (!result) {

		if ((dvbdmxfeed->type == DMX_TYPE_SEC) || (dvbdmxfeed->ts_type & TS_PACKET))
			avia_gt_dmx_queue_irq_enable(((struct avia_gt_dmx_queue *)(dvbdmxfeed->priv))->index);

	}

	return result;

}

static int avia_gt_napi_stop_feed(struct dvb_demux_feed *dvbdmxfeed)
{

	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;
	struct avia_gt_dmx_queue *queue = (struct avia_gt_dmx_queue *)dvbdmxfeed->priv;

	if (!dvbdmx->dmx.frontend)
		return -EINVAL;

	if ((dvbdmxfeed->type != DMX_TYPE_SEC) && (dvbdmxfeed->pes_type == DMX_TS_PES_PCR)) {

		avia_gt_dmx_set_pcr_pid(0, 0x0000);

		if (!(dvbdmxfeed->ts_type & TS_PACKET))
			return 0;

	}

	if ((dvbdmxfeed->type != DMX_TYPE_SEC) && (dvbdmxfeed->ts_type & TS_DECODER) &&
		((dvbdmxfeed->pes_type == DMX_TS_PES_AUDIO) || (dvbdmxfeed->pes_type == DMX_TS_PES_VIDEO)))
		avia_av_napi_decoder_stop(dvbdmxfeed);

	avia_gt_dmx_queue_stop(queue->index);

	if (queue->hw_sec_index >= 0)
	{
		avia_gt_dmx_free_section_filter(queue->hw_sec_index);
	}

	avia_gt_dmx_free_queue(queue->index);

	return 0;

}

static void avia_gt_napi_before_after_tune(fe_status_t fe_status, void *data)
{

	if ((fe_status) & FE_HAS_LOCK) {

		if (avia_gt_chip(ENX)) {
		
			enx_reg_set(FC, FE, 1);
			enx_reg_set(FC, FH, 1);

		} else if (avia_gt_chip(GTX)) {
		
			//FIXME
			
		}
	
	} else {

		if (avia_gt_chip(ENX)) {
		
			enx_reg_set(FC, FE, 0);
	
		} else if (avia_gt_chip(GTX)) {
		
			//FIXME
			
		}
	
	}

}

int __init avia_gt_napi_init(void)
{

	int result;

	printk("avia_gt_napi: $Id: avia_gt_napi.c,v 1.171 2003/01/11 22:45:16 obi Exp $\n");

	gt_info = avia_gt_get_info();

	if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {

		printk("avia_gt_napi: Unsupported chip type\n");

		return -EIO;

    }

	adapter = avia_napi_get_adapter();

	if (!adapter)
		return -EINVAL;

	memset(&demux, 0, sizeof(demux));

	demux.dmx.vendor = "C-Cube";
	demux.dmx.model = "AViA GTX/eNX";
	demux.dmx.id = "demux0_0";
	demux.dmx.capabilities = DMX_TS_FILTERING | DMX_SECTION_FILTERING;

	demux.priv = NULL;
	demux.filternum = 31;
	demux.feednum = 31;
	demux.start_feed = avia_gt_napi_start_feed;
	demux.stop_feed = avia_gt_napi_stop_feed;
	demux.write_to_decoder = avia_gt_napi_write_to_decoder;

	if (hw_crc) {

		demux.check_crc32 = avia_gt_napi_crc32;
		demux.memcopy = avia_gt_napi_memcpy;

	}
	
	if ((hw_dmx_sec = avia_gt_dmx_get_hw_sec_filt_avail()))
	{
		printk(KERN_INFO "avia_gt_napi: hw section filtering enabled.\n");
	}

	if (dvb_dmx_init(&demux)) {

		printk("avia_gt_napi: dvb_dmx_init failed\n");

		return -EFAULT;

	}

	dmxdev.filternum = 31;
	dmxdev.demux = &demux.dmx;
	dmxdev.capabilities = 0;

	if ((result = dvb_dmxdev_init(&dmxdev, adapter)) < 0) {

		printk("avia_gt_napi: dvb_dmxdev_init failed (errno = %d)\n", result);

		dvb_dmx_release(&demux);

		return result;

	}

	fe_hw.id = "hw_frontend";
	fe_hw.vendor = "Dummy Vendor";
	fe_hw.model = "hw";
	fe_hw.source = DMX_FRONTEND_0;

	if ((result = demux.dmx.add_frontend(&demux.dmx, &fe_hw)) < 0) {

		printk("avia_napi: add_frontend (hw) failed (errno = %d)\n", result);

		dvb_dmxdev_release(&dmxdev);
		dvb_dmx_release(&demux);

		return result;
		
	}
	
	fe_mem.id = "mem_frontend";
	fe_mem.vendor = "memory";
	fe_mem.model = "sw";
	fe_mem.source = DMX_MEMORY_FE;

	if ((result = demux.dmx.add_frontend(&demux.dmx, &fe_mem)) < 0) {
	
		printk("avia_napi: add_frontend (mem) failed (errno = %d)\n", result);

		demux.dmx.remove_frontend(&demux.dmx, &fe_hw);
		dvb_dmxdev_release(&dmxdev);
		dvb_dmx_release(&demux);
		
		return result;
		
	}

	if ((result = demux.dmx.connect_frontend(&demux.dmx, &fe_hw)) < 0) {
	
		printk("avia_napi: connect_frontend failed (errno = %d)\n", result);

		demux.dmx.remove_frontend(&demux.dmx, &fe_mem);
		demux.dmx.remove_frontend(&demux.dmx, &fe_hw);
		dvb_dmxdev_release(&dmxdev);
		dvb_dmx_release(&demux);
		
		return result;
		
	}

	if ((result = dvb_add_frontend_notifier(adapter, avia_gt_napi_before_after_tune, NULL)) < 0) {
	
		printk("avia_gt_napi: dvb_add_frontend_notifier failed (errno = %d)\n", result);

		demux.dmx.close(&demux.dmx);
		demux.dmx.disconnect_frontend(&demux.dmx);
		demux.dmx.remove_frontend(&demux.dmx, &fe_mem);
		demux.dmx.remove_frontend(&demux.dmx, &fe_hw);
		dvb_dmxdev_release(&dmxdev);
		dvb_dmx_release(&demux);
		
		return result;
		
	}

	return 0;

}

void __exit avia_gt_napi_exit(void)
{

	dvb_remove_frontend_notifier(adapter, avia_gt_napi_before_after_tune);
	demux.dmx.close(&demux.dmx);
	demux.dmx.disconnect_frontend(&demux.dmx);
	demux.dmx.remove_frontend(&demux.dmx, &fe_mem);
	demux.dmx.remove_frontend(&demux.dmx, &fe_hw);
	dvb_dmxdev_release(&dmxdev);
	dvb_dmx_release(&demux);

}

module_init(avia_gt_napi_init);
module_exit(avia_gt_napi_exit);

MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("Avia eNX/GTX demux driver");
MODULE_LICENSE("GPL");

