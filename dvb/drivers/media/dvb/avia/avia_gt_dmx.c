/*
 * $Id: avia_gt_dmx.c,v 1.209 2004/06/24 00:35:39 carjay Exp $
 *
 * AViA eNX/GTX dmx driver (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
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
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/tqueue.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>

#include "demux.h"
#include "dvb_demux.h"

#include "avia_av.h"
#include "avia_gt.h"
#include "avia_gt_dmx.h"
#include "avia_gt_accel.h"
#include "avia_gt_napi.h"
#include "avia_gt_ucode.h"

static void avia_gt_dmx_bh_task(void *tl_data);
static void avia_gt_pcr_irq(unsigned short irq);

static int errno;
static sAviaGtInfo *gt_info;
static struct avia_gt_ucode_info *ucode_info;
static struct avia_gt_dmx_queue *msgqueue;
static int force_stc_reload;
static sAviaGtDmxQueue queue_list[AVIA_GT_DMX_QUEUE_COUNT];
static int hw_sections = 1;

/* video, audio, teletext can be mapped to one "interested" user queue */
static int queue_client[AVIA_GT_DMX_QUEUE_USER_START] = { -1, -1, -1 };

/* Sizes are (2 ^ n) * 64 bytes. Beware of the aligning! */
static u8 queue_size_table[AVIA_GT_DMX_QUEUE_COUNT] = {
	10,			/* video	*/
	10,			/* audio	*/
	9,			/* teletext	*/
	9, 10, 11, 10, 10,	/* user 3..7	*/
	8, 8, 8, 8, 8, 8, 8, 8,	/* user 8..15	*/
	8, 8, 8, 7, 7, 7, 7, 7,	/* user 16..23	*/
	7, 7, 7, 7, 7, 7, 7,	/* user 24..30	*/
	8			/* message	*/
};

static const u8 queue_system_map[AVIA_GT_DMX_QUEUE_USER_START] = { 2, 0, 1 };

static inline int avia_gt_dmx_queue_is_system_queue(u8 queue_nr)
{
	return queue_nr < AVIA_GT_DMX_QUEUE_USER_START;
}

/*
 * this function enables interrupts on system queues if additional clients are
 * interested in the same pid as a system queue or disables interrupts if the
 * clients are leaving.
 *
 * the following issues are still unresolved:
 * - currently only one additional client can get system queue data. think of
 *   recording a service containing teletext while vbi is enabled in the
 *   background and a software teletext decoder is running.
 * - if the driver operates in spts mode then recording the running video or
 *   audio packets separately is not possible because both pids are routed
 *   into the video queue. For now recording the video pid will deliver video
 *   and audio while recording the audio pid will not deliver data at all.
 *   This has to be filtered by the software demux.
 */
static void avia_gt_dmx_enable_disable_system_queue_irqs(void)
{
	/* video, audio, teletext */
	int new_queue_client[AVIA_GT_DMX_QUEUE_USER_START] = { -1, -1, -1 };
	u8 sys_queue_nr;
	u8 queue_nr;
	u32 write_pos;
	
	/* first scan all user queues for the PID/mode-combination filtered in the system queues */
	for (queue_nr = AVIA_GT_DMX_QUEUE_USER_START; queue_nr < AVIA_GT_DMX_QUEUE_COUNT; queue_nr++) {
		if (!queue_list[queue_nr].running)
			continue;
		for (sys_queue_nr = AVIA_GT_DMX_QUEUE_VIDEO; sys_queue_nr < AVIA_GT_DMX_QUEUE_USER_START; sys_queue_nr++)
			if ((new_queue_client[sys_queue_nr] == -1) &&
				(queue_list[sys_queue_nr].pid != 0xFFFF) &&
				(queue_list[sys_queue_nr].pid == queue_list[queue_nr].pid) &&
				(queue_list[sys_queue_nr].mode == queue_list[queue_nr].mode))
					new_queue_client[sys_queue_nr] = queue_nr;
	}

	/* second setup the queue_list with the retrieved information (1:1 mapping from system queue -> user queue) */
	for (sys_queue_nr = AVIA_GT_DMX_QUEUE_VIDEO; sys_queue_nr < AVIA_GT_DMX_QUEUE_USER_START; sys_queue_nr++) {
		if ((new_queue_client[sys_queue_nr] != -1) && (queue_client[sys_queue_nr] == -1)) {
			dprintk(KERN_INFO "avia_gt_dmx: client++ on queue %d (mode %d)\n", sys_queue_nr, queue_list[sys_queue_nr].mode);
			queue_client[sys_queue_nr] = new_queue_client[sys_queue_nr];
			write_pos = avia_gt_dmx_queue_get_write_pos(sys_queue_nr);
			queue_list[sys_queue_nr].hw_write_pos = write_pos;
			queue_list[sys_queue_nr].hw_read_pos = write_pos;
			queue_list[sys_queue_nr].write_pos = write_pos;
			queue_list[sys_queue_nr].read_pos = write_pos;
			queue_list[sys_queue_nr].overflow_count = 0;
			avia_gt_dmx_queue_irq_enable(sys_queue_nr);
		} else if ((new_queue_client[sys_queue_nr] == -1) && (queue_client[sys_queue_nr] != -1)) {
			dprintk(KERN_INFO "avia_gt_dmx: client-- on queue %d (mode %d)\n", sys_queue_nr, queue_list[sys_queue_nr].mode);
			avia_gt_dmx_queue_irq_disable(sys_queue_nr);
		}
		queue_client[sys_queue_nr] = new_queue_client[sys_queue_nr];
	}
}

static u8 avia_gt_dmx_map_queue(u8 queue_nr)
{
	avia_gt_reg_set(CFGR0, UPQ, queue_nr >= 16);

	mb();

	return queue_nr & 0x0f;
}

static void avia_gt_dmx_set_queue_irq(u8 queue_nr, u8 qim, u8 block)
{
	if (!qim)
		block = 0;

	queue_nr = avia_gt_dmx_map_queue(queue_nr);

	avia_gt_writew(QnINT + queue_nr * 2, (qim << 15) | ((block & 0x1F) << 10));
}

