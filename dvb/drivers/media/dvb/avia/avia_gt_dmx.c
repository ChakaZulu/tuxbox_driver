/*
 * $Id: avia_gt_dmx.c,v 1.169 2003/04/25 05:08:19 obi Exp $
 *
 * AViA eNX/GTX dmx driver (dbox-II-project)
 *
 * Homepage: http://dbox2.elxsi.de
 *
 * Copyright (C) 2002 Florian Schirmer (jolt@tuxbox.org)
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

#define __KERNEL_SYSCALLS__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/byteorder/swab.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>

#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/unistd.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/tqueue.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/div64.h>

//#define DEBUG
#include "../dvb-core/demux.h"
#include "../dvb-core/dvb_demux.h"
#include "avia_av.h"
#include "avia_gt.h"
#include "avia_gt_dmx.h"
#include "avia_gt_accel.h"
#include "avia_gt_napi.h"
#include "avia_gt_ucode.h"

//#define dprintk printk

//static void avia_gt_dmx_queue_task(void *tl_data);
static void avia_gt_dmx_bh_task(void *tl_data);

/*
struct tq_struct avia_gt_dmx_queue_tasklet = {

	routine: avia_gt_dmx_queue_task,
	data: 0

};
*/
static int errno = 0;
static sAviaGtInfo *gt_info = NULL;
static sRISC_MEM_MAP *risc_mem_map = NULL;
static char *ucode = NULL;
static s32 hw_sections = 1;
static u8 force_stc_reload = 0;
static sAviaGtDmxQueue queue_list[AVIA_GT_DMX_QUEUE_COUNT];
static void gtx_pcr_interrupt(unsigned short irq);
static s8 section_filter_umap[32];
static sFilter_Definition_Entry filter_definition_table[32];
static s8 video_queue_client = -1;
static s8 audio_queue_client = -1;
static s8 teletext_queue_client = -1;

static const u8 queue_size_table[AVIA_GT_DMX_QUEUE_COUNT] =	{	// sizes are 1<<x*64bytes. BEWARE OF THE ALIGNING!
									// DO NOT CHANGE UNLESS YOU KNOW WHAT YOU'RE DOING!
	10,			// video
	9,			// audio
	9,			// teletext
	10, 10, 10, 10, 10,	// user 3..7
	8, 8, 8, 8, 8, 8, 8, 8,	// user 8..15
	8, 8, 8, 8, 7, 7, 7, 7,	// user 16..23
	7, 7, 7, 7, 7, 7, 7,	// user 24..30
	7			// message

};

static const u8 queue_system_map[] = {2, 0, 1};

static void avia_gt_dmx_enable_disable_system_queue_irqs(void)
{
	unsigned queue_nr;
	s8 new_video_queue_client = -1;
	s8 new_audio_queue_client = -1;
	s8 new_teletext_queue_client = -1;
	u32 write_pos;

	for (queue_nr = AVIA_GT_DMX_QUEUE_USER_START; queue_nr < AVIA_GT_DMX_QUEUE_COUNT; queue_nr++)
	{
		if (queue_list[queue_nr].running == 0) {
			continue;
		}
		if ( (new_video_queue_client == -1) &&
		     (queue_list[AVIA_GT_DMX_QUEUE_VIDEO].pid != 0xFFFF) &&
		     (queue_list[AVIA_GT_DMX_QUEUE_VIDEO].pid == queue_list[queue_nr].pid) &&
		     (queue_list[AVIA_GT_DMX_QUEUE_VIDEO].mode == queue_list[queue_nr].mode) ) {
			new_video_queue_client = queue_nr;
		}

		if ( (new_audio_queue_client == -1) &&
		     (queue_list[AVIA_GT_DMX_QUEUE_AUDIO].pid != 0xFFFF) &&
		     (queue_list[AVIA_GT_DMX_QUEUE_AUDIO].pid == queue_list[queue_nr].pid) &&
		     (queue_list[AVIA_GT_DMX_QUEUE_AUDIO].mode == queue_list[queue_nr].mode) ) {
			new_audio_queue_client = queue_nr;
		}

		if ( (new_teletext_queue_client == -1) &&
		     (queue_list[AVIA_GT_DMX_QUEUE_TELETEXT].pid != 0xFFFF) &&
		     (queue_list[AVIA_GT_DMX_QUEUE_TELETEXT].pid == queue_list[queue_nr].pid) &&
		     (queue_list[AVIA_GT_DMX_QUEUE_TELETEXT].mode == queue_list[queue_nr].mode) ) {
			new_teletext_queue_client = queue_nr;
		}
	}

	if ( (video_queue_client == -1) && (audio_queue_client == -1) &&
	     ( (new_video_queue_client != -1) || (new_audio_queue_client != -1) ) ) {
		video_queue_client = new_video_queue_client;
		audio_queue_client = new_audio_queue_client;
		write_pos = avia_gt_dmx_queue_get_write_pos(AVIA_GT_DMX_QUEUE_VIDEO);
		queue_list[AVIA_GT_DMX_QUEUE_VIDEO].hw_write_pos = write_pos;
		queue_list[AVIA_GT_DMX_QUEUE_VIDEO].hw_read_pos = write_pos;
		queue_list[AVIA_GT_DMX_QUEUE_VIDEO].write_pos = write_pos;
		queue_list[AVIA_GT_DMX_QUEUE_VIDEO].read_pos = write_pos;
		queue_list[AVIA_GT_DMX_QUEUE_VIDEO].overflow_count = 0;
		avia_gt_dmx_queue_irq_enable(AVIA_GT_DMX_QUEUE_VIDEO);
	}
	else if ( (new_video_queue_client == -1) && (new_audio_queue_client == -1) && ((video_queue_client != -1) || (audio_queue_client != -1)) ) {
		avia_gt_dmx_queue_irq_disable(AVIA_GT_DMX_QUEUE_VIDEO);
	}

	video_queue_client = new_video_queue_client;
	audio_queue_client = new_audio_queue_client;

	if ( (new_teletext_queue_client != -1) && (teletext_queue_client == -1) ) {
		teletext_queue_client = new_teletext_queue_client;
		write_pos = avia_gt_dmx_queue_get_write_pos(AVIA_GT_DMX_QUEUE_AUDIO);
		queue_list[AVIA_GT_DMX_QUEUE_TELETEXT].hw_write_pos = write_pos;
		queue_list[AVIA_GT_DMX_QUEUE_TELETEXT].hw_read_pos = write_pos;
		queue_list[AVIA_GT_DMX_QUEUE_TELETEXT].write_pos = write_pos;
		queue_list[AVIA_GT_DMX_QUEUE_TELETEXT].read_pos = write_pos;
		queue_list[AVIA_GT_DMX_QUEUE_TELETEXT].overflow_count = 0;
		avia_gt_dmx_queue_irq_enable(AVIA_GT_DMX_QUEUE_TELETEXT);
	}
	else if ( (new_teletext_queue_client == -1) && (teletext_queue_client != -1) ) {
		avia_gt_dmx_queue_irq_disable(AVIA_GT_DMX_QUEUE_TELETEXT);
	}
	teletext_queue_client = new_teletext_queue_client;
}

static struct avia_gt_dmx_queue *avia_gt_dmx_alloc_queue(u8 queue_nr, AviaGtDmxQueueProc *irq_proc, AviaGtDmxQueueProc *cb_proc, void *priv_data)
{

	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {

		printk(KERN_CRIT "avia_gt_dmx: alloc_queue: queue %d out of bounce\n", queue_nr);

		return NULL;

	}

	if (queue_list[queue_nr].busy) {

		printk(KERN_ERR "avia_gt_dmx: alloc_queue: queue %d busy\n", queue_nr);

		return NULL;

	}

	queue_list[queue_nr].busy = 1;
	queue_list[queue_nr].cb_proc = cb_proc;
	queue_list[queue_nr].hw_read_pos = 0;
	queue_list[queue_nr].hw_write_pos = 0;
	queue_list[queue_nr].irq_count = 0;
	queue_list[queue_nr].irq_proc = irq_proc;
	queue_list[queue_nr].priv_data = priv_data;
	queue_list[queue_nr].qim_irq_count = 0;
	queue_list[queue_nr].qim_jiffies = jiffies;
	queue_list[queue_nr].qim_mode = 0;
	queue_list[queue_nr].read_pos = 0;
	queue_list[queue_nr].write_pos = 0;
	queue_list[queue_nr].info.hw_sec_index = -1;

	queue_list[queue_nr].task_struct.routine=avia_gt_dmx_bh_task;
	queue_list[queue_nr].task_struct.data=&(queue_list[queue_nr].info.index);
	
	avia_gt_dmx_queue_reset(queue_nr);
	avia_gt_dmx_set_queue_irq(queue_nr, 0, 0);

	return &queue_list[queue_nr].info;

}

struct avia_gt_dmx_queue *avia_gt_dmx_alloc_queue_audio(AviaGtDmxQueueProc *irq_proc, AviaGtDmxQueueProc *cb_proc, void *priv_data)
{
	return avia_gt_dmx_alloc_queue(AVIA_GT_DMX_QUEUE_AUDIO, irq_proc, cb_proc, priv_data);
}

struct avia_gt_dmx_queue *avia_gt_dmx_alloc_queue_message(AviaGtDmxQueueProc *irq_proc, AviaGtDmxQueueProc *cb_proc, void *priv_data)
{
	return avia_gt_dmx_alloc_queue(AVIA_GT_DMX_QUEUE_MESSAGE, irq_proc, cb_proc, priv_data);
}

struct avia_gt_dmx_queue *avia_gt_dmx_alloc_queue_teletext(AviaGtDmxQueueProc *irq_proc, AviaGtDmxQueueProc *cb_proc, void *priv_data)
{
	return avia_gt_dmx_alloc_queue(AVIA_GT_DMX_QUEUE_TELETEXT, irq_proc, cb_proc, priv_data);
}

struct avia_gt_dmx_queue *avia_gt_dmx_alloc_queue_user(AviaGtDmxQueueProc *irq_proc, AviaGtDmxQueueProc *cb_proc, void *priv_data)
{
	u8 queue_nr;

