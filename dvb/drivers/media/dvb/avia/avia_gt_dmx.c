/*
 * $Id: avia_gt_dmx.c,v 1.190 2003/11/12 22:07:44 sepp776 Exp $
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
#include <linux/fcntl.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/tqueue.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>

#include "../dvb-core/demux.h"
#include "../dvb-core/dvb_demux.h"
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
static volatile sRISC_MEM_MAP *risc_mem_map;
static volatile u16 *riscram;
static volatile u16 *pst;
static volatile u16 *ppct; 
static char *ucode;
static int force_stc_reload;
static sAviaGtDmxQueue queue_list[AVIA_GT_DMX_QUEUE_COUNT];
static s8 section_filter_umap[32];
static sFilter_Definition_Entry filter_definition_table[32];
static struct avia_gt_ucode_info ucode_info;

/* video, audio, teletext */
static int queue_client[AVIA_GT_DMX_QUEUE_USER_START] = { -1, -1, -1 };

/* Sizes are (2 ^ n) * 64 bytes. Beware of the aligning! */
static const u8 queue_size_table[AVIA_GT_DMX_QUEUE_COUNT] = {
	10,			/* video	*/
	9,			/* audio	*/
	9,			/* teletext	*/
	10, 10, 10, 10, 10,	/* user 3..7	*/
	8, 8, 8, 8, 8, 8, 8, 8,	/* user 8..15	*/
	8, 8, 8, 8, 7, 7, 7, 7,	/* user 16..23	*/
	7, 7, 7, 7, 7, 7, 7,	/* user 24..30	*/
	7			/* message	*/
};

static const u8 queue_system_map[AVIA_GT_DMX_QUEUE_USER_START] = { 2, 0, 1 };

static inline
int avia_gt_dmx_queue_is_system_queue(u8 queue_nr)
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
 *   into the video queue. for now recording the video pid will deliver video
 *   and audio while recording the audio pid will not deliver data at all.
 *   this has to be filtered by the software demux.
 */
static
void avia_gt_dmx_enable_disable_system_queue_irqs(void)
{
	/* video, audio, teletext */
	int new_queue_client[AVIA_GT_DMX_QUEUE_USER_START] = { -1, -1, -1 };
	u8 sys_queue_nr;
	u8 queue_nr;
	u32 write_pos;

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

	for (sys_queue_nr = AVIA_GT_DMX_QUEUE_VIDEO; sys_queue_nr < AVIA_GT_DMX_QUEUE_USER_START; sys_queue_nr++) {
		if ((new_queue_client[sys_queue_nr] != -1) && (queue_client[sys_queue_nr] == -1)) {
			printk(KERN_INFO "avia_gt_dmx: client++ on queue %d (mode %d)\n", sys_queue_nr, queue_list[sys_queue_nr].mode);
			queue_client[sys_queue_nr] = new_queue_client[sys_queue_nr];
			write_pos = avia_gt_dmx_queue_get_write_pos(sys_queue_nr);
			queue_list[sys_queue_nr].hw_write_pos = write_pos;
			queue_list[sys_queue_nr].hw_read_pos = write_pos;
			queue_list[sys_queue_nr].write_pos = write_pos;
			queue_list[sys_queue_nr].read_pos = write_pos;
			queue_list[sys_queue_nr].overflow_count = 0;
			avia_gt_dmx_queue_irq_enable(sys_queue_nr);
		}
		else if ((new_queue_client[sys_queue_nr] == -1) && (queue_client[sys_queue_nr] != -1)) {
			printk(KERN_INFO "avia_gt_dmx: client-- on queue %d (mode %d)\n", sys_queue_nr, queue_list[sys_queue_nr].mode);
			avia_gt_dmx_queue_irq_disable(sys_queue_nr);
		}

		queue_client[sys_queue_nr] = new_queue_client[sys_queue_nr];
	}
}

static
u8 avia_gt_dmx_map_queue(u8 queue_nr)
{
	avia_gt_reg_set(CFGR0, UPQ, queue_nr >= 16);

	mb();

	return queue_nr & 0x0f;
}

static
void avia_gt_dmx_set_queue_irq(u8 queue_nr, u8 qim, u8 block)
{
	if (!qim)
		block = 0;

	queue_nr = avia_gt_dmx_map_queue(queue_nr);

	avia_gt_writew(QnINT + queue_nr * 2, (qim << 15) | ((block & 0x1F) << 10));
}