static struct avia_gt_dmx_queue *avia_gt_dmx_alloc_queue(u8 queue_nr, AviaGtDmxQueueProc *irq_proc, AviaGtDmxQueueProc *cb_proc, void *priv_data)
{
	sAviaGtDmxQueue *q;

	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_CRIT "avia_gt_dmx: alloc_queue: queue %d out of bounds\n", queue_nr);
		return NULL;
	}

	q = &queue_list[queue_nr];

	if (q->busy) {
		printk(KERN_ERR "avia_gt_dmx: alloc_queue: queue %d busy\n", queue_nr);
		return NULL;
	}

	q->busy = 1;
	q->cb_proc = cb_proc;
	q->hw_read_pos = 0;
	q->hw_write_pos = 0;
	q->irq_count = 0;
	q->irq_proc = irq_proc;
	q->priv_data = priv_data;
	q->qim_irq_count = 0;
	q->qim_jiffies = jiffies;
	q->qim_mode = 0;
	q->read_pos = 0;
	q->write_pos = 0;
	q->task_struct.routine = avia_gt_dmx_bh_task;
	q->task_struct.data = &q->info.index;

	avia_gt_dmx_queue_reset(queue_nr);
	avia_gt_dmx_set_queue_irq(queue_nr, 0, 0);

	return &q->info;
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

int avia_gt_dmx_free_queue(u8 queue_nr)
{
	sAviaGtDmxQueue *q;
	
	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_CRIT "avia_gt_dmx: free_queue: queue %d out of bounds\n", queue_nr);
		return -EINVAL;
	}

	q = &queue_list[queue_nr];

	if (!q->busy) {
		printk(KERN_ERR "avia_gt_dmx: free_queue: queue %d not busy\n", queue_nr);
		return -EFAULT;
	}

	avia_gt_dmx_queue_irq_disable(queue_nr);

	q->busy = 0;
	q->cb_proc = NULL;
	q->irq_count = 0;
	q->irq_proc = NULL;
	q->priv_data = NULL;

	return 0;
}

sAviaGtDmxQueue *avia_gt_dmx_get_queue_info(u8 queue_nr)
{
	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_CRIT "avia_gt_dmx: get_queue_info: queue %d out of bounds\n", queue_nr);
		return NULL;
	}

	return &queue_list[queue_nr];
}

u16 avia_gt_dmx_get_queue_irq(u8 queue_nr)
{
	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_CRIT "avia_gt_dmx: alloc_queue: queue %d out of bounds\n", queue_nr);
		return 0;
	}

	if (avia_gt_chip(ENX)) {
		if (queue_nr >= 17)
			return AVIA_GT_IRQ(3, queue_nr - 16);
		else if (queue_nr >= 2)
			return AVIA_GT_IRQ(4, queue_nr - 1);
		else
			return AVIA_GT_IRQ(5, queue_nr + 6);
	}
	else if (avia_gt_chip(GTX)) {
		return AVIA_GT_IRQ(2 + !!(queue_nr & 16), queue_nr & 15);
	}

	return 0;
}

void avia_gt_dmx_fake_queue_irq(u8 queue_nr)
{
	sAviaGtDmxQueue *q;

	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_CRIT "avia_gt_dmx: fake_queue_irq: queue %d out of bounds\n", queue_nr);
		return;
	}

	q = &queue_list[queue_nr];

	q->irq_count++;

	schedule_task(&q->task_struct);
}

static u32 avia_gt_dmx_queue_crc32(struct avia_gt_dmx_queue *queue, u32 count, u32 seed)
{
	sAviaGtDmxQueue *q;

	u32 chunk1_size;
	u32 crc;

	if (queue->index >= AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_CRIT "avia_gt_dmx: crc32: queue %d out of bounds\n", queue->index);
		return 0;
	}

	q = &queue_list[queue->index];

	if ((q->read_pos + count) > q->size) {
		chunk1_size = q->size - q->read_pos;
		// FIXME
		if ((avia_gt_chip(GTX)) && (chunk1_size <= 4))
			return 0;

		crc = avia_gt_accel_crc32(q->mem_addr + q->read_pos, chunk1_size, seed);

		return avia_gt_accel_crc32(q->mem_addr, count - chunk1_size, crc);
	}
	else {
		return avia_gt_accel_crc32(q->mem_addr + q->read_pos, count, seed);
	}
}

static u32 avia_gt_dmx_queue_data_get(struct avia_gt_dmx_queue *queue, void *dest, u32 count, u8 peek)
{
	sAviaGtDmxQueue *q;

	u32 bytes_avail = queue->bytes_avail(queue);
	u32 done = 0;
	u32 read_pos;

	if (queue->index >= AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_CRIT "avia_gt_dmx: queue_data_move: queue %d out of bounds\n", queue->index);
		return 0;
	}

	q = &queue_list[queue->index];

	if (count > bytes_avail) {
		printk(KERN_ERR "avia_gt_dmx: queue_data_move: %d bytes requested, %d available\n", count, bytes_avail);
		count = bytes_avail;
	}

	read_pos = q->read_pos;

	if ((read_pos > q->write_pos) && (count >= q->size - read_pos)) {
		done = q->size - read_pos;

		if (dest)
			memcpy(dest, gt_info->mem_addr + q->mem_addr + read_pos, done);

		read_pos = 0;
	}

	if ((dest) && (count - done))
		memcpy(((u8 *)dest) + done, gt_info->mem_addr + q->mem_addr + read_pos, count - done);

	if (!peek)
		q->read_pos = read_pos + count - done;

	return count;
}

static u8 avia_gt_dmx_queue_data_get8(struct avia_gt_dmx_queue *queue, u8 peek)
{
	u8 data;

	avia_gt_dmx_queue_data_get(queue, &data, sizeof(u8), peek);

	return data;
}

static u16 avia_gt_dmx_queue_data_get16(struct avia_gt_dmx_queue *queue, u8 peek)
{
	u16 data;

	avia_gt_dmx_queue_data_get(queue, &data, sizeof(u16), peek);

	return data;
}

static u32 avia_gt_dmx_queue_data_get32(struct avia_gt_dmx_queue *queue, u8 peek)
{
	u32 data;

	avia_gt_dmx_queue_data_get(queue, &data, sizeof(u32), peek);

	return data;
}

static u32 avia_gt_dmx_queue_data_put(struct avia_gt_dmx_queue *queue, const void *src, u32 count, u8 src_is_user_space)
{
	sAviaGtDmxQueue *q;

	u32 bytes_free = queue->bytes_free(queue);
	u32 done = 0;

	if (!avia_gt_dmx_queue_is_system_queue(queue->index)) {
		printk(KERN_CRIT "avia_gt_dmx: queue_data_put: queue %d out of bounds\n", queue->index);
		return 0;
	}

	q = &queue_list[queue->index];

	if (count > bytes_free) {
		printk(KERN_ERR "avia_gt_dmx: queue_data_put: %d bytes requested, %d available\n", count, bytes_free);
		count = bytes_free;
	}

	if (q->write_pos + count >= q->size) {
		done = q->size - q->write_pos;

		if (src_is_user_space)
			copy_from_user(gt_info->mem_addr + q->mem_addr + q->write_pos, src, done);
		else
			memcpy(gt_info->mem_addr + q->mem_addr + q->write_pos, src, done);

		q->write_pos = 0;
	}

	if (count - done) {
		if (src_is_user_space)
			copy_from_user(gt_info->mem_addr + q->mem_addr + q->write_pos, ((u8 *)src) + done, count - done);
		else
			memcpy(gt_info->mem_addr + q->mem_addr + q->write_pos, ((u8 *)src) + done, count - done);

		q->write_pos += count - done;
	}

	return count;
}

