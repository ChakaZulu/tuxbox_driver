/*
 * $Id: avia_gt_napi.c,v 1.168 2003/01/02 05:26:43 obi Exp $
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

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
static
u32 crc32_table[256] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
	0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
	0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
	0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
	0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
	0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
	0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
	0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
	0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
	0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
	0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
	0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
	0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
	0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
	0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
	0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
	0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
	0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
	0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
	0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
	0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
	0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4};

static u32 crc32_le (u32 crc, unsigned char const *data, size_t len)
{
	int i;

	for (i=0; i<len; i++)
                crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ *data++) & 0xff];

	return crc;
}
#else
#include <linux/crc32.h>
#endif

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

	printk("avia_gt_napi: $Id: avia_gt_napi.c,v 1.168 2003/01/02 05:26:43 obi Exp $\n");

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