static
struct avia_gt_dmx_queue *avia_gt_dmx_alloc_queue(u8 queue_nr, AviaGtDmxQueueProc *irq_proc, AviaGtDmxQueueProc *cb_proc, void *priv_data)
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
	q->info.hw_sec_index = -1;
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

static
void avia_gt_dmx_memcpy16(volatile u16 *dest, const u16 *src, size_t n)
{
	while (n--) {
		*dest++ = *src++;
		mb();
	}
}

static
void avia_gt_dmx_memset16(volatile u16 *s, const u16 c, size_t n)
{
	while (n--) {
		*s++ = c;
		mb();
	}
}

static
ssize_t avia_gt_dmx_risc_write(volatile void *dest, const void *src, size_t n)
{
	if (((u16 *)dest < riscram) || (&((u16 *)dest)[n >> 1] > &riscram[DMX_RISC_RAM_SIZE])) {
		printk(KERN_CRIT "avia_gt_dmx: invalid risc write destination\n");
		return -EINVAL;
	}

	if (n & 1) {
		printk(KERN_CRIT "avia_gt_dmx: odd size risc writes are not allowed\n");
		return -EINVAL;
	}

	avia_gt_dmx_memset16(dest, 0, n >> 1);
	avia_gt_dmx_memcpy16(dest, src, n >> 1);

	return n;
}