static u32 avia_gt_dmx_queue_get_buf1_ptr(struct avia_gt_dmx_queue *queue)
{
	sAviaGtDmxQueue *q = &queue_list[queue->index];

	return q->mem_addr + q->read_pos;
}

static u32 avia_gt_dmx_queue_get_buf2_ptr(struct avia_gt_dmx_queue *queue)
{
	sAviaGtDmxQueue *q = &queue_list[queue->index];

	if (q->write_pos >= q->read_pos)
		return 0;
	else
		return q->mem_addr;
}

static u32 avia_gt_dmx_queue_get_buf1_size(struct avia_gt_dmx_queue *queue)
{
	sAviaGtDmxQueue *q = &queue_list[queue->index];

	if (q->write_pos >= q->read_pos)
		return q->write_pos - q->read_pos;
	else
		return q->size - q->read_pos;
}

static u32 avia_gt_dmx_queue_get_buf2_size(struct avia_gt_dmx_queue *queue)
{
	sAviaGtDmxQueue *q = &queue_list[queue->index];

	if (q->write_pos >= q->read_pos)
		return 0;
	else
		return q->write_pos;
}

static u32 avia_gt_dmx_queue_get_bytes_avail(struct avia_gt_dmx_queue *queue)
{
	return avia_gt_dmx_queue_get_buf1_size(queue) + avia_gt_dmx_queue_get_buf2_size(queue);
}

static u32 avia_gt_dmx_queue_get_bytes_free(struct avia_gt_dmx_queue *queue)
{
	sAviaGtDmxQueue *q;

	if (!avia_gt_dmx_queue_is_system_queue(queue->index)) {
		printk(KERN_CRIT "avia_gt_dmx: queue_get_bytes_free: queue %d out of bounds\n", queue->index);
		return 0;
	}

	q = &queue_list[queue->index];

	q->hw_read_pos = avia_gt_dmx_system_queue_get_read_pos(queue->index);

	if (q->write_pos >= q->hw_read_pos)
		/*         free chunk1        busy chunk1        free chunk2   */
		/* queue [ FFFFFFFFFFF (RPOS) BBBBBBBBBBB (WPOS) FFFFFFFFFFF ] */
		return (q->size - q->write_pos) + q->hw_read_pos - 1;
	else
		/*         busy chunk1        free chunk1        busy chunk2   */
		/* queue [ BBBBBBBBBBB (WPOS) FFFFFFFFFFF (RPOS) BBBBBBBBBBB ] */
		return (q->hw_read_pos - q->write_pos) - 1;
}

static u32 avia_gt_dmx_queue_get_size(struct avia_gt_dmx_queue *queue)
{
	return queue_list[queue->index].size;
}

u32 avia_gt_dmx_queue_get_write_pos(u8 queue_nr)
{
	sAviaGtDmxQueue *q;

	u8 mapped_queue_nr;
	u32 previous_write_pos;
	u32 write_pos = 0xFFFFFFFF;

	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_CRIT "avia_gt_dmx: queue_get_write_pos: queue %d out of bounds\n", queue_nr);
		return 0;
	}

	q = &queue_list[queue_nr];

	mapped_queue_nr = avia_gt_dmx_map_queue(queue_nr);

	/*
	 * CAUTION: The correct sequence for accessing Queue
	 * Pointer registers is as follows:
	 * For reads,
	 * Read low word
	 * Read high word
	 * CAUTION: Not following these sequences will yield
	 * invalid data.
	 */

	do {
		previous_write_pos = write_pos;

		if (avia_gt_chip(ENX))
			write_pos = enx_reg_so(QWPnL, 4 * mapped_queue_nr)->QnWP | (enx_reg_so(QWPnH, 4 * mapped_queue_nr)->QnWP << 16);
		else if (avia_gt_chip(GTX))
			write_pos = gtx_reg_so(QWPnL, 4 * mapped_queue_nr)->QnWP | (gtx_reg_so(QWPnH, 4 * mapped_queue_nr)->QnWP << 16);
	}
	while (previous_write_pos != write_pos);

	if ((write_pos < q->mem_addr) || (write_pos >= q->mem_addr + q->size)) {
		printk(KERN_CRIT "avia_gt_dmx: queue %d hw_write_pos out of bounds! (B:0x%X/P:0x%X/E:0x%X)\n",
				queue_nr, q->mem_addr, write_pos, q->mem_addr + q->size);
#ifdef DEBUG
		BUG();
#endif
		q->hw_read_pos = 0;
		q->hw_write_pos = 0;
		q->read_pos = 0;
		q->write_pos = 0;

		avia_gt_dmx_queue_set_write_pos(queue_nr, 0);

		return 0;
	}

	return write_pos - q->mem_addr;
}