	for (queue_nr = AVIA_GT_DMX_QUEUE_USER_START; queue_nr <= AVIA_GT_DMX_QUEUE_USER_END; queue_nr++)
		if (!queue_list[queue_nr].busy)
			return avia_gt_dmx_alloc_queue(queue_nr, irq_proc, cb_proc, priv_data);

	return NULL;
}

struct avia_gt_dmx_queue *avia_gt_dmx_alloc_queue_video(AviaGtDmxQueueProc *irq_proc, AviaGtDmxQueueProc *cb_proc, void *priv_data)
{
	return avia_gt_dmx_alloc_queue(AVIA_GT_DMX_QUEUE_VIDEO, irq_proc, cb_proc, priv_data);
}

s32 avia_gt_dmx_free_queue(u8 queue_nr)
{
	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_CRIT "avia_gt_dmx: free_queue: queue %d out of bounce\n", queue_nr);
		return -EINVAL;
	}

	if (!queue_list[queue_nr].busy) {
		printk(KERN_ERR "avia_gt_dmx: free_queue: queue %d not busy\n", queue_nr);
		return -EFAULT;
	}

	avia_gt_dmx_queue_irq_disable(queue_nr);

	queue_list[queue_nr].busy = 0;
	queue_list[queue_nr].cb_proc = NULL;
	queue_list[queue_nr].irq_count = 0;
	queue_list[queue_nr].irq_proc = NULL;
	queue_list[queue_nr].priv_data = NULL;

	return 0;
}

u8 avia_gt_dmx_get_hw_sec_filt_avail(void)
{
	if (hw_sections == 1) {
		switch (risc_mem_map->Version_no) {
		case 0x0013:
		case 0x0014:
			return 1;
		default:
			break;
		}
	}
	else if (hw_sections == 2) {
		return 1;
	}

	return 0;
}

unsigned char avia_gt_dmx_map_queue(unsigned char queue_nr)
{

	if (avia_gt_chip(ENX)) {

		if (queue_nr >= 16)
			enx_reg_set(CFGR0, UPQ, 1);
		else
			enx_reg_set(CFGR0, UPQ, 0);

	} else if (avia_gt_chip(GTX)) {

		if (queue_nr >= 16)
			gtx_reg_set(CR1, UPQ, 1);
		else
			gtx_reg_set(CR1, UPQ, 0);

	}

	mb();

	return (queue_nr & 0x0F);

}

void avia_gt_dmx_force_discontinuity(void)
{

	force_stc_reload = 1;

	if (avia_gt_chip(ENX))
		enx_reg_set(FC, FD, 1);
	else if (avia_gt_chip(GTX))
		gtx_reg_16(FCR) |= 0x100;

}

sAviaGtDmxQueue *avia_gt_dmx_get_queue_info(u8 queue_nr)
{

	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {

		printk(KERN_CRIT "avia_gt_dmx: get_queue_info: queue %d out of bounce\n", queue_nr);

		return NULL;

	}

	return &queue_list[queue_nr];

}

u16 avia_gt_dmx_get_queue_irq(u8 queue_nr)
{

	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {

		printk(KERN_CRIT "avia_gt_dmx: alloc_queue: queue %d out of bounce\n", queue_nr);

		return 0;

	}

	if (avia_gt_chip(ENX)) {

		if (queue_nr >= 17)
			return AVIA_GT_IRQ(3, queue_nr - 16);
		else if (queue_nr >= 2)
			return AVIA_GT_IRQ(4, queue_nr - 1);
		else
			return AVIA_GT_IRQ(5, queue_nr + 6);

	} else if (avia_gt_chip(GTX)) {

		return AVIA_GT_IRQ(2 + !!(queue_nr & 16), queue_nr & 15);

	}

	return 0;

}

int avia_gt_dmx_load_ucode(void)
{

	int fd = 0;
	loff_t file_size;
	mm_segment_t fs;
	u8 ucode_fs_buf[2048];
	u8 *ucode_buf = ucode_fs_buf;

	fs = get_fs();
	set_fs(get_ds());

	if ((fd = open(ucode, 0, 0)) < 0) {

		printk (KERN_INFO "avia_gt_dmx: No firmware found at %s, using compiled-in version.\n", ucode);

		set_fs(fs);

		/* fall back to compiled-in ucode */
		ucode_buf = (u8 *) avia_gt_dmx_ucode_img;
		file_size = avia_gt_dmx_ucode_size;

	}

	else {
		file_size = lseek(fd, 0L, 2);

		if ((file_size <= 0) || (file_size > 2048)) {

			printk (KERN_ERR "avia_gt_dmx: Firmware wrong size '%s'\n", ucode);

			sys_close(fd);
			set_fs(fs);

			return -EFAULT;

		}

		lseek(fd, 0L, 0);

		if (read(fd, ucode_buf, file_size) != file_size) {

			printk (KERN_ERR "avia_gt_dmx: Failed to read firmware file '%s'\n", ucode);

			sys_close(fd);
			set_fs(fs);

			return -EIO;

		}

		close(fd);
		set_fs(fs);
	}

	/* queues should be stopped */
	if (file_size > 0x400)
		for (fd = 0x700; fd < 0x740;) {
			ucode_buf[fd++] = 0xDF;
			ucode_buf[fd++] = 0xFF;
		}

	avia_gt_dmx_risc_write(ucode_buf, risc_mem_map, file_size);

	printk(KERN_INFO "avia_gt_dmx: Successfully loaded ucode v%04X\n", risc_mem_map->Version_no);

	return 0;

}

void avia_gt_dmx_fake_queue_irq(u8 queue_nr)
{

	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {

		printk(KERN_CRIT "avia_gt_dmx: fake_queue_irq: queue %d out of bounce\n", queue_nr);

		return;

	}

	queue_list[queue_nr].irq_count++;

	schedule_task(&(queue_list[queue_nr].task_struct));
}

u32 avia_gt_dmx_queue_crc32(struct avia_gt_dmx_queue *queue, u32 count, u32 seed)
{

	u32 chunk1_size;
	u32 crc;

	if (queue->index >= AVIA_GT_DMX_QUEUE_COUNT) {

		printk(KERN_CRIT "avia_gt_dmx: crc32: queue %d out of bounce\n", queue->index);

		return 0;

	}

	if ((queue_list[queue->index].read_pos + count) > queue_list[queue->index].size) {

		chunk1_size = queue_list[queue->index].size - queue_list[queue->index].read_pos;

		// FIXME
		if ((avia_gt_chip(GTX)) && (chunk1_size <= 4))
			return 0;

		crc = avia_gt_accel_crc32(queue_list[queue->index].mem_addr + queue_list[queue->index].read_pos, chunk1_size, seed);

		return avia_gt_accel_crc32(queue_list[queue->index].mem_addr, count - chunk1_size, crc);

	} else {

		return avia_gt_accel_crc32(queue_list[queue->index].mem_addr + queue_list[queue->index].read_pos, count, seed);

	}

}

u32 avia_gt_dmx_queue_data_get(struct avia_gt_dmx_queue *queue, void *dest, u32 count, u8 peek)
{

	u32 bytes_avail = queue->bytes_avail(queue);
	u32 done = 0;
	u32 read_pos;

	if (queue->index >= AVIA_GT_DMX_QUEUE_COUNT) {

		printk(KERN_CRIT "avia_gt_dmx: queue_data_move: queue %d out of bounce\n", queue->index);

		return 0;

	}

	if (count > bytes_avail) {

		printk(KERN_ERR "avia_gt_dmx: queue_data_move: %d bytes requested, %d available\n", count, bytes_avail);

		count = bytes_avail;

	}

	read_pos = queue_list[queue->index].read_pos;

	if ((read_pos > queue_list[queue->index].write_pos) &&
		(count >= (queue_list[queue->index].size - read_pos))) {

		done = queue_list[queue->index].size - read_pos;

		if (dest)
			memcpy(dest, gt_info->mem_addr + queue_list[queue->index].mem_addr + read_pos, done);

		read_pos = 0;

	}

	if ((dest) && (count - done))
		memcpy(((u8 *)dest) + done, gt_info->mem_addr + queue_list[queue->index].mem_addr + read_pos, count - done);

	if (!peek)
		queue_list[queue->index].read_pos = read_pos + (count - done);

	return count;

}

u8 avia_gt_dmx_queue_data_get8(struct avia_gt_dmx_queue *queue, u8 peek)
{

	u8 data;

	avia_gt_dmx_queue_data_get(queue, &data, sizeof(u8), peek);

	return data;

}

u16 avia_gt_dmx_queue_data_get16(struct avia_gt_dmx_queue *queue, u8 peek)
{

	u16 data;

	avia_gt_dmx_queue_data_get(queue, &data, sizeof(u16), peek);

	return data;

}

u32 avia_gt_dmx_queue_data_get32(struct avia_gt_dmx_queue *queue, u8 peek)
{

	u32 data;

	avia_gt_dmx_queue_data_get(queue, &data, sizeof(u32), peek);

	return data;

}

u32 avia_gt_dmx_queue_data_put(struct avia_gt_dmx_queue *queue, void *src, u32 count, u8 src_is_user_space)
{

	u32 bytes_free = queue->bytes_free(queue);
	u32 done = 0;

	if ((queue->index != AVIA_GT_DMX_QUEUE_VIDEO) &&
		(queue->index != AVIA_GT_DMX_QUEUE_AUDIO) &&
		(queue->index != AVIA_GT_DMX_QUEUE_TELETEXT)) {

		printk(KERN_CRIT "avia_gt_dmx: queue_data_put: queue %d out of bounce\n", queue->index);

		return 0;

	}

	if (count > bytes_free) {

		printk(KERN_ERR "avia_gt_dmx: queue_data_put: %d bytes requested, %d available\n", count, bytes_free);

		count = bytes_free;

	}

	if ((queue_list[queue->index].write_pos + count) >= queue_list[queue->index].size) {

		done = queue_list[queue->index].size - queue_list[queue->index].write_pos;

		if (src_is_user_space)
			copy_from_user(gt_info->mem_addr + queue_list[queue->index].mem_addr + queue_list[queue->index].write_pos, src, done);
		else
			memcpy(gt_info->mem_addr + queue_list[queue->index].mem_addr + queue_list[queue->index].write_pos, src, done);

		queue_list[queue->index].write_pos = 0;

	}

	if (count - done) {

		if (src_is_user_space)
			copy_from_user(gt_info->mem_addr + queue_list[queue->index].mem_addr + queue_list[queue->index].write_pos, ((u8 *)src) + done, count - done);
		else
			memcpy(gt_info->mem_addr + queue_list[queue->index].mem_addr + queue_list[queue->index].write_pos, ((u8 *)src) + done, count - done);

		queue_list[queue->index].write_pos += (count - done);

	}

	return count;

}