static
void avia_gt_dmx_load_ucode(void)
{
	int fd;
	mm_segment_t fs;
	u8 ucode_fs_buf[2048];
	u16 *ucode_buf = NULL;
	loff_t file_size;
	u32 flags;

	fs = get_fs();
	set_fs(get_ds());

	if ((ucode) && ((fd = open(ucode, 0, 0)) >= 0)) {
		file_size = lseek(fd, 0, 2);
		lseek(fd, 0, 0);

		if ((file_size <= 0) || (file_size > 2048))
			printk(KERN_ERR "avia_gt_dmx: Firmware wrong size '%s'\n", ucode);
		else if (read(fd, ucode_fs_buf, file_size) != file_size)
			printk(KERN_ERR "avia_gt_dmx: Failed to read firmware file '%s'\n", ucode);
		else
			ucode_buf = (u16 *)ucode_fs_buf;

		close(fd);

		/* queues should be stopped */
		if ((ucode_buf) && (file_size >= 0x740))
			for (fd = DMX_PID_SEARCH_TABLE; fd < DMX_PID_PARSING_CONTROL_TABLE; fd++)
				ucode_buf[fd] = 0xdfff;
	}

	set_fs(fs);

	/* use internal ucode if loading failed for any reason */
	if (!ucode_buf) {
		ucode_buf = (u16 *)avia_gt_dmx_ucode_img;
		file_size = avia_gt_dmx_ucode_size;
	}

	local_irq_save(flags);
	avia_gt_dmx_risc_write(risc_mem_map, ucode_buf, file_size);
	local_irq_restore(flags);

	printk(KERN_INFO "avia_gt_dmx: loaded ucode v%04X\n", riscram[DMX_VERSION_NO]);
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

static
u32 avia_gt_dmx_queue_crc32(struct avia_gt_dmx_queue *queue, u32 count, u32 seed)
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

static
u32 avia_gt_dmx_queue_data_get(struct avia_gt_dmx_queue *queue, void *dest, u32 count, u8 peek)
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

static
u8 avia_gt_dmx_queue_data_get8(struct avia_gt_dmx_queue *queue, u8 peek)
{
	u8 data;

	avia_gt_dmx_queue_data_get(queue, &data, sizeof(u8), peek);

	return data;
}

static
u16 avia_gt_dmx_queue_data_get16(struct avia_gt_dmx_queue *queue, u8 peek)
{
	u16 data;

	avia_gt_dmx_queue_data_get(queue, &data, sizeof(u16), peek);

	return data;
}

static
u32 avia_gt_dmx_queue_data_get32(struct avia_gt_dmx_queue *queue, u8 peek)
{
	u32 data;

	avia_gt_dmx_queue_data_get(queue, &data, sizeof(u32), peek);

	return data;
}

static
u32 avia_gt_dmx_queue_data_put(struct avia_gt_dmx_queue *queue, const void *src, u32 count, u8 src_is_user_space)
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

static
u32 avia_gt_dmx_queue_get_buf1_ptr(struct avia_gt_dmx_queue *queue)
{
	sAviaGtDmxQueue *q = &queue_list[queue->index];

	return q->mem_addr + q->read_pos;
}

static
u32 avia_gt_dmx_queue_get_buf2_ptr(struct avia_gt_dmx_queue *queue)
{
	sAviaGtDmxQueue *q = &queue_list[queue->index];

	if (q->write_pos >= q->read_pos)
		return 0;
	else
		return q->mem_addr;
}

static
u32 avia_gt_dmx_queue_get_buf1_size(struct avia_gt_dmx_queue *queue)
{
	sAviaGtDmxQueue *q = &queue_list[queue->index];

	if (q->write_pos >= q->read_pos)
		return q->write_pos - q->read_pos;
	else
		return q->size - q->read_pos;
}

static
u32 avia_gt_dmx_queue_get_buf2_size(struct avia_gt_dmx_queue *queue)
{
	sAviaGtDmxQueue *q = &queue_list[queue->index];

	if (q->write_pos >= q->read_pos)
		return 0;
	else
		return q->write_pos;
}

static
u32 avia_gt_dmx_queue_get_bytes_avail(struct avia_gt_dmx_queue *queue)
{
	return avia_gt_dmx_queue_get_buf1_size(queue) + avia_gt_dmx_queue_get_buf2_size(queue);
}

static
u32 avia_gt_dmx_queue_get_bytes_free(struct avia_gt_dmx_queue *queue)
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

static
u32 avia_gt_dmx_queue_get_size(struct avia_gt_dmx_queue *queue)
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

static
void avia_gt_dmx_queue_flush(struct avia_gt_dmx_queue *queue)
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

static
void avia_gt_dmx_queue_qim_mode_update(u8 queue_nr)
{
	sAviaGtDmxQueue *q = &queue_list[queue_nr];

	u8 block_pos = rnd_div(q->hw_write_pos * 16, q->size);

	avia_gt_dmx_set_queue_irq(queue_nr, 1, ((block_pos + 2) % 16) * 2);
}

static
void avia_gt_dmx_queue_irq(unsigned short irq)
{
	sAviaGtDmxQueue *q;

	u8 bit = AVIA_GT_IRQ_BIT(irq);
	u8 nr = AVIA_GT_IRQ_REG(irq);
	u32 old_hw_write_pos,written,too_much;

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

	/*
	 * Deliver only complete ts-packets. Otherwise, /dev/dvr gets confused.
	 */

	if (q->mode == AVIA_GT_DMX_QUEUE_MODE_TS)
	{
		if (old_hw_write_pos < q->hw_write_pos)
		{
			written = q->hw_write_pos - old_hw_write_pos;
		}
		else
		{
			written = q->hw_write_pos + q->size - old_hw_write_pos;
		}
		too_much = written % 188;
		if (too_much > q->hw_write_pos)
		{
			q->hw_write_pos = q->size + q->hw_write_pos - too_much;
		}
		else
		{
			q->hw_write_pos -= too_much;
		}
	}

	if (!q->qim_mode) {
		q->qim_irq_count++;

		/* We check every secord wether irq load is too high */
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

int avia_gt_dmx_queue_start(u8 queue_nr, u8 mode, u16 pid, u8 wait_pusi, u8 filt_tab_idx, u8 no_of_filter)
{
	sAviaGtDmxQueue *q;

	if (queue_nr >= AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_CRIT "avia_gt_dmx: queue_start queue (%d) out of bounds\n", queue_nr);
		return -EINVAL;
	}

	q = &queue_list[queue_nr];

	avia_gt_dmx_queue_reset(queue_nr);

	q->pid = pid;
	q->mode = mode;

	if (avia_gt_dmx_queue_is_system_queue(queue_nr))
		avia_gt_dmx_enable_disable_system_queue_irqs();

	avia_gt_dmx_set_pid_control_table(queue_nr, mode, 0, 0, 0, 1, 0, filt_tab_idx, (mode == AVIA_GT_DMX_QUEUE_MODE_SEC8) ? 1 : 0, no_of_filter);
	avia_gt_dmx_set_pid_table(queue_nr, wait_pusi, 0, pid);

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
	avia_gt_dmx_set_pid_table(queue_nr, 0, 1, 0);

	return 0;
}

static
void avia_gt_dmx_bh_task(void *tl_data)
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
		q->read_pos = q->hw_write_pos;
		return;
	}

	queue_info = &q->info;

	/* Resync for video, audio and teletext */
	if ((avia_gt_dmx_queue_is_system_queue(queue_nr)) && (q->mode == AVIA_GT_DMX_QUEUE_MODE_TS)) {
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
				if (pid1 == 0xFFFF)
					pid1 = pid2;
			}
		}

		avail = avia_gt_dmx_queue_get_bytes_avail(queue_info);

		while (avail >= 188) {
			avia_gt_dmx_queue_data_get(queue_info, &ts, 3, 1);

			if ((ts.sync_byte == 0x47) && ((ts.pid == pid1) || (ts.pid == pid2)))
				break;

			avail--;
			avia_gt_dmx_queue_data_get8(queue_info, 0);
		}

		if (avail < 188)
			return;
	}

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

