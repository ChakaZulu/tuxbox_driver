/*
 * $Id: avia_gt_napi.c,v 1.183 2003/07/24 01:59:21 homar Exp $
 * 
 * AViA GTX/eNX demux dvb api driver (dbox-II-project)
 *
 * Homepage: http://dbox2.elxsi.de
 *
 * Copyright (C) 2000-2001 Felix "tmbinc" Domke <tmbinc@gmx.net>
 * Copyright (C) 2003 Andreas Oberritter <obi@tuxbox.org>
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
#include <linux/dvb/ca.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>

#include "../dvb-core/compat.h"
#include "../dvb-core/demux.h"
#include "../dvb-core/dmxdev.h"
#include "../dvb-core/dvb_demux.h"
#include "../dvb-core/dvb_frontend.h"
#include "../dvb-core/dvb_net.h"

#include "avia_av.h"
#include "avia_av_napi.h"
#include "avia_gt.h"
#include "avia_gt_accel.h"
#include "avia_gt_dmx.h"
#include "avia_gt_napi.h"
#include "avia_gt_vbi.h"
#include "avia_napi.h"

static sAviaGtInfo *gt_info = NULL;
static struct dvb_adapter *adapter = NULL;
static struct dvb_device *ca_dev = NULL;
static struct dvb_demux demux;
static struct dvb_net net;
static struct dmxdev dmxdev;
static dmx_frontend_t fe_hw;
static dmx_frontend_t fe_mem;
static int hw_crc = 1;
static int hw_dmx_ts = 1;
static int hw_dmx_pes = 1;
static int hw_dmx_sec = 1;

/* only used for playback */
static int need_audio_pts = 0;

static
u32 avia_gt_napi_crc32(struct dvb_demux_feed *dvbdmxfeed, const u8 *src, size_t len)
{
	if ((dvbdmxfeed->type == DMX_TYPE_SEC) && (dvbdmxfeed->feed.sec.check_crc))
		return dvbdmxfeed->feed.sec.crc_val;
	else
		return (dvbdmxfeed->feed.sec.crc_val = crc32_le(dvbdmxfeed->feed.sec.crc_val, src, len));
}

static
void avia_gt_napi_memcpy(struct dvb_demux_feed *dvbdmxfeed, u8 *dst, const u8 *src, size_t len)
{
	if ((dvbdmxfeed->type == DMX_TYPE_SEC) && (dvbdmxfeed->feed.sec.check_crc)) {

		if ((src > gt_info->mem_addr) && (src < (gt_info->mem_addr + 0x200000)))
			dvbdmxfeed->feed.sec.crc_val = avia_gt_accel_crc32(src - gt_info->mem_addr, len, dvbdmxfeed->feed.sec.crc_val);
		else
			dvbdmxfeed->feed.sec.crc_val = crc32_le(dvbdmxfeed->feed.sec.crc_val, src, len);

	}

	memcpy(dst, (void *)src, len);
}

static
struct avia_gt_dmx_queue *avia_gt_napi_queue_alloc(struct dvb_demux_feed *dvbdmxfeed, void (*cb_proc)(struct avia_gt_dmx_queue *, void *))
{
	if (dvbdmxfeed->type == DMX_TYPE_SEC)
		return avia_gt_dmx_alloc_queue_user(NULL, cb_proc, dvbdmxfeed);