u32 avia_gt_dmx_queue_get_buf1_ptr(struct avia_gt_dmx_queue *queue)
{

	return queue_list[queue->index].mem_addr + queue_list[queue->index].read_pos;

}

u32 avia_gt_dmx_queue_get_buf2_ptr(struct avia_gt_dmx_queue *queue)
{

	if (queue_list[queue->index].write_pos >= queue_list[queue->index].read_pos)
		return 0;
	else
		return queue_list[queue->index].mem_addr;

}

u32 avia_gt_dmx_queue_get_buf1_size(struct avia_gt_dmx_queue *queue)
{

	if (queue_list[queue->index].write_pos >= queue_list[queue->index].read_pos)
		return (queue_list[queue->index].write_pos - queue_list[queue->index].read_pos);
	else
		return (queue_list[queue->index].size - queue_list[queue->index].read_pos);

}

u32 avia_gt_dmx_queue_get_buf2_size(struct avia_gt_dmx_queue *queue)
{

	if (queue_list[queue->index].write_pos >= queue_list[queue->index].read_pos)
		return 0;
	else
		return queue_list[queue->index].write_pos;

}

u32 avia_gt_dmx_queue_get_bytes_avail(struct avia_gt_dmx_queue *queue)
{

	return (avia_gt_dmx_queue_get_buf1_size(queue) + avia_gt_dmx_queue_get_buf2_size(queue));

}

u32 avia_gt_dmx_queue_get_bytes_free(struct avia_gt_dmx_queue *queue)
{

	if ((queue->index != AVIA_GT_DMX_QUEUE_VIDEO) &&
		(queue->index != AVIA_GT_DMX_QUEUE_AUDIO) &&
		(queue->index != AVIA_GT_DMX_QUEUE_TELETEXT)) {

		printk(KERN_CRIT "avia_gt_dmx: queue_get_bytes_free: queue %d out of bounce\n", queue->index);

		return 0;

	}

	queue_list[queue->index].hw_read_pos = avia_gt_dmx_system_queue_get_read_pos(queue->index);

	if (queue_list[queue->index].write_pos >= queue_list[queue->index].hw_read_pos)
		/*         free chunk1        busy chunk1        free chunk2   */
		/* queue [ FFFFFFFFFFF (RPOS) BBBBBBBBBBB (WPOS) FFFFFFFFFFF ] */
		return (queue_list[queue->index].size - queue_list[queue->index].write_pos) + queue_list[queue->index].hw_read_pos - 1;
	else
		/*         busy chunk1        free chunk1        busy chunk2   */
		/* queue [ BBBBBBBBBBB (WPOS) FFFFFFFFFFF (RPOS) BBBBBBBBBBB ] */
		return (queue_list[queue->index].hw_read_pos - queue_list[queue->index].write_pos) - 1;

}

u32 avia_gt_dmx_queue_get_size(struct avia_gt_dmx_queue *queue)
{
	return queue_list[queue->index].size;
}

u32 avia_gt_dmx_queue_get_write_pos(u8 queue_nr)
{

	u8 mapped_queue_nr;
	u32 previous_write_pos;
	u32 write_pos = 0xFFFFFFFF;

	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {

		printk(KERN_CRIT "avia_gt_dmx: queue_get_write_pos: queue %d out of bounce\n", queue_nr);

		return 0;

	}

	mapped_queue_nr = avia_gt_dmx_map_queue(queue_nr);

    /*
     *
     * CAUTION: The correct sequence for accessing Queue
     * Pointer registers is as follows:
     * For reads,
     * Read low word
     * Read high word
     * CAUTION: Not following these sequences will yield
     * invalid data.
     *
     */

	do {

		previous_write_pos = write_pos;

		if (avia_gt_chip(ENX))
			write_pos = ((enx_reg_so(QWPnL, 4 * mapped_queue_nr)->Queue_n_Write_Pointer) | (enx_reg_so(QWPnH, 4 * mapped_queue_nr)->Queue_n_Write_Pointer << 16));
		else if (avia_gt_chip(GTX))
			write_pos = ((gtx_reg_so(QWPnL, 4 * mapped_queue_nr)->Queue_n_Write_Pointer) | (gtx_reg_so(QWPnH, 4 * mapped_queue_nr)->Upper_WD_n << 16));

	} while (previous_write_pos != write_pos);

	if ((write_pos >= (queue_list[queue_nr].mem_addr + queue_list[queue_nr].size)) ||
		(write_pos < queue_list[queue_nr].mem_addr)) {
		
		printk(KERN_CRIT "avia_gt_napi: queue %d hw_write_pos out of bounds! (B:0x%X/P:0x%X/E:0x%X)\n", queue_nr, queue_list[queue_nr].mem_addr, write_pos, queue_list[queue_nr].mem_addr + queue_list[queue_nr].size);
		
#ifdef DEBUG
		BUG();
#endif

		queue_list[queue_nr].hw_read_pos = 0;
		queue_list[queue_nr].hw_write_pos = 0;
		queue_list[queue_nr].read_pos = 0;
		queue_list[queue_nr].write_pos = 0;

		avia_gt_dmx_queue_set_write_pos(queue_nr, 0);

		return 0;
			
	}

	return (write_pos - queue_list[queue_nr].mem_addr);

}

void avia_gt_dmx_queue_flush(struct avia_gt_dmx_queue *queue)
{

	queue_list[queue->index].read_pos = queue_list[queue->index].write_pos;

}

/*
 * Für den "Block-IRQ-Modus" wird folgender Algorithmus verwendet:
 * Der gesamte Puffer wird gesechzehntelt. Es wird herausgefunden, in
 * welchem Sechzehntel sich der Schreibpointer gerade befindet:
 * Analog der Prozentrechnung
 *  write_pointer * 100 / queue_size mit den Grenzen 6.25, 12.5, ... 100 wird
 *  write_pointer * 16 / queue_size mit den Grenzen 1, 2 .. 16
 * verwendet.
 * Der Interrupt wird dann auf das Erreichen des _übernächsten_ Sechzehntel gesetzt.
 * Das Setzen auf das nächste Sechzehntel könnte zu Problemen führen, wenn der
 * write_pointer kurz vor der Grenze ist.
 */

static void avia_gt_dmx_queue_qim_mode_update(u8 queue_nr)
{

	u8 block_pos;

	block_pos = (queue_list[queue_nr].hw_write_pos * 16) / queue_list[queue_nr].size;

	avia_gt_dmx_set_queue_irq(queue_nr, 1, ((block_pos + 2) % 16) * 2);

}

static void avia_gt_dmx_queue_interrupt(unsigned short irq)
{

	u8 bit = AVIA_GT_IRQ_BIT(irq);
	u8 nr = AVIA_GT_IRQ_REG(irq);
	u32 old_hw_write_pos;

	s32 queue_nr = -EINVAL;

	if (avia_gt_chip(ENX)) {

		if (nr == 3)
			queue_nr = bit + 16;
		else if (nr == 4)
			queue_nr = bit + 1;
		else if (nr == 5)
			queue_nr = bit - 6;

	} else if (avia_gt_chip(GTX)) {

		queue_nr = (nr - 2) * 16 + bit;

	}

	if ((queue_nr < 0) || (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT)) {

		printk(KERN_ERR "avia_gt_dmx: unexpected queue irq (nr=%d, bit=%d)\n", nr, bit);

		return;

	}

	if (!queue_list[queue_nr].busy) {

		printk(KERN_ERR "avia_gt_dmx: irq on idle queue (queue_nr=%d)\n", queue_nr);

		return;

	}

	queue_list[queue_nr].irq_count++;

	old_hw_write_pos = queue_list[queue_nr].hw_write_pos;
	queue_list[queue_nr].hw_write_pos = avia_gt_dmx_queue_get_write_pos(queue_nr);

	if (!queue_list[queue_nr].qim_mode) {

		queue_list[queue_nr].qim_irq_count++;

		// We check every secord wether irq load is too high
		if (time_after(jiffies, queue_list[queue_nr].qim_jiffies + HZ)) {

			if (queue_list[queue_nr].qim_irq_count > 100) {

				dprintk(KERN_DEBUG "avia_gt_dmx: detected high irq load on queue %d - enabling qim mode\n", queue_nr);

				queue_list[queue_nr].qim_mode = 1;

				avia_gt_dmx_queue_qim_mode_update(queue_nr);

			}

			queue_list[queue_nr].qim_irq_count = 0;
			queue_list[queue_nr].qim_jiffies = jiffies;

		}

	} else {
	
		avia_gt_dmx_queue_qim_mode_update(queue_nr);
		
	}
	
	// Cases:
	// 
	//    [    R     OW    ] (R < OW)
	//
	// 1. [    R     OW  W ] OK
	// 2. [ W  R     OW    ] OK
	// 3. [    R  W  OW    ] OVERFLOW (incl. R == W)
	//
	//    [    OW     R    ] (OW < R)
	//
	// 4. [    OW  W  R    ] OK
	// 5. [    OW     R  W ] OVERFLOW (incl. R == W)
	// 6. [ W  OW  R       ] OVERFLOW

	if (((queue_list[queue_nr].read_pos < old_hw_write_pos) && ((queue_list[queue_nr].read_pos <= queue_list[queue_nr].hw_write_pos) && (queue_list[queue_nr].hw_write_pos < old_hw_write_pos))) ||
		((old_hw_write_pos < queue_list[queue_nr].read_pos) && ((queue_list[queue_nr].read_pos <= queue_list[queue_nr].hw_write_pos) || (queue_list[queue_nr].hw_write_pos < old_hw_write_pos))))
		queue_list[queue_nr].overflow_count++;	// We can't recover here or we will break queue handling (get_data*, bytes_avail, ...)

	if (queue_list[queue_nr].irq_proc)
		queue_list[queue_nr].irq_proc(&queue_list[queue_nr].info, queue_list[queue_nr].priv_data);
	else {
		
		if (queue_list[queue_nr].cb_proc) {
			queue_task(&(queue_list[queue_nr].task_struct), &tq_immediate);
			mark_bh(IMMEDIATE_BH);
		}
//		schedule_task(&avia_gt_dmx_queue_tasklet);
	}



		
}