static void avia_gt_dmx_queue_flush(struct avia_gt_dmx_queue *queue)
{
	sAviaGtDmxQueue *q = &queue_list[queue->index];

	q->read_pos = q->write_pos;
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

static inline
u32 rnd_div(u32 a, u32 b)
{
	//return a / b;
	return (a + (b / 2)) / b;
}

static void avia_gt_dmx_queue_qim_mode_update(u8 queue_nr)
{
	sAviaGtDmxQueue *q = &queue_list[queue_nr];

	u8 block_pos = rnd_div(q->hw_write_pos * 16, q->size);

	avia_gt_dmx_set_queue_irq(queue_nr, 1, ((block_pos + 2) % 16) * 2);
}

static void avia_gt_dmx_queue_irq(unsigned short irq)
{
	sAviaGtDmxQueue *q;

	u8 bit = AVIA_GT_IRQ_BIT(irq);
	u8 nr = AVIA_GT_IRQ_REG(irq);
	u32 old_hw_write_pos;

	u8 queue_nr = AVIA_GT_DMX_QUEUE_COUNT;

	if (avia_gt_chip(ENX)) {
		if (nr == 3)
			queue_nr = bit + 16;
		else if (nr == 4)
			queue_nr = bit + 1;
		else if (nr == 5)
			queue_nr = bit - 6;
	}
	else if (avia_gt_chip(GTX)) {
		queue_nr = (nr - 2) * 16 + bit;
	}
	
	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_ERR "avia_gt_dmx: unexpected queue irq (nr=%d, bit=%d)\n", nr, bit);
		return;
	}

	q = &queue_list[queue_nr];

	if (!q->busy) {
		printk(KERN_ERR "avia_gt_dmx: irq on idle queue (queue_nr=%d)\n", queue_nr);
		return;
	}

	q->irq_count++;

	old_hw_write_pos = q->hw_write_pos;
	q->hw_write_pos = avia_gt_dmx_queue_get_write_pos(queue_nr);

	if (!q->qim_mode) {
		q->qim_irq_count++;

		/* We check every second whether irq load is too high */
		if (time_after(jiffies, q->qim_jiffies + HZ)) {
			if (q->qim_irq_count > 100) {
				dprintk(KERN_INFO "avia_gt_dmx: detected high irq load on queue %d - enabling qim mode\n", queue_nr);
				q->qim_mode = 1;
				avia_gt_dmx_queue_qim_mode_update(queue_nr);
			}

			q->qim_irq_count = 0;
			q->qim_jiffies = jiffies;
		}
	}
	else {
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

	/* We can't recover here or we will break queue handling (get_data*, bytes_avail, ...) */
	if (((q->read_pos < old_hw_write_pos) && ((q->read_pos <= q->hw_write_pos) && (q->hw_write_pos < old_hw_write_pos))) ||
		((old_hw_write_pos < q->read_pos) && ((q->read_pos <= q->hw_write_pos) || (q->hw_write_pos < old_hw_write_pos))))
			q->overflow_count++;

	if (q->irq_proc) {
		q->irq_proc(&q->info, q->priv_data);
	}
	else if (q->cb_proc) {
		queue_task(&q->task_struct, &tq_immediate);
		mark_bh(IMMEDIATE_BH);
	}
}

void avia_gt_dmx_queue_irq_disable(u8 queue_nr)
{
	sAviaGtDmxQueue *q = &queue_list[queue_nr];

	q->running = 0;

	if (!avia_gt_dmx_queue_is_system_queue(queue_nr))
		avia_gt_dmx_enable_disable_system_queue_irqs();

	avia_gt_free_irq(avia_gt_dmx_get_queue_irq(queue_nr));
}

int avia_gt_dmx_queue_irq_enable(u8 queue_nr)
{
	sAviaGtDmxQueue *q = &queue_list[queue_nr];

	q->running = 1;

	if (!avia_gt_dmx_queue_is_system_queue(queue_nr))
		avia_gt_dmx_enable_disable_system_queue_irqs();

	return avia_gt_alloc_irq(avia_gt_dmx_get_queue_irq(queue_nr), avia_gt_dmx_queue_irq);
}

int avia_gt_dmx_queue_reset(u8 queue_nr)
{
	sAviaGtDmxQueue *q;

	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_CRIT "avia_gt_dmx: queue_reset: queue %d out of bounds\n", queue_nr);
		return -EINVAL;
	}

	q = &queue_list[queue_nr];

	q->hw_read_pos = 0;
	q->hw_write_pos = 0;
	q->read_pos = 0;
	q->write_pos = 0;

	avia_gt_dmx_queue_set_write_pos(queue_nr, 0);

	if (avia_gt_dmx_queue_is_system_queue(queue_nr))
		avia_gt_dmx_system_queue_set_pos(queue_nr, 0, 0);

	return 0;
}

void avia_gt_dmx_queue_set_write_pos(u8 queue_nr, u32 write_pos)
{
	sAviaGtDmxQueue *q;

	u8 mapped_queue_nr;

	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_CRIT "avia_gt_dmx: set_queue queue (%d) out of bounds\n", queue_nr);
		return;
	}

	q = &queue_list[queue_nr];

	mapped_queue_nr = avia_gt_dmx_map_queue(queue_nr);

	write_pos += q->mem_addr;
	avia_gt_writew(QWPnL + 4 * mapped_queue_nr, write_pos & 0xFFFF);
	avia_gt_writew(QWPnH + 4 * mapped_queue_nr, ((write_pos >> 16) & 0x3F) | (queue_size_table[queue_nr] << 6));
}

int avia_gt_dmx_queue_start(u8 queue_nr,u8 qmode, u16 pid)
{
	sAviaGtDmxQueue *q;

	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_CRIT "avia_gt_dmx: queue_start queue (%d) out of bounds\n", queue_nr);
		return -EINVAL;
	}

	q = &queue_list[queue_nr];

	if (queue_nr==AVIA_GT_DMX_QUEUE_VIDEO) {
		if (qmode==TS) {		/* implies SPTS-Mode, increase queue size */
			queue_size_table[0] = 11;
			printk(KERN_INFO "SPTS, queue 0 extended.\n");
		} else {
			printk(KERN_INFO "PES, queue 0 normal.\n");
			queue_size_table[0] = 10;
		}
		q->size = (1 << queue_size_table[0]) * 64;
	}

	avia_gt_dmx_queue_reset(queue_nr);

	q->pid = pid;
	q->mode = qmode;

	if (avia_gt_dmx_queue_is_system_queue(queue_nr))
		avia_gt_dmx_enable_disable_system_queue_irqs();

	ucode_info->start_queue_feeds(queue_nr);
	return 0;
}

int avia_gt_dmx_queue_stop(u8 queue_nr)
{
	sAviaGtDmxQueue *q;

	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_CRIT "avia_gt_dmx: queue_stop queue (%d) out of bounds\n", queue_nr);
		return -EINVAL;
	}

	q = &queue_list[queue_nr];

	q->pid = 0xFFFF;

	avia_gt_dmx_queue_irq_disable(queue_nr);
	ucode_info->stop_queue_feeds(queue_nr,1);

	return 0;
}

