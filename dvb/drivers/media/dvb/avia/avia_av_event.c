/*
 * $Id: avia_av_event.c,v 1.8 2003/07/01 08:21:57 obi Exp $
 *   
 * AViA 500/600 event driver (dbox-II-project)
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <linux/init.h>

#include "avia_av.h"
#include "avia_av_event.h"
#include <dbox/event.h>

static u32 event_delay;
static spinlock_t event_lock = SPIN_LOCK_UNLOCKED;
static struct timer_list event_timer;

static void avia_av_event_timer_function(unsigned long data)
{
	struct event_t event;
	struct avia_av_event_reg *reg = (struct avia_av_event_reg *)data;

	spin_lock_irq(&event_lock);

	// TODO: optimize
	if ((++event_delay) == 30) {
		event_delay = 0;

		if (reg) {
			if (((avia_av_dram_read(H_SIZE) & 0xFFFF) != reg->hsize) ||
				((avia_av_dram_read(V_SIZE) & 0xFFFF) != reg->vsize)) {
				reg->hsize = (avia_av_dram_read(H_SIZE) & 0xFFFF);
				reg->vsize = (avia_av_dram_read(V_SIZE) & 0xFFFF);
				memset(&event, 0, sizeof(struct event_t));
				event.event = EVENT_VHSIZE_CHANGE;
				event_write_message(&event, 1);
			}

			if ((avia_av_dram_read(ASPECT_RATIO) & 0xFFFF) != reg->aratio) {
				reg->aratio = (avia_av_dram_read(ASPECT_RATIO) & 0xFFFF);
				memset(&event, 0, sizeof(struct event_t));
				event.event = EVENT_ARATIO_CHANGE;
				event_write_message(&event, 1);
			}

			if ((avia_av_dram_read(FRAME_RATE) & 0xFFFF) != reg->frate)
				reg->frate = avia_av_dram_read(FRAME_RATE) & 0xFFFF;

			if ((avia_av_dram_read(BIT_RATE) & 0xFFFF) != reg->brate)
				reg->brate = avia_av_dram_read(BIT_RATE) & 0xFFFF;

			if ((avia_av_dram_read(VBV_SIZE) & 0xFFFF) != reg->vbsize)
				reg->vbsize = avia_av_dram_read(VBV_SIZE) & 0xFFFF;

			if ((avia_av_dram_read(AUDIO_TYPE) & 0xFFFF) != reg->atype)
				reg->atype = avia_av_dram_read(AUDIO_TYPE) & 0xFFFF;
		}
	}

	mod_timer(&event_timer, jiffies + HZ / AVIA_AV_EVENT_TIMER + 2 * HZ / 100);

	spin_unlock_irq(&event_lock);
}

int avia_av_event_init(void)
{
	printk(KERN_INFO "avia_av_event: $Id: avia_av_event.c,v 1.8 2003/07/01 08:21:57 obi Exp $\n");

	event_delay = 0;

	event_timer.data = (unsigned long)kmalloc(sizeof(struct avia_av_event_reg), GFP_KERNEL);

	if (!event_timer.data)
		return -ENOMEM;

	memset((void *)event_timer.data, 0, sizeof(struct avia_av_event_reg));

	event_timer.expires = jiffies + HZ / AVIA_AV_EVENT_TIMER + 2 * HZ / 100;
	event_timer.function = avia_av_event_timer_function;

	init_timer(&event_timer);
	add_timer(&event_timer);
		
	return 0;
}

void avia_av_event_exit(void)
{
	spin_lock_irq(&event_lock);

	if (event_timer.data) {
		kfree((void *)event_timer.data);
		event_timer.data = 0;
	}

	del_timer(&event_timer);

	spin_unlock_irq(&event_lock);
}

#if defined(STANDALONE)
module_init(avia_av_event_init);
module_exit(avia_av_event_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("AViA 500/600 event driver");
MODULE_LICENSE("GPL");
#endif