void avia_gt_dmx_queue_irq_disable(u8 queue_nr)
{

	queue_list[queue_nr].running = 0;
	if (queue_nr >= AVIA_GT_DMX_QUEUE_USER_START) {
		avia_gt_dmx_enable_disable_system_queue_irqs();
	}
	avia_gt_free_irq(avia_gt_dmx_get_queue_irq(queue_nr));

}

s32 avia_gt_dmx_queue_irq_enable(u8 queue_nr)
{

	queue_list[queue_nr].running = 1;
	if (queue_nr >= AVIA_GT_DMX_QUEUE_USER_START) {
		avia_gt_dmx_enable_disable_system_queue_irqs();
	}
	return avia_gt_alloc_irq(avia_gt_dmx_get_queue_irq(queue_nr), avia_gt_dmx_queue_interrupt);

}

s32 avia_gt_dmx_queue_reset(u8 queue_nr)
{

	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {

		printk(KERN_CRIT "avia_gt_dmx: queue_reset: queue %d out of bounce\n", queue_nr);

		return -EINVAL;

	}

	queue_list[queue_nr].hw_read_pos = 0;
	queue_list[queue_nr].hw_write_pos = 0;
	queue_list[queue_nr].read_pos = 0;
	queue_list[queue_nr].write_pos = 0;

	avia_gt_dmx_queue_set_write_pos(queue_nr, 0);

	if ((queue_nr == AVIA_GT_DMX_QUEUE_VIDEO) || (queue_nr == AVIA_GT_DMX_QUEUE_AUDIO) || (queue_nr == AVIA_GT_DMX_QUEUE_TELETEXT))
		avia_gt_dmx_system_queue_set_pos(queue_nr, 0, 0);

	return 0;

}

void avia_gt_dmx_queue_set_write_pos(u8 queue_nr, u32 write_pos)
{

	u8 mapped_queue_nr;

	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {

		printk(KERN_CRIT "avia_gt_dmx: set_queue queue (%d) out of bounce\n", queue_nr);

		return;

	}

	mapped_queue_nr = avia_gt_dmx_map_queue(queue_nr);

	if (avia_gt_chip(ENX)) {

		enx_reg_16(QWPnL + 4 * mapped_queue_nr) = (queue_list[queue_nr].mem_addr + write_pos) & 0xFFFF;
		enx_reg_16(QWPnH + 4 * mapped_queue_nr) = (((queue_list[queue_nr].mem_addr + write_pos) >> 16) & 0x3F) | (queue_size_table[queue_nr] << 6);

	} else if (avia_gt_chip(GTX)) {

		gtx_reg_16(QWPnL + 4 * mapped_queue_nr) = (queue_list[queue_nr].mem_addr + write_pos) & 0xFFFF;
		gtx_reg_16(QWPnH + 4 * mapped_queue_nr) = (((queue_list[queue_nr].mem_addr + write_pos) >> 16) & 0x3F) | (queue_size_table[queue_nr] << 6);

	}

}

int avia_gt_dmx_queue_start(u8 queue_nr, u8 mode, u16 pid, u8 wait_pusi, u8 filt_tab_idx, u8 no_of_filter)
{
	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_CRIT "avia_gt_dmx: queue_start queue (%d) out of bounce\n", queue_nr);
		return -EINVAL;
	}

	avia_gt_dmx_queue_reset(queue_nr);
	queue_list[queue_nr].pid = pid;
	queue_list[queue_nr].mode = mode;

	if (queue_nr < AVIA_GT_DMX_QUEUE_USER_START)
		avia_gt_dmx_enable_disable_system_queue_irqs();

	avia_gt_dmx_set_pid_control_table(queue_nr, mode, 0, 0, 0, 1, 0, filt_tab_idx, (mode == AVIA_GT_DMX_QUEUE_MODE_SEC8) ? 1 : 0,no_of_filter);
	avia_gt_dmx_set_pid_table(queue_nr, wait_pusi, 0, pid);

	return 0;
}

int avia_gt_dmx_queue_stop(u8 queue_nr)
{
	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_CRIT "avia_gt_dmx: queue_stop queue (%d) out of bounce\n", queue_nr);
		return -EINVAL;
	}

	queue_list[queue_nr].pid = 0xFFFF;
	avia_gt_dmx_queue_irq_disable(queue_nr);
	avia_gt_dmx_set_pid_table(queue_nr, 0, 1, 0);

	return 0;
}

static void avia_gt_dmx_bh_task(void *tl_data)
{
	int queue_nr = *((u8 *) tl_data);
	struct avia_gt_dmx_queue *queue_info = &queue_list[queue_nr].info;
	void *priv_data;
	AviaGtDmxQueueProc *cb_proc;
	u16 pid1 = 0xFFFF;
	u16 pid2;
	u16 packet_pid;
	u8 head[3];
	unsigned avail;

	queue_list[queue_nr].write_pos = queue_list[queue_nr].hw_write_pos;

	if (queue_list[queue_nr].overflow_count) {
		printk(KERN_WARNING "avia_gt_dmx: queue %d overflow (count: %d)\n", queue_nr, queue_list[queue_nr].overflow_count);
		queue_list[queue_nr].overflow_count = 0;
		queue_list[queue_nr].read_pos = queue_list[queue_nr].hw_write_pos;
		return;
	}

	/*
	 * Resync for video, audio and teletext
	 */

	if (queue_nr <= AVIA_GT_DMX_QUEUE_TELETEXT) {
		if (queue_nr == AVIA_GT_DMX_QUEUE_TELETEXT) {
			pid1 = queue_list[AVIA_GT_DMX_QUEUE_TELETEXT].pid;
			pid2 = pid1;
		}
		else {
			if (queue_list[AVIA_GT_DMX_QUEUE_VIDEO].pid != 0xFFFF) {
				pid1 = queue_list[AVIA_GT_DMX_QUEUE_VIDEO].pid;
				pid2 = pid1;
			}
			if (queue_list[AVIA_GT_DMX_QUEUE_AUDIO].pid != 0xFFFF) {
				pid2 = queue_list[AVIA_GT_DMX_QUEUE_AUDIO].pid;
				if (pid1 == 0xFFFF) {
					pid1 = pid2;
				}
			}
		}

		avail = avia_gt_dmx_queue_get_bytes_avail(queue_info);

		while (avail >= 188) {
			avia_gt_dmx_queue_data_get(queue_info,head,sizeof(head),1);

			packet_pid = ((head[1] & 0x1F) << 8) | head[2];
			if ( (head[0] == 0x47) &&
			     ( (packet_pid == pid1) || (packet_pid == pid2) ) ) {
				break;
			}
			avail--;
			avia_gt_dmx_queue_data_get8(queue_info,0);
		}

		if (avail < 188) {
			return;
		}
	}

	if ( (queue_nr == AVIA_GT_DMX_QUEUE_VIDEO) && (video_queue_client != -1) ) {
		cb_proc = queue_list[video_queue_client].cb_proc;
		priv_data = queue_list[video_queue_client].priv_data;
	}
	else if ( (queue_nr == AVIA_GT_DMX_QUEUE_VIDEO) && (audio_queue_client != 1) ) {
		cb_proc = queue_list[audio_queue_client].cb_proc;
		priv_data = queue_list[audio_queue_client].priv_data;
	}
	else if ( (queue_nr == AVIA_GT_DMX_QUEUE_TELETEXT) && (teletext_queue_client != -1) ) {
		cb_proc = queue_list[teletext_queue_client].cb_proc;
		priv_data = queue_list[teletext_queue_client].priv_data;
	}
	else {
		cb_proc = queue_list[queue_nr].cb_proc;
		priv_data = queue_list[queue_nr].priv_data;
	}

	if (cb_proc) {
		cb_proc(queue_info, priv_data);
	}
}
/*
static void avia_gt_dmx_queue_task(void *tl_data)
{

	s8 queue_nr;

	for (queue_nr = (AVIA_GT_DMX_QUEUE_COUNT - 1); queue_nr >= 0; queue_nr--) {	// msg queue must have priority

		queue_list[queue_nr].write_pos = queue_list[queue_nr].hw_write_pos;

		if (queue_list[queue_nr].overflow_count) {

			printk(KERN_WARNING "avia_gt_dmx: queue %d overflow (count: %d)\n", queue_nr, queue_list[queue_nr].overflow_count);

			queue_list[queue_nr].overflow_count = 0;
			queue_list[queue_nr].read_pos = queue_list[queue_nr].hw_write_pos;

			continue;

		}

		if (queue_list[queue_nr].write_pos == queue_list[queue_nr].read_pos)
			continue;

		if (queue_list[queue_nr].cb_proc)
			queue_list[queue_nr].cb_proc(&queue_list[queue_nr].info, queue_list[queue_nr].priv_data);

		//queue_list[queue_nr].read_pos = queue_list[queue_nr].write_pos;
		queue_list[queue_nr].irq_count = 0;

	}

}
*/
void avia_gt_dmx_set_pcr_pid(u8 enable, u16 pid)
{

	if (avia_gt_chip(ENX)) {

		enx_reg_16(PCR_PID) = ((!!enable) << 13) | pid;

		avia_gt_free_irq(ENX_IRQ_PCR);
		avia_gt_alloc_irq(ENX_IRQ_PCR, gtx_pcr_interrupt);			 // pcr reception

	} else if (avia_gt_chip(GTX)) {

		gtx_reg_16(PCRPID) = ((!!enable) << 13) | pid;

		avia_gt_free_irq(GTX_IRQ_PCR);
		avia_gt_alloc_irq(GTX_IRQ_PCR, gtx_pcr_interrupt);			 // pcr reception

	}

	avia_gt_dmx_force_discontinuity();

}