static void avia_gt_dmx_bh_task(void *tl_data)
{
	u8 queue_nr = *(u8 *)tl_data;
	u8 sys_queue_nr;
	void *priv_data;
	AviaGtDmxQueueProc *cb_proc;
	u16 pid1 = 0xFFFF;
	u16 pid2;
	struct ts_header ts;
	u32 avail;
	sAviaGtDmxQueue *q;
	struct avia_gt_dmx_queue *queue_info;

	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_CRIT "avia_gt_dmx: bh_task queue (%d) out of bounds\n", queue_nr);
		return;
	}

	q = &queue_list[queue_nr];

	q->write_pos = q->hw_write_pos;

	if (q->overflow_count) {
		printk(KERN_WARNING "avia_gt_dmx: queue %d overflow (count: %d)\n", queue_nr, q->overflow_count);
		q->overflow_count = 0;
		q->read_pos = q->write_pos = q->hw_write_pos;
		return;
	}

	/* Message queue is special */
	if (queue_nr==AVIA_GT_DMX_QUEUE_MESSAGE){
		if (q->cb_proc)
			q->cb_proc(&q->info, q->priv_data);
		return;
	}

	queue_info = &q->info;

	/* Resync for TS-Queues */
	if (q->mode == TS) {
		if (queue_nr == AVIA_GT_DMX_QUEUE_VIDEO) {
			if (queue_list[AVIA_GT_DMX_QUEUE_VIDEO].pid != 0xFFFF) {
				pid1 = queue_list[AVIA_GT_DMX_QUEUE_VIDEO].pid;
				pid2 = pid1;
			}
			if (queue_list[AVIA_GT_DMX_QUEUE_AUDIO].pid != 0xFFFF) {
				pid2 = queue_list[AVIA_GT_DMX_QUEUE_AUDIO].pid;
				if (pid1 == 0xFFFF)
					pid1 = pid2;
			}
		}
		else
		{
			pid1 = queue_list[queue_nr].pid;
			pid2 = pid1;
		}

		if ( (avail = queue_info->bytes_avail(queue_info)) < 188 )
		{
			return;
		}

		queue_info->get_data(queue_info, &ts, 3, 1);

		/* Resynchronisation TS-queues */

		if ( (ts.sync_byte != 0x47) || ((ts.pid != pid1) && (ts.pid != pid2) ) )
		{
			while (avail >= 188)
			{
				if ((ts.sync_byte == 0x47) && ((ts.pid == pid1) || (ts.pid == pid2)) )
				{
					break;
				}
				queue_info->get_data(queue_info,NULL,1,0);
				avail--;
			}

			if (avail < 188)
			{
				return;
			}
		}
	}

	/* if we deal with a system queue, look up the "interested" user queue */
	for (sys_queue_nr = AVIA_GT_DMX_QUEUE_VIDEO; sys_queue_nr < AVIA_GT_DMX_QUEUE_USER_START; sys_queue_nr++) {
		if ((sys_queue_nr == queue_nr) && (queue_client[sys_queue_nr] != -1)) {
			cb_proc = queue_list[queue_client[sys_queue_nr]].cb_proc;
			priv_data = queue_list[queue_client[sys_queue_nr]].priv_data;
			break;
		}
	}

	if (sys_queue_nr == AVIA_GT_DMX_QUEUE_USER_START) {
		cb_proc = q->cb_proc;
		priv_data = q->priv_data;
	}

	if (cb_proc)
		cb_proc(queue_info, priv_data);

}

void avia_gt_dmx_set_pcr_pid(u8 enable, u16 pid)
{
	avia_gt_writew(PCR_PID, ((!!enable) << 13) | pid);

	avia_gt_free_irq(gt_info->irq_pcr);
	avia_gt_alloc_irq(gt_info->irq_pcr, avia_gt_pcr_irq);

	avia_gt_dmx_force_discontinuity();
}

u32 avia_gt_dmx_system_queue_get_read_pos(u8 queue_nr)
{
	sAviaGtDmxQueue *q;

	u16 base = 0;
	u32 read_pos = 0xFFFFFFFF;
	u32 previous_read_pos;

	if (!avia_gt_dmx_queue_is_system_queue(queue_nr)) {
		printk(KERN_CRIT "avia_gt_dmx: system_queue_get_read_pos: queue %d out of bounds\n", queue_nr);
		return 0;
	}

	q = &queue_list[queue_nr];

	base = gt_info->aq_rptr + (queue_system_map[queue_nr] * 8);

	/*
	 * CAUTION: The correct sequence for accessing Queue
	 * Pointer registers is as follows:
	 * For reads,
	 * Read low word
	 * Read high word
	 * CAUTION: Not following these sequences will yield
	 * invalid data.
	 */

	do {
		previous_read_pos = read_pos;
		read_pos = avia_gt_reg_16n(base);
		read_pos |= (avia_gt_reg_16n(base + 2) & 0x3F) << 16;
	}
	while (previous_read_pos != read_pos);

	if ((read_pos < q->mem_addr) || (read_pos >= q->mem_addr + q->size)) {
		printk(KERN_CRIT "avia_gt_dmx: system_queue_get_read_pos: queue %d read_pos 0x%X queue_base 0x%X queue_end 0x%X\n",
				queue_nr, read_pos, q->mem_addr, q->mem_addr + q->size);
		return 0;
	}

	return read_pos - q->mem_addr;
}

void avia_gt_dmx_system_queue_set_pos(u8 queue_nr, u32 read_pos, u32 write_pos)
{
	u16 base;

	if (!avia_gt_dmx_queue_is_system_queue(queue_nr)) {
		printk(KERN_CRIT "avia_gt_dmx: queue_system_set_pos: queue %d out of bounds\n", queue_nr);
		return;
	}

	read_pos += queue_list[queue_nr].mem_addr;

	base = gt_info->aq_rptr + (queue_system_map[queue_nr] * 8);
	avia_gt_reg_16n(base) = read_pos & 0xffff;
	avia_gt_dmx_system_queue_set_write_pos(queue_nr, write_pos);
	avia_gt_reg_16n(base + 2) = (read_pos >> 16) & 0x3f;
}

void avia_gt_dmx_system_queue_set_read_pos(u8 queue_nr, u32 read_pos)
{
	u16 base;

	if (!avia_gt_dmx_queue_is_system_queue(queue_nr)) {
		printk(KERN_CRIT "avia_gt_dmx: system_queue_set_read_pos: queue %d out of bounds\n", queue_nr);
		return;
	}

	read_pos += queue_list[queue_nr].mem_addr;

	base = gt_info->aq_rptr + (queue_system_map[queue_nr] * 8);
	avia_gt_reg_16n(base) = read_pos & 0xffff;
	avia_gt_reg_16n(base + 2) = (read_pos >> 16) & 0x3f;
}

void avia_gt_dmx_system_queue_set_write_pos(u8 queue_nr, u32 write_pos)
{
	u16 base;

	if (!avia_gt_dmx_queue_is_system_queue(queue_nr)) {
		printk(KERN_CRIT "avia_gt_dmx: system_queue_set_write_pos: queue %d out of bounds\n", queue_nr);
		return;
	}

	write_pos += queue_list[queue_nr].mem_addr;

	base = gt_info->aq_rptr + (queue_system_map[queue_nr] * 8);
	avia_gt_reg_16n(base + 4) = write_pos & 0xffff;
	avia_gt_reg_16n(base + 6) = ((write_pos >> 16) & 0x3f) | (queue_size_table[queue_nr] << 6);
}