int avia_gt_dmx_set_pid_control_table(u8 queue_nr, u8 type, u8 fork, u8 cw_offset, u8 cc, u8 start_up, u8 pec, u8 filt_tab_idx, u8 _psh, u8 no_of_filter)
{
	sPID_Parsing_Control_Entry ppc_entry;
	struct avia_gt_ucode_info *ucode_info;
	u8 target_queue_nr;
	u32 flags;

	if (queue_nr > AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_CRIT "avia_gt_dmx: pid control table entry out of bounds (entry=%d)!\n", queue_nr);
		return -EINVAL;
	}

	ucode_info = avia_gt_dmx_get_ucode_info();

	dprintk(KERN_DEBUG "avia_gt_dmx_set_pid_control_table, entry %d, type %d, fork %d, cw_offset %d, cc %d, start_up %d, pec %d, filt_tab_idx %d, _psh %d\n",
		queue_nr, type, fork, cw_offset, cc, start_up, pec, filt_tab_idx, _psh);

	/* Special case for SPTS audio queue */
	if ((queue_nr == AVIA_GT_DMX_QUEUE_AUDIO) && (type == AVIA_GT_DMX_QUEUE_MODE_TS))
		target_queue_nr = AVIA_GT_DMX_QUEUE_VIDEO;
	else
		target_queue_nr = queue_nr;

	target_queue_nr += ucode_info->qid_offset;

	ppc_entry.type = type;
	ppc_entry.QID = target_queue_nr;
	ppc_entry.fork = !!fork;
	ppc_entry.CW_offset = cw_offset;
	ppc_entry.CC = cc;
	ppc_entry._PSH = _psh;
	ppc_entry.start_up = !!start_up;
	ppc_entry.PEC = !!pec;
	ppc_entry.filt_tab_idx = filt_tab_idx;
	//ppc_entry.State = 0;
	ppc_entry.Reserved1 = 0;
	ppc_entry.no_of_filter = no_of_filter;
	/* FIXME: ppc_entry.State = 7; */
	ppc_entry.State = 7;

	local_irq_save(flags);
	avia_gt_dmx_risc_write(&risc_mem_map->PID_Parsing_Control_Table[queue_nr], &ppc_entry, sizeof(ppc_entry));
	local_irq_restore(flags);

	return 0;
}