int avia_gt_dmx_set_pid_control_table(u8 queue_nr, u8 type, u8 fork, u8 cw_offset, u8 cc, u8 start_up, u8 pec, u8 filt_tab_idx, u8 _psh, u8 no_of_filter)
{

	sPID_Parsing_Control_Entry ppc_entry;
	u8 target_queue_nr;

	if (queue_nr > AVIA_GT_DMX_QUEUE_COUNT) {

		printk(KERN_CRIT "avia_gt_dmx: pid control table entry out of bounce (entry=%d)!\n", queue_nr);

		return -EINVAL;

	}

	dprintk(KERN_DEBUG "avia_gt_dmx_set_pid_control_table, entry %d, type %d, fork %d, cw_offset %d, cc %d, start_up %d, pec %d, filt_tab_idx %d, _psh %d\n",
		queue_nr,type,fork,cw_offset,cc,start_up,pec,filt_tab_idx,_psh);

	// Special case for SPTS audio queue
	if ((queue_nr == AVIA_GT_DMX_QUEUE_AUDIO) && (type == AVIA_GT_DMX_QUEUE_MODE_TS))
		target_queue_nr = AVIA_GT_DMX_QUEUE_VIDEO;
	else
		target_queue_nr = queue_nr;

	if (risc_mem_map->Version_no < 0xA000)
		target_queue_nr++;

	ppc_entry.type = type;
	ppc_entry.QID = target_queue_nr;
	ppc_entry.fork = !!fork;
	ppc_entry.CW_offset = cw_offset;
	ppc_entry.CC = cc;
	ppc_entry._PSH = _psh;
	ppc_entry.start_up = !!start_up;
	ppc_entry.PEC = !!pec;
	ppc_entry.filt_tab_idx = filt_tab_idx;
	ppc_entry.State = 0;
	ppc_entry.Reserved1 = 0;
	ppc_entry.no_of_filter = no_of_filter;
//FIXME	ppc_entry.State = 7;

	avia_gt_dmx_risc_write(&ppc_entry, &risc_mem_map->PID_Parsing_Control_Table[queue_nr], sizeof(ppc_entry));

	return 0;

}

int avia_gt_dmx_set_pid_table(u8 entry, u8 wait_pusi, u8 valid, u16 pid)
{

	sPID_Entry pid_entry;

	if (entry > 31) {

		printk(KERN_CRIT "avia_gt_dmx: pid search table entry out of bounce (entry=%d)!\n", entry);

		return -EINVAL;

	}

	dprintk(KERN_DEBUG "avia_gt_dmx_set_pid_table, entry %d, wait_pusi %d, valid %d, pid 0x%04X\n", entry, wait_pusi, valid, pid);

	pid_entry.wait_pusi = wait_pusi;
	pid_entry.VALID = !!valid;			// 0 = VALID, 1 = INVALID
	pid_entry.Reserved1 = 0;
	pid_entry.PID = pid;

	avia_gt_dmx_risc_write(&pid_entry, &risc_mem_map->PID_Search_Table[entry], sizeof(pid_entry));

	return 0;

}

void avia_gt_dmx_set_queue_irq(u8 queue_nr, u8 qim, u8 block)
{

	if (!qim)
		block = 0;

	queue_nr = avia_gt_dmx_map_queue(queue_nr);

	if (avia_gt_chip(ENX))
		enx_reg_16n(0x08C0 + queue_nr * 2) = ((qim << 15) | ((block & 0x1F) << 10));
	else if (avia_gt_chip(GTX))
		gtx_reg_16(QIn + queue_nr * 2) = ((qim << 15) | ((block & 0x1F) << 10));

}

u32 avia_gt_dmx_system_queue_get_read_pos(u8 queue_nr)
{

	u16 base = 0;
	u32 read_pos = 0xFFFFFFFF;
	u32 previous_read_pos;

	if ((queue_nr != AVIA_GT_DMX_QUEUE_VIDEO) &&
		(queue_nr != AVIA_GT_DMX_QUEUE_AUDIO) &&
		(queue_nr != AVIA_GT_DMX_QUEUE_TELETEXT)) {

		printk(KERN_CRIT "avia_gt_dmx: system_queue_get_read_pos: queue %d out of bounce\n", queue_nr);

		return 0;

	}

	if (avia_gt_chip(ENX))
		base = queue_system_map[queue_nr] * 8 + 0x8E0;
	else if (avia_gt_chip(GTX))
		base = queue_system_map[queue_nr] * 8 + 0x1E0;

    /*
     *
     * CAUTION: The correct sequence for accessing Queue
     * Pointer registers is as follows:
     * For reads,
     * Read low word
     * Read high word
     * CAUTION: Not following these sequences will yield
     * invalid data.
     *
     */

	do {

		previous_read_pos = read_pos;

		if (avia_gt_chip(ENX))
			read_pos = enx_reg_16n(base) | ((enx_reg_16n(base + 2) & 0x3F) << 16);
		else if (avia_gt_chip(GTX))
			read_pos = gtx_reg_16n(base) | ((gtx_reg_16n(base + 2) & 0x3F) << 16);

	} while (previous_read_pos != read_pos);
	
	if (read_pos > (queue_list[queue_nr].mem_addr + queue_list[queue_nr].size)) {
	
		printk(KERN_CRIT "avia_gt_dmx: system_queue_get_read_pos: queue %d read_pos 0x%X > queue_end 0x%X\n", queue_nr, read_pos, queue_list[queue_nr].mem_addr + queue_list[queue_nr].size);

		read_pos = queue_list[queue_nr].mem_addr;

	}

	if (read_pos < queue_list[queue_nr].mem_addr) {

		printk(KERN_CRIT "avia_gt_dmx: system_queue_get_read_pos: queue %d read_pos 0x%X < queue_base 0x%X\n", queue_nr, read_pos, queue_list[queue_nr].mem_addr);
		
		read_pos = queue_list[queue_nr].mem_addr;
	
	}

	return (read_pos - queue_list[queue_nr].mem_addr);

}


void avia_gt_dmx_system_queue_set_pos(u8 queue_nr, u32 read_pos, u32 write_pos)
{

	int	base;

	if ((queue_nr != AVIA_GT_DMX_QUEUE_VIDEO) &&
		(queue_nr != AVIA_GT_DMX_QUEUE_AUDIO) &&
		(queue_nr != AVIA_GT_DMX_QUEUE_TELETEXT)) {

		printk(KERN_CRIT "avia_gt_dmx: queue_system_set_pos: queue %d out of bounce\n", queue_nr);

		return;

	}
	
	if (avia_gt_chip(ENX)) {

		base = queue_system_map[queue_nr] * 8 + 0x8E0;

		enx_reg_16n(base + 0) = (queue_list[queue_nr].mem_addr + read_pos) & 0xFFFF;
		avia_gt_dmx_system_queue_set_write_pos(queue_nr, write_pos);
		enx_reg_16n(base + 2) = ((queue_list[queue_nr].mem_addr + read_pos) >> 16) & 63;

	} else if (avia_gt_chip(GTX)) {

		base = queue_system_map[queue_nr] * 8 + 0x1E0;

		gtx_reg_16n(base + 0) = (queue_list[queue_nr].mem_addr + read_pos) & 0xFFFF;
		avia_gt_dmx_system_queue_set_write_pos(queue_nr, write_pos);
		gtx_reg_16n(base + 2) = ((queue_list[queue_nr].mem_addr + read_pos) >> 16) & 63;

	}

}

void avia_gt_dmx_system_queue_set_read_pos(u8 queue_nr, u32 read_pos)
{

	int base;

	if ((queue_nr != AVIA_GT_DMX_QUEUE_VIDEO) &&
		(queue_nr != AVIA_GT_DMX_QUEUE_AUDIO) &&
		(queue_nr != AVIA_GT_DMX_QUEUE_TELETEXT)) {

		printk(KERN_CRIT "avia_gt_dmx: system_queue_set_read_pos: queue %d out of bounce\n", queue_nr);

		return;

	}

	if (avia_gt_chip(ENX)) {

		base = queue_system_map[queue_nr] * 8 + 0x8E0;

		enx_reg_16n(base) = (queue_list[queue_nr].mem_addr + read_pos) & 0xFFFF;
		enx_reg_16n(base + 2) = ((queue_list[queue_nr].mem_addr + read_pos) >> 16) & 63;

	} else if (avia_gt_chip(GTX)) {

		base = queue_system_map[queue_nr] * 8 + 0x1E0;

		gtx_reg_16n(base) = (queue_list[queue_nr].mem_addr + read_pos) & 0xFFFF;
		gtx_reg_16n(base + 2) = ((queue_list[queue_nr].mem_addr + read_pos) >> 16) & 63;

	}

}

void avia_gt_dmx_system_queue_set_write_pos(u8 queue_nr, u32 write_pos)
{

	int base;

	if ((queue_nr != AVIA_GT_DMX_QUEUE_VIDEO) &&
		(queue_nr != AVIA_GT_DMX_QUEUE_AUDIO) &&
		(queue_nr != AVIA_GT_DMX_QUEUE_TELETEXT)) {

		printk(KERN_CRIT "avia_gt_dmx: system_queue_set_write_pos: queue %d out of bounce\n", queue_nr);

		return;

	}

	if (avia_gt_chip(ENX)) {

		base = queue_system_map[queue_nr] * 8 + 0x8E0;

		enx_reg_16n(base + 4) = (queue_list[queue_nr].mem_addr + write_pos) & 0xFFFF;
		enx_reg_16n(base + 6) = (((queue_list[queue_nr].mem_addr + write_pos) >> 16) & 63) | (queue_size_table[queue_nr] << 6);

	} else if (avia_gt_chip(GTX)) {

		base = queue_system_map[queue_nr] * 8 + 0x1E0;

		gtx_reg_16n(base + 4) = (queue_list[queue_nr].mem_addr + write_pos) & 0xFFFF;
		gtx_reg_16n(base + 6) = (((queue_list[queue_nr].mem_addr + write_pos) >> 16) & 63) | (queue_size_table[queue_nr] << 6);

	}

}

void avia_gt_dmx_reset(unsigned char reenable)
{

	if (avia_gt_chip(ENX))
		enx_reg_set(RSTR0, TDMP, 1);
	else if (avia_gt_chip(GTX))
		gtx_reg_set(RR1, RISC, 1);

	if (reenable) {

		if (avia_gt_chip(ENX))
			enx_reg_set(RSTR0, TDMP, 0);
		else if (avia_gt_chip(GTX))
			gtx_reg_set(RR1, RISC, 0);

	}

}