/* ucode can't handle 2 active feeds with the same pid and doesn't start a feed
	in this case so we have to take care it does get started/stopped when a decoder
	filter is removed/set.
*/
void avia_gt_dmx_tap (u8 queue_nr, int start)
{
	/* decoder feed will stop, start tap feed */
	if (start){
		ucode_info->start_feed (queue_list[queue_nr].info.feed_idx, 1);
	} else {	/* decoder feed will take over, stop tap feed */
		ucode_info->stop_feed (queue_list[queue_nr].info.feed_idx, 0);
	}
}

static u64 avia_gt_dmx_get_pcr_base(void)
{
	u64 pcr2 = avia_gt_readw(TP_PCR_2);
	u64 pcr1 = avia_gt_readw(TP_PCR_1);
	u64 pcr0 = avia_gt_readw(TP_PCR_0);

	return (pcr2 << 17) | (pcr1 << 1) | (pcr0 >> 15);
}

static u64 avia_gt_dmx_get_latched_stc_base(void)
{
	u64 l_stc2 = avia_gt_readw(LC_STC_2);
	u64 l_stc1 = avia_gt_readw(LC_STC_1);
	u64 l_stc0 = avia_gt_readw(LC_STC_0);

	return (l_stc2 << 17) | (l_stc1 << 1) | (l_stc0 >> 15);
}

static u64 avia_gt_dmx_get_stc_base(void)
{
	u64 stc2 = avia_gt_readw(STC_COUNTER_2);
	u64 stc1 = avia_gt_readw(STC_COUNTER_1);
	u64 stc0 = avia_gt_readw(STC_COUNTER_0);

	return (stc2 << 17) | (stc1 << 1) | (stc0 >> 15);
}

int avia_gt_dmx_get_stc( struct dmx_demux* demux, unsigned int num,
			u64 *stc, unsigned int *base)
{
	if (num != 0)
		return -EINVAL;
	else
		*stc = avia_gt_dmx_get_latched_stc_base();

	return 0;
}

static void avia_gt_dmx_set_dac(s16 pulse_count)
{
	avia_gt_writel(DAC_PC, (pulse_count << 16) | 9);
}

#define MAX_PCR_DIFF	36000
#define MAX_DIFF_DIFF	4
#define FORCE_DIFF	80
#define MAX_DIFF	200
#define GAIN		50

static void avia_gt_pcr_irq(unsigned short irq)
{
	u64 pcr;
	u64 lstc;
	u64 stc;
	s32 pcr_lstc_diff;
	s32 diff_diff;
	s64 pcr_diff;
	enum {
		SLOW, OK, FAST
	}  direction;
	u8 gain_changed = 0;

	static u64 last_pcr = 0;
	static u64 last_lstc = 0;
	static s32 last_pcr_lstc_diff = 0;
	static s16 gain = 0;

	pcr = avia_gt_dmx_get_pcr_base();

	if (force_stc_reload) {
		stc = avia_gt_dmx_get_stc_base();
		avia_av_set_stc(stc >> 1,(stc & 1) << 15);
		force_stc_reload = 0;
		last_pcr = pcr;
		last_lstc = pcr;
		last_pcr_lstc_diff = 0;
		return;
	}

	/*
	 * Wenn die PCR-Differenz zu hoch ist, muß folgendes passiert sein:
	 *  - der Sender sendet die PCR-Pakete zu selten
	 *  - wir haben ein paar PCR-Pakete (Interrupts) übersehen (hallo printk)
	 *  - es gab eine PCR discontinuity
	 */

	if (pcr > last_pcr)
		pcr_diff = pcr - last_pcr;
	else
		pcr_diff = pcr + 0x200000000ULL - last_pcr;

	if (pcr_diff > MAX_PCR_DIFF) {
		printk(KERN_INFO "PCR discontinuity: PCR: 0x%01X%08X, OLDPCR: 0x%01X%08X, Diff: %d\n",
			(u32) (pcr >> 32),(u32) (pcr & 0xFFFFFFFF),
			(u32) (last_pcr >> 32), (u32) (last_pcr & 0xFFFFFFFF),
			(s32) pcr_diff);
		avia_gt_dmx_force_discontinuity();
		return;
	}

	/*
	 * Berechnen der Differenz zwischen STC und PCR. Dabei müssen die
	 * Überlaufe und ihre nicht festgelegte Reihenfolge von STC und PCR
	 * beachtet werden.
	 */

	lstc = avia_gt_dmx_get_latched_stc_base();

	if (pcr < last_pcr) {		/* Überlauf PCR */
		if (lstc < last_lstc) {	/* Überlauf STC */
			pcr_lstc_diff = lstc - pcr;
		}
		else {				/* kein Überlauf STC */
			pcr_lstc_diff = lstc - pcr - 0x200000000ULL;
		}
	}
	else {					/* kein Überlauf PCR */
		if (lstc < last_lstc) {	/* Überlauf STC */
			pcr_lstc_diff = lstc + 0x200000000ULL - pcr;
		}
		else {				/* kein Überlauf STC */
			pcr_lstc_diff = lstc - pcr;
		}
	}

	last_pcr = pcr;
	last_lstc = lstc;

	/*
	 * Sync lost ?
	 */

	if ((pcr_lstc_diff > MAX_DIFF) || (pcr_lstc_diff < -MAX_DIFF)) {
		avia_gt_dmx_force_discontinuity();
		return;
	}

	/*
	 * Feststellen, ob die STC schneller oder langsamer läuft.
	 * Absichtlich träge, da die CR eiert... (deshalb auch das Ignorieren der
	 * Extension, bringt eh' nix)
	 */

	diff_diff = pcr_lstc_diff - last_pcr_lstc_diff;

	if (diff_diff > MAX_DIFF_DIFF)
		direction = FAST;
	else if (diff_diff < -MAX_DIFF_DIFF)
		direction = SLOW;
	else
		direction = OK;

	/*
	 * Nachstellen der STC
	 */

	if (pcr_lstc_diff > FORCE_DIFF) {
		if (direction != SLOW) {
			gain += GAIN;
			gain_changed = 1;
			last_pcr_lstc_diff = pcr_lstc_diff;
		}
	}
	else if (pcr_lstc_diff < -FORCE_DIFF) {
		if (direction != FAST) {
			gain -= GAIN;
			gain_changed = 1;
			last_pcr_lstc_diff = pcr_lstc_diff;
		}
	}
	else if (direction == FAST) {
		gain += GAIN;
		gain_changed = 1;
		last_pcr_lstc_diff++;
	}
	else if (direction == SLOW) {
		gain -= GAIN;
		gain_changed = 1;
		last_pcr_lstc_diff--;
	}

	if (gain_changed) {
		dprintk(KERN_INFO "avia_gt_dmx: Diff: %d, Last-Diff: %d Direction: %d, Gain: %d\n",pcr_lstc_diff,last_pcr_lstc_diff,direction,gain);
		avia_gt_dmx_set_dac(gain);
	}
}