int avia_gt_dmx_set_pid_table(u8 entry, u8 wait_pusi, u8 valid, u16 pid)
{
	sPID_Entry pid_entry;
	u32 flags;

	if (entry > 31) {
		printk(KERN_CRIT "avia_gt_dmx: pid search table entry out of bounds (entry=%d)!\n", entry);
		return -EINVAL;
	}

	dprintk(KERN_DEBUG "avia_gt_dmx_set_pid_table, entry %d, wait_pusi %d, valid %d, pid 0x%04X\n", entry, wait_pusi, valid, pid);

	pid_entry.wait_pusi = wait_pusi;
	pid_entry.VALID = !!valid;			// 0 = VALID, 1 = INVALID
	pid_entry.Reserved1 = 0;
	pid_entry.PID = pid;

	local_irq_save(flags);
	avia_gt_dmx_risc_write(&risc_mem_map->PID_Search_Table[entry], &pid_entry, sizeof(pid_entry));
	local_irq_restore(flags);

	return 0;
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

static
void avia_gt_dmx_reset(int reenable)
{
	avia_gt_reg_set(RSTR0, TDMP, 1);

	if (reenable)
		avia_gt_reg_set(RSTR0, TDMP, 0);
}

static
int avia_gt_dmx_risc_init(void)
{
	avia_gt_dmx_reset(0);

	avia_gt_dmx_load_ucode();

	avia_gt_dmx_reset(1);

	if (avia_gt_chip(ENX))
		enx_reg_16(EC) = 0;

	return 0;
}

static
u64 avia_gt_dmx_get_pcr_base(void)
{
	u64 pcr2 = avia_gt_readw(TP_PCR_2);
	u64 pcr1 = avia_gt_readw(TP_PCR_1);
	u64 pcr0 = avia_gt_readw(TP_PCR_0);

	return (pcr2 << 17) | (pcr1 << 1) | (pcr0 >> 15);
}

static
u64 avia_gt_dmx_get_latched_stc_base(void)
{
	u64 l_stc2 = avia_gt_readw(LC_STC_2);
	u64 l_stc1 = avia_gt_readw(LC_STC_1);
	u64 l_stc0 = avia_gt_readw(LC_STC_0);

	return (l_stc2 << 17) | (l_stc1 << 1) | (l_stc0 >> 15);
}

static
u64 avia_gt_dmx_get_stc_base(void)
{
	u64 stc2 = avia_gt_readw(STC_COUNTER_2);
	u64 stc1 = avia_gt_readw(STC_COUNTER_1);
	u64 stc0 = avia_gt_readw(STC_COUNTER_0);

	return (stc2 << 17) | (stc1 << 1) | (stc0 >> 15);
}

static
void avia_gt_dmx_set_dac(s16 pulse_count)
{
	avia_gt_writel(DAC_PC, (pulse_count << 16) | 9);
}

#define MAX_PCR_DIFF	36000
#define MAX_DIFF_DIFF	4
#define FORCE_DIFF	80
#define MAX_DIFF	200
#define GAIN		50

static
void avia_gt_pcr_irq(unsigned short irq)
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

void avia_gt_dmx_free_section_filter(u8 index)
{
	/* section_filter_umap is s8[] */
	s8 nr = (s8)index;

	if ((index > 31) || (section_filter_umap[index] != index)) {
		printk(KERN_CRIT "avia_gt_dmx: trying to free section filters with wrong index!\n");
		return;
	}

	while (index < 32) {
		if (section_filter_umap[index] == nr)
			section_filter_umap[index++] = -1;
		else
			break;
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
	u32 flags;

	/*
	 * Copy and "normalize" the filters. The section-length cannot be filtered.
	 */

	if (!filter)
		return -1;

	while (filter && anz_filters < MAX_SECTION_FILTERS) {
		mask[anz_filters][0]  = filter->filter.filter_mask[0];
		value[anz_filters][0] = filter->filter.filter_value[0] & filter->filter.filter_mask[0];
		mode[anz_filters][0]  = filter->filter.filter_mode[0] | ~filter->filter.filter_mask[0];

		if (mask[anz_filters][0])
			in_use[anz_filters] = 0;
		else
			in_use[anz_filters] = -1;

		for (i = 1; i < 8; i++) {
			mask[anz_filters][i]  = filter->filter.filter_mask[i+2];
			value[anz_filters][i] = filter->filter.filter_value[i+2] & filter->filter.filter_mask[i+2];
			mode[anz_filters][i]  = filter->filter.filter_mode[i+2] | ~filter->filter.filter_mask[i+2];
			in_use[anz_filters] = mask[anz_filters][i] ? i : in_use[anz_filters];
		}

		unchanged[anz_filters] = 1;

		if ((mode[anz_filters][3] != 0xFF) && ((mode[anz_filters][3] & mask[anz_filters][3]) == 0)) {
			ver_not[anz_filters] = 1;
			not_value[anz_filters] = value[anz_filters][3];
			not_mask[anz_filters] = mask[anz_filters][3];
		}
		else {
			ver_not[anz_filters] = 0;
		}

		/*
		 * Don't need to filter because one filter does contain a mask with
		 * all bits set to zero.
		 */

		if (in_use[anz_filters] == -1)
			return -1;

		anz_filters++;
		filter = filter->next;
	}

	if (filter) {
		printk(KERN_WARNING "avia_gt_dmx: too many section filters for hw-acceleration in this feed.\n");
		return -1;
	}

	i = anz_filters;

	while (i < MAX_SECTION_FILTERS)
		in_use[i++] = -1;

	/*
	 * Special case: only one section filter in this feed.
	 */

	if (anz_filters == 1) {
		/*
		 * Check wether we need a not filter
		 */

		anz_not = 0;
		anz_mixed = 0;
		anz_normal = 0;
		and_or = 0;	// AND

		for (i = 0; i < 8; i++) {
			if (mode[0][i] != 0xFF) {
				anz_not++;
				if (mode[0][i] & mask[0][i])
					anz_mixed++;
			}
			else if (mask[0][i]) {
				anz_normal++;
			}
		}

		/*
		 * Only the byte with the version has a mode != 0xFF.
		 */
		if ((anz_not == 1) && (anz_mixed == 0) && (mode[0][3] != 0xFF)) {
		}

		/*
		 * Mixed mode.
		 */
		else if ((anz_not > 0) && (anz_normal > 0)) {
			anz_filters = 2;
			for (i = 0; i < 8; i++) {
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
		else if (anz_not > 0) {
			not_the_first = 1;
		}
	}

	/*
	 * More than one filter
	 */

	else {
		and_or = 1;	// OR

		/*
		 * Cannot check for "mode-0" bits. Delete them from the mask.
		 */

		for (i = 0; i < anz_filters; i++) {
			in_use[i] = -1;
			for (j = 0; j < 8; j++) {
				mask[i][j] = mask[i][j] & mode[i][j];
				value[i][j] = value[i][j] & mask[i][j];
				if (mask[i][j])
					in_use[i] = j;
			}
			if (in_use[i] == -1)
				return -1;	// We cannot filter. Damn, thats a really bad case.
		}

		/*
		 * Eliminate redundant filters.
		 */

		old_anz_filters = anz_filters + 1;

		while (anz_filters != old_anz_filters) {
			old_anz_filters = anz_filters;
			for (i = 0; (i < MAX_SECTION_FILTERS - 1) && (anz_filters > 1); i++) {
				if (in_use[i] == -1)
					continue;
				for (j = i + 1; j < MAX_SECTION_FILTERS && (anz_filters > 1); j++) {
					if (in_use[j] == -1)
						continue;

					if (in_use[i] < in_use[j])
						compare_len = in_use[i] + 1;
					else
						compare_len = in_use[j] + 1;

					different_bit_index = -1;

					/*
					 * Check wether the filters are equal or different only in
					 * one bit.
					 */

					for (k = 0; k < compare_len; k++) {
						if ((mask[i][k] == mask[j][k]) && (value[i][k] != value[j][k])) {
							if (different_bit_index != -1)
								goto next_check;

							xor = value[i][k] ^ value[j][k];

							if (hweight8(xor) == 1)
								different_bit_index = k;
							else
								goto next_check;
						}
						else {
							goto next_check;
						}
					}

					if (different_bit_index != -1) {
						mask[i][different_bit_index] -= xor;
						value[i][different_bit_index] &= mask[i][different_bit_index];
						if (different_bit_index == in_use[i]) {
							in_use[i] = -1;
							for (k = 0; k < compare_len; k++)
								if (mask[i][k])
									in_use[i] = k;
						}
						else {
							in_use[i] = compare_len - 1;
						}
						if (in_use[i] == -1)
							return -1;	// Uups, eliminated all filters...
					}
					else {
						in_use[i] = compare_len - 1;
					}

					if ((not_value[i] != not_value[j]) || (not_mask[i] != not_mask[j]) || (ver_not[i] != ver_not[j]))
						unchanged[i] = 0;

					k = compare_len;

					while (k < 8) {
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
					memset(new_mask, 0, sizeof(new_mask));
					memset(new_value, 0, sizeof(new_value));

					for (k = 0; k < compare_len; k++) {
						temp_mask = mask[i][k] & mask[j][k];
						if (((temp_mask == mask[i][k]) ||
						      (temp_mask == mask[j][k])) &&
						      ((value[i][k] & temp_mask) == (value[j][k] & temp_mask)))	{
							new_mask[k] = temp_mask;
							new_value[k] = value[i][k] & temp_mask;
						}
						else {
							new_valid = 0;
							break;
						}
					}
					if (new_valid) {
						memcpy(mask[i], new_mask, 8);
						memcpy(value[i], new_value, 8);
						if ((not_value[i] != not_value[j]) || (not_mask[i] != not_mask[j]) || (ver_not[i] != ver_not[j]))
							unchanged[i] = 0;
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

	while (i < 32) {
		if (section_filter_umap[i] == -1) {
			if (j == 1) {
				new_entry_len++;
			}
			else {
				if ((new_entry_len < entry_len) && (new_entry_len >= anz_filters)) {
					entry_len = new_entry_len;
					entry = new_entry;
				}
				new_entry_len = 1;
				new_entry = i;
				j = 1;
			}
		}
		else {
			j = 0;
		}
		i++;
	}

	if (((entry == -1) && (new_entry != -1) && (new_entry_len >= anz_filters)) ||
	     ((new_entry_len < entry_len) && (new_entry_len >= anz_filters))) {
		entry = new_entry;
		entry_len = new_entry_len;
	}

	if (entry == -1)
		return -1;

	/*
	 * Mark filter_param_table as used.
	 */

	i = entry;

	while (i < entry + anz_filters)
		section_filter_umap[i++] = entry;

	/*
	 * Set filter_definition_table.
	 */

	filter_definition_table[entry].and_or_flag = and_or;

	/*
	 * Set filter parameter tables.
	 */

	i = 0;
	j = 0;
	while (j < anz_filters) {
		if (in_use[i] == -1) {
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

		if (unchanged[i] && ver_not[i]) {
			fpe3[j].not_flag_ver_id_byte = 1;
			fpe2[j].mask_3 = not_mask[i];
			fpe2[j].param_3 = not_value[i];
		}
		else {
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

	/* copy to riscram */
	local_irq_save(flags);
	avia_gt_dmx_risc_write(((u8 *) (&risc_mem_map->Filter_Definition_Table)) + (entry & 0xFE), ((u8*) filter_definition_table) + (entry & 0xFE), 2);
	avia_gt_dmx_risc_write(&risc_mem_map->Filter_Parameter_Table1[entry], fpe1, anz_filters * sizeof(sFilter_Parameter_Entry1));
	avia_gt_dmx_risc_write(&risc_mem_map->Filter_Parameter_Table2[entry], fpe2, anz_filters * sizeof(sFilter_Parameter_Entry2));
	avia_gt_dmx_risc_write(&risc_mem_map->Filter_Parameter_Table3[entry], fpe3, anz_filters * sizeof(sFilter_Parameter_Entry3));
	local_irq_restore(flags);

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

void avia_gt_dmx_force_discontinuity(void)
{
	force_stc_reload = 1;

	avia_gt_reg_set(FC, FD, 1);
}

static
void avia_gt_dmx_enable_disable_framer(u8 enable)
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

static
int avia_gt_dmx_enable_disable_clip_mode(u8 queue_nr, u8 enable)
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

void avia_gt_dmx_ecd_reset(void)
{
	u32 flags;

	local_irq_save(flags);
	avia_gt_dmx_memset16(&riscram[DMX_CONTROL_WORDS_1], 0, 24);
	avia_gt_dmx_memset16(&riscram[DMX_CONTROL_WORDS_2], 0, 24);
	avia_gt_dmx_memset16(&riscram[DMX_CONTROL_WORDS_3], 0, 16);
	local_irq_restore(flags);
}

int avia_gt_dmx_ecd_set_key(u8 index, u8 parity, const u8 *cw)
{
	u16 offset;
	u32 flags;

	offset = DMX_CONTROL_WORDS_1 + ((index / 3) << 7) + ((index % 3) << 3) + ((parity ^ 1) << 2);

	local_irq_save(flags);
	avia_gt_dmx_risc_write(&riscram[offset], cw, 8);
	local_irq_restore(flags);

	return 0;
}

int avia_gt_dmx_ecd_set_pid(u8 index, u16 pid)
{
	u16 offset, control;
	u32 flags;

	for (offset = 0; offset < 0x20; offset++) {
		if ((pst[offset] & 0x1fff) == pid) {
			control = ppct[offset << 1];
			if (((control >> 4) & 7) != index) {
				local_irq_save(flags);
				avia_gt_dmx_memset16(&ppct[offset << 1], 0, 1);
				avia_gt_dmx_memset16(&ppct[offset << 1], (control & 0xff8f) | (index << 4), 1);
				local_irq_restore(flags);
			}
			return 0;
		}
	}

	printk(KERN_DEBUG "avia_gt_dmx: pid %04x not found\n", pid);
	return -EINVAL;
}

void avia_gt_dmx_set_ucode_info(void)
{
	u16 version_no = riscram[DMX_VERSION_NO];

	switch (version_no) {
	case 0x0013:
	case 0x0014:
		if (avia_gt_chip(ENX)) 
			ucode_info.caps = (AVIA_GT_UCODE_CAP_ECD |
				AVIA_GT_UCODE_CAP_PES |
				//AVIA_GT_UCODE_CAP_SEC |
				AVIA_GT_UCODE_CAP_TS);
		else
			ucode_info.caps = (AVIA_GT_UCODE_CAP_ECD |
				AVIA_GT_UCODE_CAP_PES |
				AVIA_GT_UCODE_CAP_SEC |
				AVIA_GT_UCODE_CAP_TS);
		
		ucode_info.qid_offset = 1;
		ucode_info.queue_mode_pes = 3;
 		break;
	case 0x001a:
		ucode_info.caps = (AVIA_GT_UCODE_CAP_ECD |
			AVIA_GT_UCODE_CAP_PES |
			AVIA_GT_UCODE_CAP_TS);
		ucode_info.qid_offset = 0;
		ucode_info.queue_mode_pes = 1;
		break;
	case 0xb102:
	case 0xb107:
	case 0xb121:
		ucode_info.caps = (AVIA_GT_UCODE_CAP_PES |
			AVIA_GT_UCODE_CAP_TS);
		ucode_info.qid_offset = 0;
		ucode_info.queue_mode_pes = 3;
		break;
	default:
		ucode_info.caps = 0;
		if (version_no < 0xa000)
			ucode_info.qid_offset = 1;
		else 
			ucode_info.qid_offset = 0;
		ucode_info.queue_mode_pes = 3;
		break;
	}
}

struct avia_gt_ucode_info *avia_gt_dmx_get_ucode_info(void)
{
	return &ucode_info;
}

int __init avia_gt_dmx_init(void)
{
	sAviaGtDmxQueue *q;
	int result;
	u32 queue_addr;
	u8 queue_nr;

	printk(KERN_INFO "avia_gt_dmx: $Id: avia_gt_dmx.c,v 1.190 2003/11/12 22:07:44 sepp776 Exp $\n");;

	gt_info = avia_gt_get_info();

	if (!avia_gt_supported_chipset(gt_info))
		return -ENODEV;

	if (avia_gt_chip(ENX))
		enx_reg_32(RSTR0) |= (1 << 31) | (1 << 23) | (1 << 22);

	risc_mem_map = avia_gt_reg_o(gt_info->tdp_ram);
	riscram = (volatile u16 *)risc_mem_map;
	pst = &riscram[DMX_PID_SEARCH_TABLE];
	ppct = &riscram[DMX_PID_PARSING_CONTROL_TABLE];

	if ((result = avia_gt_dmx_risc_init()))
		return result;

	/* fill ucode info struct */
	avia_gt_dmx_set_ucode_info();

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

		q->mem_addr = queue_addr;
		queue_addr += q->size;

		q->pid = 0xFFFF;
		q->running = 0;
		q->info.index = queue_nr;
		q->info.bytes_avail = avia_gt_dmx_queue_get_bytes_avail;
		q->info.bytes_free = avia_gt_dmx_queue_get_bytes_free;
		q->info.size = avia_gt_dmx_queue_get_size;
		q->info.crc32_le = avia_gt_dmx_queue_crc32;
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

	for (queue_nr = 0; queue_nr < 32; queue_nr++) {
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
MODULE_PARM(ucode, "s");
MODULE_PARM_DESC(ucode, "path to risc microcode");

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
EXPORT_SYMBOL(avia_gt_dmx_set_pid_control_table);
EXPORT_SYMBOL(avia_gt_dmx_set_pid_table);

EXPORT_SYMBOL(avia_gt_dmx_system_queue_set_pos);
EXPORT_SYMBOL(avia_gt_dmx_system_queue_set_read_pos);
EXPORT_SYMBOL(avia_gt_dmx_system_queue_set_write_pos);
EXPORT_SYMBOL(avia_gt_dmx_free_section_filter);
EXPORT_SYMBOL(avia_gt_dmx_alloc_section_filter);
EXPORT_SYMBOL(avia_gt_dmx_force_discontinuity);
EXPORT_SYMBOL(avia_gt_dmx_enable_framer);
EXPORT_SYMBOL(avia_gt_dmx_disable_framer);
EXPORT_SYMBOL(avia_gt_dmx_enable_clip_mode);
EXPORT_SYMBOL(avia_gt_dmx_disable_clip_mode);
EXPORT_SYMBOL(avia_gt_dmx_queue_write);
EXPORT_SYMBOL(avia_gt_dmx_queue_nr_get_bytes_free);
EXPORT_SYMBOL(avia_gt_dmx_ecd_reset);
EXPORT_SYMBOL(avia_gt_dmx_ecd_set_key);
EXPORT_SYMBOL(avia_gt_dmx_ecd_set_pid);
EXPORT_SYMBOL(avia_gt_dmx_get_ucode_info);