int avia_gt_dmx_risc_init(void)
{

	avia_gt_dmx_reset(0);

	if (avia_gt_dmx_load_ucode()) {

		printk(KERN_ERR "avia_gt_dmx: No valid firmware found! TV mode disabled.\n");

		return 0;

	}

	if (avia_gt_chip(ENX)) {

		avia_gt_dmx_reset(1);
		enx_reg_16(EC) = 0;

	} else if (avia_gt_chip(GTX)) {

		avia_gt_dmx_reset(1);

	}

	return 0;

}

void avia_gt_dmx_risc_write(void *src, void *dst, u16 count)
{

	if ((((u32)dst) < ((u32)risc_mem_map)) || (((u32)dst) > (((u32)risc_mem_map) + sizeof(sRISC_MEM_MAP)))) {

		printk(KERN_CRIT "avia_gt_dmx: invalid risc write destination\n");

		return;

	}

	avia_gt_dmx_risc_write_offs(src, ((u32)dst) - ((u32)risc_mem_map), count);

}

void avia_gt_dmx_risc_write_offs(void *src, u16 offset, u16 count)
{

	u32 pos;
	//u32 flags;

	if (count & 1) {

		printk(KERN_CRIT "avia_gt_dmx: odd size risc write transfer detected\n");

		return;

	}

	if ((offset + count) > sizeof(sRISC_MEM_MAP)) {

		printk(KERN_CRIT "avia_gt_dmx: invalid risc write length detected\n");

		return;

	}

	//local_irq_save(flags);

	for (pos = 0; pos < count; pos += 2) {

		if (avia_gt_chip(ENX)) {

			if ((enx_reg_16n(TDP_INSTR_RAM + offset + pos)) != (((u16 *)src)[pos / 2])) {

				enx_reg_16n(TDP_INSTR_RAM + offset + pos) = ((u16 *)src)[pos / 2];

				mb();

			}

		} else if (avia_gt_chip(GTX)) {

			if ((gtx_reg_16n(GTX_REG_RISC + offset + pos)) != (((u16 *)src)[pos / 2])) {

				gtx_reg_16n(GTX_REG_RISC + offset + pos) = ((u16 *)src)[pos / 2];

				mb();

			}

		}

	}

	//local_irq_restore(flags);

}

void enx_tdp_stop(void)
{

	enx_reg_32(EC) = 2;			//stop tdp

}

// Ugly as hell - but who cares? :-)
#define MAKE_PCR(base2, base1, base0, extension) ((((u64)(extension)) << 50) | (((u64)(base2)) << 17) | (((u64)(base1)) << 1) | ((u64)(base0)))
#define PCR_BASE(pcr) ((pcr) & 0x1FFFFFFFF)
#define PCR_EXTENSION(pcr) ((pcr) >> 50)
#define PCR_VALUE(pcr) (PCR_BASE(pcr) * 300 + PCR_EXTENSION(pcr))

u64 avia_gt_dmx_get_transport_pcr(void)
{

	if (avia_gt_chip(ENX))
		return MAKE_PCR(enx_reg_s(TP_PCR_2)->PCR_Base, enx_reg_s(TP_PCR_1)->PCR_Base, enx_reg_s(TP_PCR_0)->PCR_Base, enx_reg_s(TP_PCR_0)->PCR_Extension);
	else if (avia_gt_chip(GTX))
		return MAKE_PCR(gtx_reg_s(PCR2)->PCR_Base, gtx_reg_s(PCR1)->PCR_Base, gtx_reg_s(PCR0)->PCR_Base, gtx_reg_s(PCR0)->PCR_Extension);

	return 0;

}

u64 avia_gt_dmx_get_latched_stc(void)
{

	if (avia_gt_chip(ENX))
		return MAKE_PCR(enx_reg_s(LC_STC_2)->Latched_STC_Base, enx_reg_s(LC_STC_1)->Latched_STC_Base, enx_reg_s(LC_STC_0)->Latched_STC_Base, enx_reg_s(LC_STC_0)->Latched_STC_Extension);
	else if (avia_gt_chip(GTX))
		return MAKE_PCR(gtx_reg_s(LSTC2)->Latched_STC_Base, gtx_reg_s(LSTC1)->Latched_STC_Base, gtx_reg_s(LSTC0)->Latched_STC_Base, gtx_reg_s(LSTC0)->Latched_STC_Extension);

	return 0;

}

u64 avia_gt_dmx_get_stc(void)
{

	if (avia_gt_chip(ENX))
		return MAKE_PCR(enx_reg_s(STC_COUNTER_2)->STC_Count, enx_reg_s(STC_COUNTER_1)->STC_Count, enx_reg_s(STC_COUNTER_0)->STC_Count, enx_reg_s(STC_COUNTER_0)->STC_Extension);
	else if (avia_gt_chip(GTX))
		return MAKE_PCR(gtx_reg_s(STCC2)->STC_Count, gtx_reg_s(STCC1)->STC_Count, gtx_reg_s(STCC0)->STC_Count, gtx_reg_s(STCC0)->STC_Extension);

	return 0;

}

static void avia_gt_dmx_set_dac(s16 pulse_count)
{

	if (avia_gt_chip(ENX)) {

		enx_reg_16(DAC_PC) = pulse_count;
		enx_reg_16(DAC_CP) = 9;

	} else if (avia_gt_chip(GTX)) {

		gtx_reg_32(DPCR) = (pulse_count << 16) | 9;
		
	}

}

static s16 gain = 0;
static s64 last_remote_diff = 0;

static void gtx_pcr_interrupt(unsigned short irq)
{

	u64 tp_pcr;
	u64 l_stc;
	u64 stc;

	s64 local_diff;
	s64 remote_diff;

	tp_pcr = avia_gt_dmx_get_transport_pcr();
	l_stc = avia_gt_dmx_get_latched_stc();
	stc = avia_gt_dmx_get_stc();

	if (force_stc_reload) {

		printk(KERN_INFO "avia_gt_dmx: reloading stc\n");

		avia_av_set_pcr(PCR_BASE(tp_pcr) >> 1, (PCR_BASE(tp_pcr) & 0x01) << 15);
		force_stc_reload = 0;

	}

	local_diff = (s64)PCR_VALUE(stc) - (s64)PCR_VALUE(l_stc);
	remote_diff = (s64)PCR_VALUE(tp_pcr) - (s64)PCR_VALUE(stc);

#define GAIN 25

	if (remote_diff > 0) {

		if (remote_diff > last_remote_diff)
			gain -= 2*GAIN;
		else
			gain += GAIN;

	} else if (remote_diff < 0) {

		if (remote_diff < last_remote_diff)
			gain += 2*GAIN;
		else
			gain -= GAIN;


	}

	avia_gt_dmx_set_dac(gain);

	last_remote_diff = remote_diff;

	dprintk(KERN_DEBUG "tp_pcr/stc/dir/diff: 0x%08x%08x/0x%08x%08x//%d\n", (u32)(PCR_VALUE(tp_pcr) >> 32), (u32)(PCR_VALUE(tp_pcr) & 0x0FFFFFFFF), (u32)(PCR_VALUE(stc) >> 32), (u32)(PCR_VALUE(stc) & 0x0FFFFFFFF), (s32)(remote_diff));

#if 0

	if ((remote_diff > TIME_THRESHOLD) || (remote_diff < -TIME_THRESHOLD)) {

		printk("avia_gt_dmx: stc out of sync!\n");
		avia_gt_dmx_force_discontinuity();

	}

#endif

}


void avia_gt_dmx_free_section_filter(u8 index)
{
	unsigned nr = index;
	
	if ( (index > 31) || (section_filter_umap[index] != index) )
	{
		printk(KERN_CRIT "avia_gt_dmx: trying to free section filters with wrong index!\n");
		return;
	}

	while (index < 32)
	{
		if (section_filter_umap[index] == nr)
		{
			section_filter_umap[index] = -1;
		}
		else
		{
			break;
		}
		index++;
	}
}

/*
 * Analyzes the section filters and convert them into an usable format.
 */

#define MAX_SECTION_FILTERS	16