void avia_gt_dmx_force_discontinuity(void)
{
	force_stc_reload = 1;

	avia_gt_reg_set(FC, FD, 1);
}

static void avia_gt_dmx_enable_disable_framer(u8 enable)
{
	enable = !!enable;

	avia_gt_reg_set(FC, FE, enable);

	if (enable)
		avia_gt_reg_set(FC, FH, enable);
}

void avia_gt_dmx_enable_framer(void)
{
	return avia_gt_dmx_enable_disable_framer(1);
}

void avia_gt_dmx_disable_framer(void)
{
	return avia_gt_dmx_enable_disable_framer(0);
}

static int avia_gt_dmx_enable_disable_clip_mode(u8 queue_nr, u8 enable)
{
	enable = !!enable;

	switch (queue_nr) {
	case AVIA_GT_DMX_QUEUE_VIDEO:
		avia_gt_reg_set(CFGR0, VCP, enable);
		break;
	case AVIA_GT_DMX_QUEUE_AUDIO:
		avia_gt_reg_set(CFGR0, ACP, enable);
		break;
	case AVIA_GT_DMX_QUEUE_TELETEXT:
		avia_gt_reg_set(CFGR0, TCP, enable);
		break;
	case AVIA_GT_DMX_SYSTEM_QUEUES:
		avia_gt_reg_set(CFGR0, VCP, enable);
		avia_gt_reg_set(CFGR0, ACP, enable);
		avia_gt_reg_set(CFGR0, TCP, enable);
		break;
	default:
		return -EINVAL;
	}

	avia_gt_dmx_enable_disable_framer(!enable);
	return 0;
}

int avia_gt_dmx_enable_clip_mode(u8 queue_nr)
{
	return avia_gt_dmx_enable_disable_clip_mode(queue_nr, 1);
}

int avia_gt_dmx_disable_clip_mode(u8 queue_nr)
{
	return avia_gt_dmx_enable_disable_clip_mode(queue_nr, 0);
}

ssize_t avia_gt_dmx_queue_write(u8 queue_nr, const u8 *buf, size_t count, u32 nonblock)
{
	sAviaGtDmxQueue *q = avia_gt_dmx_get_queue_info(queue_nr);
	size_t todo = count, n;
	size_t queue_size;

	if (!avia_gt_dmx_queue_is_system_queue(queue_nr))
		return -EINVAL;

	if (!count)
		return 0;

	/* FIXME: do not wait for the whole queue to be empty */
	queue_size = q->info.size(&q->info) - 1;

	while (todo > 0) {
		n = min(todo, queue_size);

		while (q->info.bytes_free(&q->info) < n) {
			if (nonblock)
				return (count - todo) ? (count - todo) : -EAGAIN;
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(HZ / 100); /* 10 ms (TODO: optimize) */
		}

		q->info.put_data(&q->info, buf, n, 1);
		avia_gt_dmx_system_queue_set_write_pos(queue_nr, q->write_pos);

		todo -= n;
		buf += n;
	}

	return count - todo;
}

int avia_gt_dmx_queue_nr_get_bytes_free(u8 queue_nr)
{
	sAviaGtDmxQueue *q = avia_gt_dmx_get_queue_info(queue_nr);

	if (!q)
		return -EINVAL;

	return q->info.bytes_free(&q->info);
}

