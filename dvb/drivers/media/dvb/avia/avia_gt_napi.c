/*
 * $Id: avia_gt_napi.c,v 1.169 2003/01/09 01:20:27 obi Exp $
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
static int hw_dmx_sec = 1;
static int hw_sections = 0;

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
		
	avia_gt_dmx_queue_start(queue->index, AVIA_GT_DMX_QUEUE_MODE_TS, dvbdmxfeed->pid, 0);

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
		
	avia_gt_dmx_queue_start(queue->index, AVIA_GT_DMX_QUEUE_MODE_TS, dvbdmxfeed->pid, 0);

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
		
	avia_gt_dmx_queue_start(queue->index, AVIA_GT_DMX_QUEUE_MODE_PES, dvbdmxfeed->pid, 0);
	
	return 0;

}

static int avia_gt_napi_start_feed_section(struct dvb_demux_feed *dvbdmxfeed)
{

	if ((!hw_dmx_sec) || (!hw_sections))
		return avia_gt_napi_start_feed_generic(dvbdmxfeed);

	return -EINVAL;

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

	printk("avia_gt_napi: $Id: avia_gt_napi.c,v 1.169 2003/01/09 01:20:27 obi Exp $\n");

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

#ifdef MODULE
MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("Avia eNX/GTX demux driver");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#endif