int avia_gt_dmx_alloc_section_filter(void *f)
{
	u8 mask[MAX_SECTION_FILTERS][8];
	u8 value[MAX_SECTION_FILTERS][8];
	u8 mode[MAX_SECTION_FILTERS][8];
	s8 in_use[MAX_SECTION_FILTERS];
	u8 ver_not[MAX_SECTION_FILTERS];
	u8 unchanged[MAX_SECTION_FILTERS];
	u8 new_mask[8];
	u8 new_value[8];
	u8 new_valid;
	u8 not_value[MAX_SECTION_FILTERS];
	u8 not_mask[MAX_SECTION_FILTERS];
	u8 temp_mask;
	u8 and_or;
	u8 anz_not;
	u8 anz_mixed;
	u8 anz_normal;
	u8 compare_len;
	s8 different_bit_index;
	u8 xor = 0;
	u8 not_the_first = 0;
	u8 not_the_second = 0;
	struct dvb_demux_filter *filter = (struct dvb_demux_filter *) f;

	unsigned anz_filters = 0;
	unsigned old_anz_filters;
	unsigned i,j,k;
	signed entry;
	unsigned entry_len;
	signed new_entry;
	unsigned new_entry_len;
	sFilter_Parameter_Entry1 fpe1[MAX_SECTION_FILTERS];
	sFilter_Parameter_Entry2 fpe2[MAX_SECTION_FILTERS];
	sFilter_Parameter_Entry3 fpe3[MAX_SECTION_FILTERS];

	/*
	 * Copy and "normalize" the filters. The section-length cannot be filtered.
	 */

	if (!filter)
	{
		return -1;
	}

	while (filter && anz_filters < MAX_SECTION_FILTERS)
	{
		mask[anz_filters][0]  = filter->filter.filter_mask[0];
		value[anz_filters][0] = filter->filter.filter_value[0] & filter->filter.filter_mask[0];
		mode[anz_filters][0]  = filter->filter.filter_mode[0] | !filter->filter.filter_mask[0];

		if (mask[anz_filters][0])
		{
			in_use[anz_filters] = 0;
		}
		else
		{
			in_use[anz_filters] = -1;
		}

		for (i = 1; i < 8; i++)
		{
			mask[anz_filters][i]  = filter->filter.filter_mask[i+2];
			value[anz_filters][i] = filter->filter.filter_value[i+2] & filter->filter.filter_mask[i+2];
			mode[anz_filters][i]  = filter->filter.filter_mode[i+2] | !filter->filter.filter_mask[i+2];
			in_use[anz_filters] = mask[anz_filters][i] ? i : in_use[anz_filters];
		}

		unchanged[anz_filters] = 1;
		if ( (mode[anz_filters][3] != 0xFF) && ((mode[anz_filters][3] & mask[anz_filters][3]) == 0) )
		{
			ver_not[anz_filters] = 1;
			not_value[anz_filters] = value[anz_filters][3];
			not_mask[anz_filters] = mask[anz_filters][3];
		}
		else
		{
			ver_not[anz_filters] = 0;
		}

		/*
		 * Don't need to filter because one filter does contain a mask with
		 * all bits set to zero.
		 */

		if (in_use[anz_filters] == -1)
		{
			return -1;
		}

		anz_filters++;
		filter = filter->next;
	}

	if (filter)
	{
		printk(KERN_WARNING "too many section filters for hw-acceleration in this feed.\n");
		return -1;
	}

	i = anz_filters;
	while (i < MAX_SECTION_FILTERS)
	{
		in_use[i++] = -1;
	}

	/*
	 * Special case: only one section filter in this feed.
	 */

	if (anz_filters == 1)
	{

		/*
		 * Check wether we need a not filter
		 */

		anz_not = 0;
		anz_mixed = 0;
		anz_normal = 0;
		and_or = 0;	// AND

		for (i = 0; i < 8; i++)
		{
			if (mode[0][i] != 0xFF)
			{
				anz_not++;
				if (mode[0][i] & mask[0][i])
				{
					anz_mixed++;
				}
			}
			else if (mask[0][i])
			{
				anz_normal++;
			}
		}

		/*
		 * Only the byte with the version has a mode != 0xFF.
		 */
		if ( (anz_not == 1) && (anz_mixed == 0) && (mode[0][3] != 0xFF) )
		{
		}
		/*
		 * Mixed mode.
		 */
		else if ( (anz_not > 0) && (anz_normal > 0) )
		{
			anz_filters = 2;
			for (i = 0; i < 8; i++)
			{
				value[1][i] = value[0][i] & ~mode[0][i];
				mask[1][i] = ~mode[0][i];
				mask[0][i] = mask[0][i] & mode[0][i];
				value[0][i] = value[0][i] & mask[0][i];
				in_use[1] = in_use[0];
				not_the_second = 1;
			}
		}
		/*
		 * All relevant bits have mode-bit 0.
		 */
		else if (anz_not > 0)
		{
			not_the_first = 1;
		}
	}

	/*
	 * More than one filter
	 */

	else
	{
		and_or = 1;	// OR

		/*
		 * Cannot check for "mode-0" bits. Delete them from the mask.
		 */

		for (i = 0; i < anz_filters; i++)
		{
			in_use[i] = -1;
			for (j = 0; j < 8; j++)
			{
				mask[i][j] = mask[i][j] & mode[i][j];
				value[i][j] = value[i][j] & mask[i][j];
				if (mask[i][j])
				{
					in_use[i] = j;
				}
			}
			if (in_use[i] == -1)
			{
				return -1;	// We cannot filter. Damn, thats a really bad case.
			}
		}

		/*
		 * Eliminate redundant filters.
		 */

		old_anz_filters = anz_filters + 1;
		while (anz_filters != old_anz_filters)
		{
			old_anz_filters = anz_filters;
			for (i = 0; (i < MAX_SECTION_FILTERS - 1) && (anz_filters > 1); i++)
			{
				if (in_use[i] == -1)
				{
					continue;
				}
				for (j = i + 1; j < MAX_SECTION_FILTERS && (anz_filters > 1); j++)
				{
					if (in_use[j] == -1)
					{
						continue;
					}

					if (in_use[i] < in_use[j])
					{
						compare_len = in_use[i] + 1;
					}
					else
					{
						compare_len = in_use[j] + 1;
					}

					different_bit_index = -1;

					/*
					 * Check wether the filters are equal or different only in
					 * one bit.
					 */

					for (k = 0; k < compare_len; k++)
					{
						if ( (mask[i][k] == mask[j][k]) && (value[i][k] != value[j][k]) )
						{
							if (different_bit_index != -1)
							{
								goto next_check;
							}

							xor = value[i][k] ^ value[j][k];
							if (hweight8(xor) == 1)
							{
								different_bit_index = k;
							}
							else
							{
								goto next_check;
							}
						}
						else
						{
							goto next_check;
						}
					}

					if (different_bit_index != -1)
					{
						mask[i][different_bit_index] -= xor;
						value[i][different_bit_index] &= mask[i][different_bit_index];
						if (different_bit_index == in_use[i])
						{
							in_use[i] = -1;
							for (k = 0; k < compare_len; k++)
							{
								if (mask[i][k])
								{
									in_use[i] = k;
								}
							}
						}
						else
						{
							in_use[i] = compare_len - 1;
						}
						if (in_use[i] == -1)
						{
							return -1;	// Uups, eliminated all filters...
						}
					}
					else
					{
						in_use[i] = compare_len - 1;
					}

					if ( (not_value[i] != not_value[j]) || (not_mask[i] != not_mask[j]) || (ver_not[i] != ver_not[j]) )
					{
						unchanged[i] = 0;
					}
					k = compare_len;
					while (k < 8)
					{
						mask[i][k] = 0;
						value[i][k++] = 0;
					}
					in_use[j] = -1;
					anz_filters--;
					continue;


next_check:
					/*
					 * If mask1 has less bits set than mask2 and all bits set in mask1 are set in mask2, too, then
					 * they are redundant if the corresponding bits in both values are equal.
					 */

					new_valid = 1;
					memset(new_mask,0,sizeof(new_mask));
					memset(new_value,0,sizeof(new_value));
					for (k = 0; k < compare_len; k++)
					{
						temp_mask = mask[i][k] & mask[j][k];
						if ( ((temp_mask == mask[i][k]) ||
						      (temp_mask == mask[j][k]) ) &&
						      ( (value[i][k] & temp_mask) == (value[j][k] & temp_mask) ) )
						{
							new_mask[k] = temp_mask;
							new_value[k] = value[i][k] & temp_mask;
						}
						else
						{
							new_valid = 0;
							break;
						}
					}
					if (new_valid)
					{
						memcpy(mask[i],new_mask,8);
						memcpy(value[i],new_value,8);
						if ( (not_value[i] != not_value[j]) || (not_mask[i] != not_mask[j]) || (ver_not[i] != ver_not[j]) )
						{
							unchanged[i] = 0;
						}
						in_use[i] = compare_len - 1;
						in_use[j] = -1;
						anz_filters--;
						continue;
					}
				}
			}
		}
	}

	/*
	 * Now we have anz_filters filters in value and mask. Look for best space
	 * in the filter_param_table.
	 */

	i = 0;
	j = 0;
	entry_len = 33;
	entry = -1;
	new_entry_len = 33;
	new_entry = -1;

	while (i < 32)
	{
		if (section_filter_umap[i] == -1)
		{
			if (j == 1)
			{
				new_entry_len++;
			}
			else
			{
				if ( (new_entry_len < entry_len) && (new_entry_len >= anz_filters) )
				{
					entry_len = new_entry_len;
					entry = new_entry;
				}
				new_entry_len = 1;
				new_entry = i;
				j = 1;
			}
		}
		else
		{
			j = 0;
		}
		i++;
	}

	if ( ((entry == -1) && (new_entry != -1) && (new_entry_len >= anz_filters)) ||
	     ((new_entry_len < entry_len) && (new_entry_len >= anz_filters) ) )
	{
		entry = new_entry;
		entry_len = new_entry_len;
	}

	if (entry == -1)
	{
		return -1;
	}

	/*
	 * Mark filter_param_table as used.
	 */

	i = entry;
	while (i < entry + anz_filters)
	{
		section_filter_umap[i++] = entry;
	}

	/*
	 * Set filter_definition_table.
	 */

	filter_definition_table[entry].and_or_flag = and_or;
	avia_gt_dmx_risc_write(((u8*) filter_definition_table) + (entry & 0xFE), ((u8 *) (&risc_mem_map->Filter_Definition_Table)) + (entry & 0xFE), 2);

	/*
	 * Set filter parameter tables.
	 */

	i = 0;
	j = 0;
	while (j < anz_filters)
	{
		if (in_use[i] == -1)
		{
			i++;
			continue;
		}


		fpe1[j].mask_0  = mask[i][0];
		fpe1[j].param_0 = value[i][0];
		fpe1[j].mask_1  = mask[i][1];
		fpe1[j].param_1 = value[i][1];
		fpe1[j].mask_2  = mask[i][2];
		fpe1[j].param_2 = value[i][2];

		fpe2[j].mask_4  = mask[i][4];
		fpe2[j].param_4 = value[i][4];
		fpe2[j].mask_5  = mask[i][5];
		fpe2[j].param_5 = value[i][5];

		fpe3[j].mask_6  = mask[i][6];
		fpe3[j].param_6 = value[i][6];
		fpe3[j].mask_7  = mask[i][7];
		fpe3[j].param_7 = value[i][7];
		fpe3[j].Reserved1 = 0;
		fpe3[j].not_flag = 0;
		fpe3[j].Reserved2 = 0;

		if (unchanged[i] && ver_not[i])
		{
			fpe3[j].not_flag_ver_id_byte = 1;
			fpe2[j].mask_3 = not_mask[i];
			fpe2[j].param_3 = not_value[i];
		}
		else
		{
			fpe3[j].not_flag_ver_id_byte = 0;
			fpe2[j].mask_3  = mask[i][3];
			fpe2[j].param_3 = value[i][3];
		}

		fpe3[j].Reserved3 = 0;
		fpe3[j].Reserved4 = 0;

		j++;
		i++;
	}

	fpe3[0].not_flag = not_the_first;
	fpe3[1].not_flag = not_the_second;

	avia_gt_dmx_risc_write(fpe1,&risc_mem_map->Filter_Parameter_Table1[entry],anz_filters * sizeof(sFilter_Parameter_Entry1));
	avia_gt_dmx_risc_write(fpe2,&risc_mem_map->Filter_Parameter_Table2[entry],anz_filters * sizeof(sFilter_Parameter_Entry2));
	avia_gt_dmx_risc_write(fpe3,&risc_mem_map->Filter_Parameter_Table3[entry],anz_filters * sizeof(sFilter_Parameter_Entry3));

#if 0
	printk("I programmed following filters, And-Or: %d:\n",filter_definition_table[entry].and_or_flag);
	for (j = 0; j <anz_filters; j++)
	{
		printk("%d: %02X %02X %02X %02X %02X %02X %02X %02X\n",j,
			fpe1[j].param_0,fpe1[j].param_1,fpe1[j].param_2,
			fpe2[j].param_3,fpe2[j].param_4,fpe2[j].param_5,
			fpe3[j].param_6,fpe3[j].param_7);
		printk("   %02X %02X %02X %02X %02X %02X %02X %02X\n",
			fpe1[j].mask_0,fpe1[j].mask_1,fpe1[j].mask_2,
			fpe2[j].mask_3,fpe2[j].mask_4,fpe2[j].mask_5,
			fpe3[j].mask_6,fpe3[j].mask_7);
		printk("Not-Flag: %d, Not-Flag-Version: %d\n",fpe3[j].not_flag,fpe3[j].not_flag_ver_id_byte);
	}
#endif

	/*
	 * Tell caller the allocated filter_definition_table entry and the amount of used filters - 1.
	 */

	return entry | ((anz_filters - 1) << 8);

}