int __init avia_gt_dmx_init(void)
{
	sAviaGtDmxQueue *q;
	sAviaGtDmxRiscInit risc_init;
	int result;
	u32 queue_addr;
	u8 queue_nr;
	
	printk(KERN_INFO "avia_gt_dmx: $Id: avia_gt_dmx.c,v 1.209 2004/06/24 00:35:39 carjay Exp $\n");;

	gt_info = avia_gt_get_info();
	ucode_info = avia_gt_dmx_get_ucode_info();
	
	if (!avia_gt_supported_chipset(gt_info))
		return -ENODEV;

	if (avia_gt_chip(ENX))
		enx_reg_32(RSTR0) |= (1 << 31) | (1 << 23) | (1 << 22);

	memset (&risc_init,0,sizeof(sAviaGtDmxRiscInit));
	
	if (!hw_sections) 
		risc_init.ucode_flags|=DISABLE_UCODE_SECTION_FILTERING;
	
	if ((result = avia_gt_dmx_risc_init(&risc_init)))
		return result;

	if (avia_gt_chip(ENX)) {
		enx_reg_set(RSTR0, AVI, 0);
		enx_reg_set(RSTR0, QUE, 0);
		enx_reg_set(RSTR0, GFIX, 0);
		enx_reg_set(RSTR0, VDEO, 0);
		enx_reg_set(RSTR0, FRMR, 0);
		enx_reg_set(RSTR0, UART2, 0);
		enx_reg_set(RSTR0, PAR1284, 0);
		enx_reg_set(RSTR0, PCMA, 0);

		enx_reg_16(FC) = 0x1147;

		enx_reg_16(SYNC_HYST) = 0x21;
		enx_reg_16(BQ) = 0x00BC;

		enx_reg_set(RSTR0,FRCH, 0);
		enx_reg_16(FC) = 0x9147;

		/* enable dac output */
		enx_reg_32(CFGR0) |= 1 << 24;

		/* 0x6CF geht nicht (ordentlich) */
		enx_reg_16(AVI_0) = 0xF;
		enx_reg_16(AVI_1) = 0xA;

		/* disable clip mode */
		enx_reg_32(CFGR0) &= ~3;
	}
	else if (avia_gt_chip(GTX)) {
		// take framer, ci, avi module out of reset
		//rh(RR1)&=~0x1C;

		gtx_reg_set(RSTR0, DAC, 1);
		gtx_reg_set(RSTR0, DAC, 0);

		/* autsch, das muss so. kann das mal wer überprüfen? */
		gtx_reg_16(RSTR0) = 0;
		gtx_reg_16(RSTR1) = 0;
		gtx_reg_16(RISCCON) = 0;

		/* byte wide input */
		gtx_reg_16(FC) = 0x9147;
		gtx_reg_16(SYNCH) = 0x21;

		gtx_reg_16(AVI) = 0x71F;
		gtx_reg_16(AVI + 2) = 0xF;
	}

	memset(queue_list, 0, sizeof(queue_list));

	queue_addr = AVIA_GT_MEM_DMX_OFFS;

	for (queue_nr = 0; queue_nr < AVIA_GT_DMX_QUEUE_COUNT; queue_nr++) {
		q = &queue_list[queue_nr];
		q->size = (1 << queue_size_table[queue_nr]) * 64;

		if (queue_addr & (q->size - 1)) {
			printk(KERN_WARNING "avia_gt_dmx: warning, misaligned queue %d (is 0x%X, size %d), aligning...\n",
					queue_nr, queue_addr, q->size);
			queue_addr += q->size;
			queue_addr &= ~(q->size - 1);
		}

		if (queue_addr + q->size > AVIA_GT_MEM_DMX_OFFS + AVIA_GT_MEM_DMX_SIZE) {
			printk(KERN_CRIT "avia_gt_dmx: alert! queue %d (0x%X, size %d) is not inside demux memory boundaries!\n"
					"avia_gt_dmx: using this queue will crash the system!\n",
					queue_nr, queue_addr, q->size);
		}

		q->mem_addr = queue_addr;
		queue_addr += q->size;

		q->pid = 0xFFFF;
		q->running = 0;
		q->info.index = queue_nr;
		q->info.bytes_avail = avia_gt_dmx_queue_get_bytes_avail;
		q->info.bytes_free = avia_gt_dmx_queue_get_bytes_free;
		q->info.size = avia_gt_dmx_queue_get_size;
		q->info.crc32_be = avia_gt_dmx_queue_crc32;
		q->info.get_buf1_ptr = avia_gt_dmx_queue_get_buf1_ptr;
		q->info.get_buf2_ptr = avia_gt_dmx_queue_get_buf2_ptr;
		q->info.get_buf1_size = avia_gt_dmx_queue_get_buf1_size;
		q->info.get_buf2_size = avia_gt_dmx_queue_get_buf2_size;
		q->info.get_data = avia_gt_dmx_queue_data_get;
		q->info.get_data8 = avia_gt_dmx_queue_data_get8;
		q->info.get_data16 = avia_gt_dmx_queue_data_get16;
		q->info.get_data32 = avia_gt_dmx_queue_data_get32;
		q->info.flush = avia_gt_dmx_queue_flush;
		q->info.put_data = avia_gt_dmx_queue_data_put;

		if (avia_gt_dmx_queue_is_system_queue(queue_nr))
			avia_gt_dmx_system_queue_set_pos(queue_nr, 0, 0);

		avia_gt_dmx_queue_set_write_pos(queue_nr, 0);
		avia_gt_dmx_set_queue_irq(queue_nr, 0, 0);
		avia_gt_dmx_queue_irq_disable(queue_nr);
	}
	if (ucode_info->caps&AVIA_GT_UCODE_CAP_MSGQ){
		if (!(msgqueue=avia_gt_dmx_alloc_queue_message(NULL, ucode_info->handle_msgq, NULL))){
			printk (KERN_ERR "Could not allocate message queue\n");
		} else {
			avia_gt_dmx_queue_irq_enable(msgqueue->index);
		}
	}

	return 0;
}

void __exit avia_gt_dmx_exit(void)
{
	if (msgqueue) avia_gt_dmx_free_queue(msgqueue->index);
	avia_gt_dmx_risc_reset(0);
}

#if defined(STANDALONE)
module_init(avia_gt_dmx_init);
module_exit(avia_gt_dmx_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("AViA eNX/GTX demux driver");
MODULE_LICENSE("GPL");
#endif
MODULE_PARM(ucode, "s");
MODULE_PARM_DESC(ucode, "path to risc microcode");
MODULE_PARM(hw_sections, "i");
MODULE_PARM_DESC(hw_sections, "hw_sections: 0=disabled, 1=enabled if possible (default)");

EXPORT_SYMBOL(avia_gt_dmx_alloc_queue_audio);
EXPORT_SYMBOL(avia_gt_dmx_alloc_queue_message);
EXPORT_SYMBOL(avia_gt_dmx_alloc_queue_teletext);
EXPORT_SYMBOL(avia_gt_dmx_alloc_queue_user);
EXPORT_SYMBOL(avia_gt_dmx_alloc_queue_video);
EXPORT_SYMBOL(avia_gt_dmx_free_queue);
EXPORT_SYMBOL(avia_gt_dmx_get_queue_info);
EXPORT_SYMBOL(avia_gt_dmx_fake_queue_irq);
EXPORT_SYMBOL(avia_gt_dmx_queue_get_write_pos);
EXPORT_SYMBOL(avia_gt_dmx_queue_irq_disable);
EXPORT_SYMBOL(avia_gt_dmx_queue_irq_enable);
EXPORT_SYMBOL(avia_gt_dmx_queue_reset);
EXPORT_SYMBOL(avia_gt_dmx_queue_set_write_pos);
EXPORT_SYMBOL(avia_gt_dmx_queue_start);
EXPORT_SYMBOL(avia_gt_dmx_queue_stop);
EXPORT_SYMBOL(avia_gt_dmx_set_pcr_pid);

EXPORT_SYMBOL(avia_gt_dmx_system_queue_set_pos);
EXPORT_SYMBOL(avia_gt_dmx_system_queue_set_read_pos);
EXPORT_SYMBOL(avia_gt_dmx_system_queue_set_write_pos);
EXPORT_SYMBOL(avia_gt_dmx_tap);
EXPORT_SYMBOL(avia_gt_dmx_force_discontinuity);
EXPORT_SYMBOL(avia_gt_dmx_enable_framer);
EXPORT_SYMBOL(avia_gt_dmx_disable_framer);
EXPORT_SYMBOL(avia_gt_dmx_enable_clip_mode);
EXPORT_SYMBOL(avia_gt_dmx_disable_clip_mode);
EXPORT_SYMBOL(avia_gt_dmx_queue_write);
EXPORT_SYMBOL(avia_gt_dmx_queue_nr_get_bytes_free);
EXPORT_SYMBOL(avia_gt_dmx_get_ucode_info);
EXPORT_SYMBOL(avia_gt_dmx_set_ucode_info);
EXPORT_SYMBOL(avia_gt_dmx_get_stc);