	if (dvbdmxfeed->type != DMX_TYPE_TS) {
		printk(KERN_ERR "avia_gt_napi: strange feed type %d found\n", dvbdmxfeed->type);
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


static
void avia_gt_napi_queue_callback_section(struct avia_gt_dmx_queue *queue, void *data)
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

	bytes_avail = queue->bytes_avail(queue);

	while (bytes_avail >= 3) {

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
		if (section_length > 4096) {
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
			break;

		/*
		 * Determine who is interested in the section.
		 */

		compare_len = (section_length < DVB_DEMUX_MASK_MAX) ? section_length : DVB_DEMUX_MASK_MAX;
		dvbdmxfilter = dvbdmxfeed->filter;
		crc = queue->crc32(queue,section_length, ~0);
		chunk1 = queue->get_buf1_size(queue);

		/*
		 * If we have more than one potential client, copy section here to avoid reading dmx-memory
		 * multiple times.
		 */

		if ((dvbdmxfilter->next) || (compare_len == section_length)) {
			queue->get_data(queue, section_header, section_length, 0);
			copied = 1;
		}
		else {
			queue->get_data(queue, section_header, compare_len, 1);
			copied = 0;
		}

		while (dvbdmxfilter) {
			if ((dvbdmxfilter->feed->feed.sec.check_crc) && (crc)) {
				dprintk(KERN_ERR "avia_gt_napi: crc invalid (0x%08X)\n", crc);
				goto next_client;
			}

			/*
			 * Check wether filter matches.
			 */
			neq = 0;

			for (i = 0; i < compare_len; i++) {
				xor = dvbdmxfilter->filter.filter_value[i] ^ section_header[i];
				if (dvbdmxfilter->maskandmode[i] & xor)
					goto next_client;
				neq |= dvbdmxfilter->maskandnotmode[i] & xor;
			}

			if (dvbdmxfilter->doneq && !neq)
				goto next_client;

			/*
			 * Call section callback
			 */
			if (copied)
				dvbdmxfilter->feed->cb.sec(section_header, section_length, NULL, 0,
						&dvbdmxfilter->filter, DMX_OK);
			else if (section_length <= chunk1)
				dvbdmxfilter->feed->cb.sec(gt_info->mem_addr + queue->get_buf1_ptr(queue),
						section_length, NULL, 0, &dvbdmxfilter->filter,DMX_OK);
			else
				dvbdmxfilter->feed->cb.sec(gt_info->mem_addr + queue->get_buf1_ptr(queue),
						chunk1, gt_info->mem_addr + queue->get_buf2_ptr(queue),
						section_length - chunk1, &dvbdmxfilter->filter, DMX_OK);

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
			queue->get_data(queue, NULL, section_length, 0);

		bytes_avail -= section_length;
	}
}

static
void avia_gt_napi_queue_callback_generic(struct avia_gt_dmx_queue *queue, void *data)
{
	struct dvb_demux_feed *dvbdmxfeed = (struct dvb_demux_feed *)data;
	u32 bytes_avail;
	u32 chunk1;
	u8 ts_buf[188];

	if ((bytes_avail = queue->bytes_avail(queue)) < 188)
		return;

	if (queue->get_data8(queue, 1) != 0x47) {
		printk(KERN_ERR "avia_gt_napi: lost sync on queue %d\n", queue->index);
		queue->get_data(queue, NULL, bytes_avail, 0);
		return;
	}

	/* Align size on ts paket size */
	bytes_avail -= bytes_avail % 188;

	/* Does the buffer wrap around? */
	if (bytes_avail > queue->get_buf1_size(queue)) {
		chunk1 = queue->get_buf1_size(queue);
		chunk1 -= chunk1 % 188;

		/* Do we have at least one complete packet before buffer wraps? */
		if (chunk1) {
			dvb_dmx_swfilter_packets(dvbdmxfeed->demux, gt_info->mem_addr + queue->get_buf1_ptr(queue), chunk1 / 188);
			queue->get_data(queue, NULL, chunk1, 0);
			bytes_avail -= chunk1;
		}

		/* Handle the wrapped packet */
		queue->get_data(queue, ts_buf, 188, 0);
		dvb_dmx_swfilter_packet(dvbdmxfeed->demux, ts_buf);
		bytes_avail -= 188;
	}

	/* Remaining packets after the buffer has wrapped */
	if (bytes_avail) {
		dvb_dmx_swfilter_packets(dvbdmxfeed->demux, gt_info->mem_addr + queue->get_buf1_ptr(queue), bytes_avail / 188);
		queue->get_data(queue, NULL, bytes_avail, 0);
	}
}

static
void avia_gt_napi_queue_callback_ts_pes(struct avia_gt_dmx_queue *queue, void *data)
{
	struct dvb_demux_feed *dvbdmxfeed = data;

	dvbdmxfeed->cb.ts(gt_info->mem_addr + queue->get_buf1_ptr(queue),
			  queue->get_buf1_size(queue),
			  gt_info->mem_addr + queue->get_buf2_ptr(queue),
			  queue->get_buf2_size(queue), &dvbdmxfeed->feed.ts,
			  DMX_OK);

	queue->flush(queue);
}

static
int avia_gt_napi_start_feed_generic(struct dvb_demux_feed *dvbdmxfeed)
{
	struct avia_gt_dmx_queue *queue = avia_gt_napi_queue_alloc(dvbdmxfeed, avia_gt_napi_queue_callback_generic);

	if (!queue)
		return -EBUSY;

	dvbdmxfeed->priv = queue;

	if ((dvbdmxfeed->type != DMX_TYPE_SEC) && (dvbdmxfeed->ts_type & TS_DECODER) &&
		((dvbdmxfeed->pes_type == DMX_TS_PES_AUDIO) || (dvbdmxfeed->pes_type == DMX_TS_PES_VIDEO)))
		avia_av_napi_decoder_start(dvbdmxfeed);

	return avia_gt_dmx_queue_start(queue->index, AVIA_GT_DMX_QUEUE_MODE_TS, dvbdmxfeed->pid, 0, 0, 0);
}

static
int avia_gt_napi_start_feed_ts(struct dvb_demux_feed *dvbdmxfeed)
{
	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;
	struct avia_gt_dmx_queue *queue;

	if (!hw_dmx_ts)
		return avia_gt_napi_start_feed_generic(dvbdmxfeed);

	if (dvbdmx->dmx.frontend->source == DMX_MEMORY_FE)
		queue = avia_gt_napi_queue_alloc(dvbdmxfeed, NULL);
	else
		queue = avia_gt_napi_queue_alloc(dvbdmxfeed, avia_gt_napi_queue_callback_ts_pes);

	if (!queue)
		return -EBUSY;

	dvbdmxfeed->priv = queue;

	if (dvbdmxfeed->ts_type & TS_DECODER)
		switch (dvbdmxfeed->pes_type) {
		case DMX_TS_PES_AUDIO:
		case DMX_TS_PES_VIDEO:
			avia_av_napi_decoder_start(dvbdmxfeed);
			break;
		case DMX_TS_PES_TELETEXT:
			avia_gt_vbi_start();
			break;
		default:
			break;
		}

	return avia_gt_dmx_queue_start(queue->index, AVIA_GT_DMX_QUEUE_MODE_TS, dvbdmxfeed->pid, 0, 0, 0);
}

static
int avia_gt_napi_start_feed_pes(struct dvb_demux_feed *dvbdmxfeed)
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

	return avia_gt_dmx_queue_start(queue->index, AVIA_GT_DMX_QUEUE_MODE_PES, dvbdmxfeed->pid, 0, 0, 0);
}

static
int avia_gt_napi_start_feed_section(struct dvb_demux_feed *dvbdmxfeed)
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
		return avia_gt_napi_start_feed_generic(dvbdmxfeed);

	/*
	 * Get a queue.
	 */
	if (!(queue = avia_gt_napi_queue_alloc(dvbdmxfeed, avia_gt_napi_queue_callback_section))) {
		avia_gt_dmx_free_section_filter(filter_index & 0xff);
		return -EBUSY;
	}

	dvbdmxfeed->priv = queue;
	queue->hw_sec_index = filter_index & 0xff;

	return avia_gt_dmx_queue_start(queue->index, AVIA_GT_DMX_QUEUE_MODE_SEC8, dvbdmxfeed->pid, 1, filter_index & 0xff, filter_index >> 8);
}

static
int avia_gt_napi_write_to_decoder(struct dvb_demux_feed *dvbdmxfeed, const u8 *buf, size_t count)
{
	/*
	 * count is always 188 with current dvb-core,
	 * so there is always exactly one ts packet in buf.
	 */

	struct ts_header *ts = (struct ts_header *) buf;
	int pes_offset;
	int err;

	if (ts->sync_byte != 0x47)
		return -EINVAL;

	if ((dvbdmxfeed->pes_type == DMX_TS_PES_VIDEO) || (dvbdmxfeed->pes_type == DMX_TS_PES_AUDIO)) {
		if ((err = avia_gt_dmx_queue_write(AVIA_GT_DMX_QUEUE_VIDEO, buf, count, 0)) < 0)
			return err;

		if ((!need_audio_pts) || (dvbdmxfeed->pes_type != DMX_TS_PES_AUDIO))
			return 0;

		if (!((ts->payload_unit_start_indicator) && (ts->payload)))
			return 0;

		pes_offset = sizeof(struct ts_header);

		if (ts->adaptation_field)
			pes_offset += buf[4] + 1;

		if (pes_offset > (184 - sizeof(struct pes_header)))
			return 0;

		if (avia_av_audio_pts_to_stc((struct pes_header *) &buf[pes_offset]) == 0)
			need_audio_pts = 0;

		return 0;
	}

	return -EINVAL;
}

static
int avia_gt_napi_start_feed(struct dvb_demux_feed *dvbdmxfeed)
{
	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;
	int result;

	if (!dvbdmx->dmx.frontend)
		return -EINVAL;

	/*
	 * do not put anything but audio and video into demux queues for now during ts
	 * playback. pcr will go through software demux and teletext is not yet done.
	 */
	if ((dvbdmx->dmx.frontend->source == DMX_MEMORY_FE) &&
		(((dvbdmxfeed->pes_type != DMX_TS_PES_AUDIO) &&
		  (dvbdmxfeed->pes_type != DMX_TS_PES_VIDEO)) ||
		 (!(dvbdmxfeed->ts_type & TS_DECODER)) ||
		 (dvbdmxfeed->type == DMX_TYPE_SEC)))
			return 0;

	if ((dvbdmx->dmx.frontend->source == DMX_MEMORY_FE) &&
		(dvbdmxfeed->pes_type == DMX_TS_PES_AUDIO) &&
		(dvbdmxfeed->ts_type & TS_DECODER))
		need_audio_pts = 1;

	if ((dvbdmxfeed->type == DMX_TYPE_TS) || (dvbdmxfeed->type == DMX_TYPE_PES)) {
		if ((dvbdmxfeed->pes_type == DMX_TS_PES_PCR) && (dvbdmxfeed->ts_type & TS_DECODER)) {
			avia_gt_dmx_set_pcr_pid(1, dvbdmxfeed->pid);
			if (!(dvbdmxfeed->ts_type & TS_PACKET))
				return 0;
		}
		if (dvbdmxfeed->ts_type & TS_PAYLOAD_ONLY)
			result = avia_gt_napi_start_feed_pes(dvbdmxfeed);
		else
			result = avia_gt_napi_start_feed_ts(dvbdmxfeed);
	}
	else if (dvbdmxfeed->type == DMX_TYPE_SEC) {
		result = avia_gt_napi_start_feed_section(dvbdmxfeed);
	}
	else {
		printk(KERN_ERR "avia_gt_napi: unknown feed type %d found\n", dvbdmxfeed->type);
		return -EINVAL;
	}

	if (result)
		return result;

	if ((dvbdmxfeed->type == DMX_TYPE_SEC) ||
		(dvbdmxfeed->ts_type & TS_PACKET) ||
		(dvbdmx->dmx.frontend->source == DMX_MEMORY_FE))
		return avia_gt_dmx_queue_irq_enable(((struct avia_gt_dmx_queue *)(dvbdmxfeed->priv))->index);

	return 0;
}

static
int avia_gt_napi_stop_feed(struct dvb_demux_feed *dvbdmxfeed)
{
	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;
	struct avia_gt_dmx_queue *queue = dvbdmxfeed->priv;

	if (!dvbdmx->dmx.frontend)
		return -EINVAL;

	/*
	 * only audio and video have a
	 * demux queue during ts playback
	 */
	if ((dvbdmx->dmx.frontend->source == DMX_MEMORY_FE) &&
		(((dvbdmxfeed->pes_type != DMX_TS_PES_AUDIO) &&
		  (dvbdmxfeed->pes_type != DMX_TS_PES_VIDEO)) ||
		 (!(dvbdmxfeed->ts_type & TS_DECODER)) ||
		 (dvbdmxfeed->type == DMX_TYPE_SEC)))
			return 0;

	if ((dvbdmxfeed->type != DMX_TYPE_SEC) && (dvbdmxfeed->ts_type & TS_DECODER))
		switch (dvbdmxfeed->pes_type) {
		case DMX_TS_PES_AUDIO:
		case DMX_TS_PES_VIDEO:
			avia_av_napi_decoder_stop(dvbdmxfeed);
			break;
		case DMX_TS_PES_TELETEXT:
			avia_gt_vbi_stop();
			break;
		case DMX_TS_PES_PCR:
			avia_gt_dmx_set_pcr_pid(0, 0x0000);
			if (!(dvbdmxfeed->ts_type & TS_PACKET))
				return 0;
			break;
		default:
			break;
		}

	avia_gt_dmx_queue_stop(queue->index);

	if (queue->hw_sec_index >= 0)
		avia_gt_dmx_free_section_filter(queue->hw_sec_index);

	return avia_gt_dmx_free_queue(queue->index);
}

static
void avia_gt_napi_before_after_tune(fe_status_t fe_status, void *data)
{
	if (fe_status & FE_HAS_LOCK)
		avia_gt_dmx_enable_framer();
	else
		avia_gt_dmx_disable_framer();
}

static
int avia_gt_napi_connect_frontend(dmx_demux_t *demux, dmx_frontend_t *frontend)
{
	int err;

	if ((err = dvbdmx_connect_frontend(demux, frontend)) < 0)
		return err;

	if (demux->frontend->source == DMX_MEMORY_FE)
		if ((err = avia_gt_dmx_enable_clip_mode(AVIA_GT_DMX_SYSTEM_QUEUES)) < 0)
			return err;

	return 0;
}

static
int avia_gt_napi_disconnect_frontend(dmx_demux_t *demux)
{
	int err;

	if (demux->frontend->source == DMX_MEMORY_FE)
		if ((err = avia_gt_dmx_disable_clip_mode(AVIA_GT_DMX_SYSTEM_QUEUES)) < 0)
			return err;

	return dvbdmx_disconnect_frontend(demux);
}

static const struct ca_slot_info avia_gt_napi_ecd_slot_info = {
	.num = 0,
	.type = CA_DESCR,
	.flags = 0,
};

static const struct ca_descr_info avia_gt_napi_ecd_descr_info = {
	.num = 8,
	.type = CA_ECD,
};

static const struct ca_caps avia_gt_napi_ecd_caps = {
	.slot_num = 1,
	.slot_type = CA_DESCR,
	.descr_num = 8,
	.descr_type = CA_ECD,
};

static
int avia_gt_napi_ecd_ioctl(struct inode *inode, struct file *file, unsigned int cmd, void *parg)
{
	if (((file->f_flags & O_ACCMODE) == O_RDONLY) &&
		(cmd != CA_GET_CAP) &&
		(cmd != CA_GET_SLOT_INFO) &&
		(cmd != CA_GET_DESCR_INFO) &&
		(cmd != CA_GET_MSG))
		return -EPERM;

	switch (cmd) {
	case CA_RESET:
		avia_gt_dmx_ecd_reset();
		break;

	case CA_GET_CAP:
		memcpy(parg, &avia_gt_napi_ecd_caps, sizeof(struct ca_caps));
		break;

	case CA_GET_SLOT_INFO:
		memcpy(parg, &avia_gt_napi_ecd_slot_info, sizeof(struct ca_slot_info));
		break;

	case CA_GET_DESCR_INFO:
		memcpy(parg, &avia_gt_napi_ecd_descr_info, sizeof(struct ca_descr_info));
		break;

	case CA_GET_MSG:
	case CA_SEND_MSG:
		return -EOPNOTSUPP;

	case CA_SET_DESCR:
	{
		struct ca_descr *d = (struct ca_descr *)parg;

		if ((d->index >= avia_gt_napi_ecd_caps.descr_num) || (d->parity > 1))
			return -EINVAL;

		return avia_gt_dmx_ecd_set_key(d->index, d->parity, d->cw);
	}

	case CA_SET_PID:
	{
		struct ca_pid *p = (struct ca_pid *)parg;

		if ((p->index < -1) || (p->index >= (int)avia_gt_napi_ecd_caps.descr_num))
			return -EINVAL;

		if (p->pid > 0x1fff)
			return -EINVAL;

		if (p->index == -1)
			return 0;

		return avia_gt_dmx_ecd_set_pid(p->index, p->pid);
	}

	default:
		return -ENOIOCTLCMD;
	}

	return 0;
}

static struct file_operations avia_gt_napi_ecd_fops = {
	.owner = THIS_MODULE,
	.ioctl = dvb_generic_ioctl,
	.open = dvb_generic_open,
	.release = dvb_generic_release,
};

static struct dvb_device avia_gt_napi_ecd_dev = {
	.priv = 0,
	.users = ~0,
	.writers = 1,
	.fops = &avia_gt_napi_ecd_fops,
	.kernel_ioctl = avia_gt_napi_ecd_ioctl,
};

int __init avia_gt_napi_init(void)
{
	int result;

	printk(KERN_INFO "avia_gt_napi: $Id: avia_gt_napi.c,v 1.183 2003/07/24 01:59:21 homar Exp $\n");

	gt_info = avia_gt_get_info();

	if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {
		printk(KERN_ERR "avia_gt_napi: Unsupported chip type\n");
		return -EIO;
	}

	adapter = avia_napi_get_adapter();

	if (!adapter)
		return -EINVAL;

	memset(&demux, 0, sizeof(demux));

	demux.dmx.capabilities = DMX_TS_FILTERING | DMX_PES_FILTERING |
		DMX_SECTION_FILTERING | DMX_MEMORY_BASED_FILTERING |
		DMX_CRC_CHECKING;

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

	if ((hw_dmx_sec) && ((hw_dmx_sec = avia_gt_dmx_get_hw_sec_filt_avail())))
		printk(KERN_INFO "avia_gt_napi: hw section filtering enabled.\n");

	if (dvb_dmx_init(&demux)) {
		printk(KERN_ERR "avia_gt_napi: dvb_dmx_init failed\n");
		return -EFAULT;
	}

	demux.dmx.connect_frontend = avia_gt_napi_connect_frontend;
	demux.dmx.disconnect_frontend = avia_gt_napi_disconnect_frontend;

	dmxdev.filternum = 31;
	dmxdev.demux = &demux.dmx;
	dmxdev.capabilities = 0;

	if ((result = dvb_dmxdev_init(&dmxdev, adapter)) < 0) {
		printk(KERN_ERR "avia_gt_napi: dvb_dmxdev_init failed (errno=%d)\n", result);
		goto init_failed_dmx_release;
	}

	fe_hw.source = DMX_FRONTEND_0;

	if ((result = demux.dmx.add_frontend(&demux.dmx, &fe_hw)) < 0) {
		printk(KERN_ERR "avia_gt_napi: add_frontend (hw) failed (errno=%d)\n", result);
		goto init_failed_dmxdev_release;
	}

	fe_mem.source = DMX_MEMORY_FE;

	if ((result = demux.dmx.add_frontend(&demux.dmx, &fe_mem)) < 0) {
		printk(KERN_ERR "avia_gt_napi: add_frontend (mem) failed (errno=%d)\n", result);
		goto init_failed_remove_hw_frontend;
	}

	if ((result = demux.dmx.connect_frontend(&demux.dmx, &fe_hw)) < 0) {
		printk(KERN_ERR "avia_gt_napi: connect_frontend failed (errno=%d)\n", result);
		goto init_failed_remove_mem_frontend;
	}

	if ((result = dvb_add_frontend_notifier(adapter, avia_gt_napi_before_after_tune, NULL)) < 0) {
		printk(KERN_ERR "avia_gt_napi: dvb_add_frontend_notifier failed (errno=%d)\n", result);
		goto init_failed_disconnect_frontend;
	}

	if (avia_gt_dmx_ecd_ucode_present()) {
		if ((result = dvb_register_device(adapter, &ca_dev, &avia_gt_napi_ecd_dev, NULL, DVB_DEVICE_CA)) < 0) {
			printk(KERN_ERR "avia_gt_napi: dvb_register_device failed (errno=%d)\n", result);
			goto init_failed_remove_frontend_notifier;
		}
	}

	net.card_num = adapter->num;
	dvb_net_init(adapter, &net, &demux.dmx);
	return 0;

init_failed_remove_frontend_notifier:
	dvb_remove_frontend_notifier(adapter, avia_gt_napi_before_after_tune);
init_failed_disconnect_frontend:
	demux.dmx.disconnect_frontend(&demux.dmx);
init_failed_remove_mem_frontend:
	demux.dmx.remove_frontend(&demux.dmx, &fe_mem);
init_failed_remove_hw_frontend:
	demux.dmx.remove_frontend(&demux.dmx, &fe_hw);
init_failed_dmxdev_release:
	dvb_dmxdev_release(&dmxdev);
init_failed_dmx_release:
	dvb_dmx_release(&demux);

	return result;
}

void __exit avia_gt_napi_exit(void)
{
	dvb_net_release(&net);
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
MODULE_DESCRIPTION("AViA eNX/GTX demux driver");
MODULE_LICENSE("GPL");