int __init avia_gt_dmx_init(void)
{

	int result;
	u32 queue_addr;
	u8 queue_nr;

	printk(KERN_INFO "avia_gt_dmx: $Id: avia_gt_dmx.c,v 1.169 2003/04/25 05:08:19 obi Exp $\n");;

	gt_info = avia_gt_get_info();

	if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {

		printk(KERN_ERR "avia_gt_dmx: Unsupported chip type\n");

		return -EIO;

	}

//	avia_gt_dmx_reset(1);


	if (avia_gt_chip(ENX)) {

		enx_reg_32(RSTR0)|=(1<<31)|(1<<23)|(1<<22);

		risc_mem_map = (sRISC_MEM_MAP *)enx_reg_o(TDP_INSTR_RAM);

	} else if (avia_gt_chip(GTX)) {

		risc_mem_map = (sRISC_MEM_MAP *)gtx_reg_o(GTX_REG_RISC);

	}

	if ((result = avia_gt_dmx_risc_init()))
		return result;

	if (avia_gt_chip(ENX)) {

		enx_reg_32(RSTR0) &= ~(1 << 27);
		enx_reg_32(RSTR0) &= ~(1 << 13);
		enx_reg_32(RSTR0) &= ~(1 << 11);
		enx_reg_32(RSTR0) &= ~(1 << 9);
		enx_reg_32(RSTR0) &= ~(1 << 23);
		enx_reg_32(RSTR0) &= ~(1 << 31);

		enx_reg_32(CFGR0) &= ~(1 << 3);
		enx_reg_32(CFGR0) &= ~(1 << 1);
		enx_reg_32(CFGR0) &= ~(1 << 0);

		enx_reg_16(FC) = 0x9147;
		enx_reg_16(SYNC_HYST) = 0x21;
		enx_reg_16(BQ) = 0x00BC;

		enx_reg_32(CFGR0) |= 1 << 24;		// enable dac output

		enx_reg_16(AVI_0) = 0xF;					// 0x6CF geht nicht (ordentlich)
		enx_reg_16(AVI_1) = 0xA;

		enx_reg_32(CFGR0) &= ~3;				// disable clip mode

	} else if (avia_gt_chip(GTX)) {

	//	rh(RR1)&=~0x1C;							 // take framer, ci, avi module out of reset
		gtx_reg_set(RR1, DAC, 1);
		gtx_reg_set(RR1, DAC, 0);

		gtx_reg_16(RR0) = 0;						// autsch, das muss so. kann das mal wer überprüfen?
		gtx_reg_16(RR1) = 0;
		gtx_reg_16(RISCCON) = 0;

		gtx_reg_16(FCR) = 0x9147;							 // byte wide input
		gtx_reg_16(SYNCH) = 0x21;

		gtx_reg_16(AVI) = 0x71F;
		gtx_reg_16(AVI+2) = 0xF;

	}

	memset(queue_list, 0, sizeof(queue_list));

	queue_addr = AVIA_GT_MEM_DMX_OFFS;

	for (queue_nr = 0; queue_nr < AVIA_GT_DMX_QUEUE_COUNT; queue_nr++) {

		queue_list[queue_nr].size = (1 << queue_size_table[queue_nr]) * 64;

		if (queue_addr & (queue_list[queue_nr].size - 1)) {

			printk(KERN_WARNING "avia_gt_dmx: warning, misaligned queue %d (is 0x%X, size %d), aligning...\n", queue_nr, queue_addr, queue_list[queue_nr].size);

			queue_addr += queue_list[queue_nr].size;
			queue_addr &= ~(queue_list[queue_nr].size - 1);

		}

		queue_list[queue_nr].mem_addr = queue_addr;
		queue_addr += queue_list[queue_nr].size;

		queue_list[queue_nr].pid = 0xFFFF;
		queue_list[queue_nr].running = 0;
		queue_list[queue_nr].info.index = queue_nr;
		queue_list[queue_nr].info.bytes_avail = avia_gt_dmx_queue_get_bytes_avail;
		queue_list[queue_nr].info.bytes_free = avia_gt_dmx_queue_get_bytes_free;
		queue_list[queue_nr].info.size = avia_gt_dmx_queue_get_size;
		queue_list[queue_nr].info.crc32 = avia_gt_dmx_queue_crc32;
		queue_list[queue_nr].info.get_buf1_ptr = avia_gt_dmx_queue_get_buf1_ptr;
		queue_list[queue_nr].info.get_buf2_ptr = avia_gt_dmx_queue_get_buf2_ptr;
		queue_list[queue_nr].info.get_buf1_size = avia_gt_dmx_queue_get_buf1_size;
		queue_list[queue_nr].info.get_buf2_size = avia_gt_dmx_queue_get_buf2_size;
		queue_list[queue_nr].info.get_data = avia_gt_dmx_queue_data_get;
		queue_list[queue_nr].info.get_data8 = avia_gt_dmx_queue_data_get8;
		queue_list[queue_nr].info.get_data16 = avia_gt_dmx_queue_data_get16;
		queue_list[queue_nr].info.get_data32 = avia_gt_dmx_queue_data_get32;
		queue_list[queue_nr].info.flush = avia_gt_dmx_queue_flush;
		queue_list[queue_nr].info.put_data = avia_gt_dmx_queue_data_put;

		if ((queue_nr == AVIA_GT_DMX_QUEUE_VIDEO) || (queue_nr == AVIA_GT_DMX_QUEUE_AUDIO) || (queue_nr == AVIA_GT_DMX_QUEUE_TELETEXT))
			avia_gt_dmx_system_queue_set_pos(queue_nr, 0, 0);

		avia_gt_dmx_queue_set_write_pos(queue_nr, 0);
		avia_gt_dmx_set_queue_irq(queue_nr, 0, 0);
		avia_gt_dmx_queue_irq_disable(queue_nr);

	}

	for (queue_nr = 0; queue_nr < 32; queue_nr++)
	{
		section_filter_umap[queue_nr] = -1;
		filter_definition_table[queue_nr].and_or_flag = 0;
		filter_definition_table[queue_nr].filter_param_id = queue_nr;
		filter_definition_table[queue_nr].Reserved = 0;
	}

	return 0;

}

void __exit avia_gt_dmx_exit(void)
{
	avia_gt_dmx_reset(0);
}

#if defined(STANDALONE)
module_init(avia_gt_dmx_init);
module_exit(avia_gt_dmx_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("AViA eNX/GTX demux driver");
MODULE_LICENSE("GPL");
#endif
MODULE_PARM(hw_sections, "i");
MODULE_PARM(ucode, "s");
MODULE_PARM_DESC(hw_sections, "0: software section filtering, 1: hardware section filtering");
MODULE_PARM_DESC(ucode, "path to risc microcode");

EXPORT_SYMBOL(avia_gt_dmx_force_discontinuity);
EXPORT_SYMBOL(avia_gt_dmx_alloc_queue_audio);
EXPORT_SYMBOL(avia_gt_dmx_alloc_queue_message);
EXPORT_SYMBOL(avia_gt_dmx_alloc_queue_teletext);
EXPORT_SYMBOL(avia_gt_dmx_alloc_queue_user);
EXPORT_SYMBOL(avia_gt_dmx_alloc_queue_video);
EXPORT_SYMBOL(avia_gt_dmx_fake_queue_irq);
EXPORT_SYMBOL(avia_gt_dmx_free_queue);
EXPORT_SYMBOL(avia_gt_dmx_get_queue_info);

EXPORT_SYMBOL(avia_gt_dmx_queue_get_bytes_free);
EXPORT_SYMBOL(avia_gt_dmx_queue_get_write_pos);
EXPORT_SYMBOL(avia_gt_dmx_queue_irq_disable);
EXPORT_SYMBOL(avia_gt_dmx_queue_irq_enable);
EXPORT_SYMBOL(avia_gt_dmx_queue_reset);
EXPORT_SYMBOL(avia_gt_dmx_queue_set_write_pos);
EXPORT_SYMBOL(avia_gt_dmx_queue_start);
EXPORT_SYMBOL(avia_gt_dmx_queue_stop);
EXPORT_SYMBOL(avia_gt_dmx_system_queue_set_pos);
EXPORT_SYMBOL(avia_gt_dmx_system_queue_set_read_pos);
EXPORT_SYMBOL(avia_gt_dmx_system_queue_set_write_pos);

EXPORT_SYMBOL(avia_gt_dmx_set_pcr_pid);
EXPORT_SYMBOL(avia_gt_dmx_set_pid_control_table);
EXPORT_SYMBOL(avia_gt_dmx_set_pid_table);
EXPORT_SYMBOL(avia_gt_dmx_get_hw_sec_filt_avail);

EXPORT_SYMBOL(avia_gt_dmx_free_section_filter);
EXPORT_SYMBOL(avia_gt_dmx_alloc_section_filter);
