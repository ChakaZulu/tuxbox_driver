/*
 *   avia_gt_dmx.h - dmx driver for AViA (dbox-II-project)
 *
 *   Homepage: http://www.tuxbox.org
 *
 *   Copyright (C) 2002 Florian Schirmer (jolt@tuxbox.org)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef AVIA_GT_DMX_H
#define AVIA_GT_DMX_H

#define AVIA_GT_DMX_QUEUE_COUNT			32	/* HIGH_SPEED queue isn't really a queue */

#define AVIA_GT_DMX_QUEUE_VIDEO			0
#define AVIA_GT_DMX_QUEUE_AUDIO			1
#define AVIA_GT_DMX_QUEUE_TELETEXT		2
#define AVIA_GT_DMX_QUEUE_USER_START		3
#define AVIA_GT_DMX_QUEUE_USER_END		30
#define AVIA_GT_DMX_QUEUE_MESSAGE		31
#define AVIA_GT_DMX_QUEUE_HIGH_SPEED		32

#define AVIA_GT_DMX_SYSTEM_QUEUES		0xff	/* alias for video+audio+teletext queues */

struct avia_gt_dmx_queue {
	u8 index;
	s8 hw_sec_index;

	u32	(*bytes_avail)(struct avia_gt_dmx_queue *queue);
	u32	(*bytes_free)(struct avia_gt_dmx_queue *queue);
	u32	(*size)(struct avia_gt_dmx_queue *queue);
	u32	(*crc32_be)(struct avia_gt_dmx_queue *queue, u32 count, u32 seed);
	u32	(*get_buf1_ptr)(struct avia_gt_dmx_queue *queue);
	u32	(*get_buf2_ptr)(struct avia_gt_dmx_queue *queue);
	u32	(*get_buf1_size)(struct avia_gt_dmx_queue *queue);
	u32	(*get_buf2_size)(struct avia_gt_dmx_queue *queue);
	u32	(*get_data)(struct avia_gt_dmx_queue *queue, void *dest, u32 count, u8 peek);
	u8	(*get_data8)(struct avia_gt_dmx_queue *queue, u8 peek);
	u16	(*get_data16)(struct avia_gt_dmx_queue *queue, u8 peek);
	u32	(*get_data32)(struct avia_gt_dmx_queue *queue, u8 peek);
	void	(*flush)(struct avia_gt_dmx_queue *queue);
	u32	(*put_data)(struct avia_gt_dmx_queue *queue, const void *src, u32 count, u8 src_is_user_space);
};

typedef void (AviaGtDmxQueueProc)(struct avia_gt_dmx_queue *queue, void *priv_data);

typedef struct {
	u8 busy;
	AviaGtDmxQueueProc *cb_proc;
	u32 hw_read_pos;
	u32 hw_write_pos;
	struct avia_gt_dmx_queue info;
	u32 irq_count;
	AviaGtDmxQueueProc *irq_proc;
	u32 mem_addr;
	u8 overflow_count;
	void *priv_data;
	u32 qim_irq_count;
	u32 qim_jiffies;
	u8 qim_mode;
	u32 read_pos;
	u32 size;
	u32 write_pos;
	u16 pid;
	u8 mode;
	u8 running;
	struct tq_struct task_struct;
} sAviaGtDmxQueue;

struct avia_gt_dmx_queue *avia_gt_dmx_alloc_queue_audio(AviaGtDmxQueueProc *irq_proc, AviaGtDmxQueueProc *cb_proc, void *cb_data);
struct avia_gt_dmx_queue *avia_gt_dmx_alloc_queue_message(AviaGtDmxQueueProc *irq_proc, AviaGtDmxQueueProc *cb_proc, void *cb_data);
struct avia_gt_dmx_queue *avia_gt_dmx_alloc_queue_teletext(AviaGtDmxQueueProc *irq_proc, AviaGtDmxQueueProc *cb_proc, void *cb_data);
struct avia_gt_dmx_queue *avia_gt_dmx_alloc_queue_user(AviaGtDmxQueueProc *irq_proc, AviaGtDmxQueueProc *cb_proc, void *cb_data);
struct avia_gt_dmx_queue *avia_gt_dmx_alloc_queue_video(AviaGtDmxQueueProc *irq_proc, AviaGtDmxQueueProc *cb_proc, void *cb_data);
int avia_gt_dmx_free_queue(u8 queue_nr);
sAviaGtDmxQueue *avia_gt_dmx_get_queue_info(u8 queue_nr);
void avia_gt_dmx_fake_queue_irq(u8 queue_nr);
u32 avia_gt_dmx_queue_get_write_pos(u8 queue_nr);
void avia_gt_dmx_queue_irq_disable(u8 queue_nr);
int avia_gt_dmx_queue_irq_enable(u8 queue_nr);
int avia_gt_dmx_queue_reset(u8 queue_nr);
void avia_gt_dmx_queue_set_write_pos(u8 queue_nr, u32 write_pos);
int avia_gt_dmx_queue_start(u8 queue_nr, u8 mode, u16 pid, u8 wait_pusi, u8 filt_tab_idx, u8 no_of_filter);
int avia_gt_dmx_queue_stop(u8 queue_nr);
void avia_gt_dmx_set_pcr_pid(u8 enable, u16 pid);
u32 avia_gt_dmx_system_queue_get_read_pos(u8 queue_nr);
void avia_gt_dmx_system_queue_set_pos(u8 queue_nr, u32 read_pos, u32 write_pos);
void avia_gt_dmx_system_queue_set_read_pos(u8 queue_nr, u32 read_pos);
void avia_gt_dmx_system_queue_set_write_pos(u8 queue_nr, u32 write_pos);

void avia_gt_dmx_force_discontinuity(void);
void avia_gt_dmx_enable_framer(void);
void avia_gt_dmx_disable_framer(void);

int avia_gt_dmx_enable_clip_mode(u8 queue_nr);
int avia_gt_dmx_disable_clip_mode(u8 queue_nr);

int avia_gt_dmx_queue_write(u8 queue_nr, const u8 *buf, size_t count, u32 nonblock);
int avia_gt_dmx_queue_nr_get_bytes_free(u8 queue_nr);

int avia_gt_dmx_init(void);
void avia_gt_dmx_exit(void);

#endif /* AVIA_GT_DMX_H */
