/*
 * $Id: dbox2_fp_rc.c,v 1.17 2003/11/20 01:12:02 carjay Exp $
 *
 * Copyright (C) 2002 by Florian Schirmer <jolt@tuxbox.org>
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
#include <linux/init.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/tqueue.h>
#include <linux/poll.h>
#include <linux/input.h>

#include <dbox/dbox2_fp_core.h>

#include "input_fake.h"

enum {
	KEY_RELEASED = 0,
	KEY_PRESSED,
	KEY_AUTOREPEAT
};
#define UP_TIMEOUT (HZ / 4)

static struct input_dev *rc_input_dev;
static int old_rc = 1;
static int new_rc = 1;
static u8 toggle_bits = 0xFF;

//#define dprintk printk
#define dprintk if (0) printk

static struct {

	unsigned long code;
	u8 value_new;
	u8 value_old;
	
} rc_key_map[] = {

	{KEY_0,				0x00, 0x00},
	{KEY_1,				0x01, 0x01},
	{KEY_2,				0x02, 0x02},
	{KEY_3,				0x03, 0x03},
	{KEY_4,				0x04, 0x04},
	{KEY_5,				0x05, 0x05},
	{KEY_6,				0x06, 0x06},
	{KEY_7,				0x07, 0x07},
	{KEY_8,				0x08, 0x08},
	{KEY_9,				0x09, 0x09},
	{KEY_RIGHT, 			0x0A, 0x2E},
	{KEY_LEFT,			0x0B, 0x2F},
	{KEY_UP,			0x0C, 0x0E},
	{KEY_DOWN,			0x0D, 0x0F},
	{KEY_OK,			0x0E, 0x30},
	{KEY_MUTE,			0x0F, 0x28},
	{KEY_POWER,			0x10, 0x0C},
	{KEY_GREEN,			0x11, 0x55},
	{KEY_YELLOW,			0x12, 0x52},
	{KEY_RED,			0x13, 0x2d},
	{KEY_BLUE,			0x14, 0x3b},
	{KEY_VOLUMEUP,			0x15, 0x16},
	{KEY_VOLUMEDOWN,		0x16, 0x17},
	{KEY_HELP,			0x17, 0x82},
	{KEY_SETUP,			0x18, 0x27},
	{KEY_TOPLEFT,			0x1B, 0xff},
	{KEY_TOPRIGHT,			0x1C, 0xff},
	{KEY_BOTTOMLEFT,		0x1D, 0xff},
	{KEY_BOTTOMRIGHT,		0x1E, 0xff},
	{KEY_HOME,			0x1F, 0x20},
	{KEY_PAGEDOWN,			0x53, 0x53},
	{KEY_PAGEUP,			0x54, 0x54},
	
};

#define RC_KEY_COUNT	(sizeof(rc_key_map) / sizeof(rc_key_map[0]))

static void dbox2_fp_rc_keyup(unsigned long data)
{

	if ((!data) || (!test_bit(data, rc_input_dev->key)))
		return;

	input_event(rc_input_dev, EV_KEY, data, KEY_RELEASED);	// "key released" event after timeout

}

static struct timer_list keyup_timer = { 

	.function = dbox2_fp_rc_keyup
	
};

void dbox2_fp_rc_queue_handler(u8 queue_nr)
{

	u16 rc_code;
	u8 rc_key_nr;

	dprintk("event on queue %d\n", queue_nr);

	if ((queue_nr != 0) && (queue_nr != 3))
		return;

	u8 cmd;
	switch (mid) {
		case TUXBOX_DBOX2_MID_NOKIA:
			cmd = 0x01;		
			break;
			
		case TUXBOX_DBOX2_MID_PHILIPS:
		case TUXBOX_DBOX2_MID_SAGEM:
			cmd = 0x26;	
			break;
	}
	fp_cmd(fp_get_i2c(), cmd, (u8*)&rc_code, sizeof(rc_code));

	dprintk("raw=0x%08X code=0x%08X rest=0x%08X\n", rc_code, rc_code & 0x1F, rc_code & (~0x1F));

	for (rc_key_nr = 0; rc_key_nr < RC_KEY_COUNT; rc_key_nr++) {
	
		if (((queue_nr == 0) && (rc_key_map[rc_key_nr].value_old == (rc_code & 0xFF))) ||
			((queue_nr == 3) && (rc_key_map[rc_key_nr].value_new == (rc_code & 0x1F)))) {
		
			if (timer_pending(&keyup_timer)) {
	
				del_timer(&keyup_timer);
	
				if ((keyup_timer.data != rc_key_map[rc_key_nr].code) || (toggle_bits != ((rc_code >> 6) & 0x03)))
					input_event(rc_input_dev, EV_KEY, keyup_timer.data, KEY_RELEASED);	// "key released" event because a new key arrived
			}
																		
			if (toggle_bits == ((rc_code>>6)&0x03))
				input_event(rc_input_dev, EV_KEY, rc_key_map[rc_key_nr].code, KEY_AUTOREPEAT);		// "key autorepeat" event
			else
				input_event(rc_input_dev, EV_KEY, rc_key_map[rc_key_nr].code, KEY_PRESSED);		// "key pressed" event

			keyup_timer.expires = jiffies + UP_TIMEOUT;
			keyup_timer.data = rc_key_map[rc_key_nr].code;

			add_timer(&keyup_timer);
			
			toggle_bits = (rc_code >> 6) & 0x03;
		}

	}

}

int __init dbox2_fp_rc_init(struct input_dev *input_dev)
{

	u8 rc_key_nr;
	
	rc_input_dev = input_dev;
		
	set_bit(EV_KEY, rc_input_dev->evbit);

	for (rc_key_nr = 0; rc_key_nr < RC_KEY_COUNT; rc_key_nr++)
		set_bit(rc_key_map[rc_key_nr].code, rc_input_dev->keybit);

	if (old_rc)
		dbox2_fp_queue_alloc(0, dbox2_fp_rc_queue_handler);

	if (new_rc)
		dbox2_fp_queue_alloc(3, dbox2_fp_rc_queue_handler);
		
	// Enable break codes	
	//fp_sendcmd(fp_get_i2c(), 0x01, 0x80);
	fp_sendcmd(fp_get_i2c(), 0x26, 0x80); // Mhhh .. muesste das nicht an 0x01 bei Nokia? Geht aber beides nicht mit einer Sagem FB

	return 0;

}

void __exit dbox2_fp_rc_exit(void)
{

	if (old_rc)
		dbox2_fp_queue_free(0);

	if (new_rc)
		dbox2_fp_queue_free(3);

	// Disable break codes	
	//fp_sendcmd(fp_get_i2c(), 0x01, 0x00);
	fp_sendcmd(fp_get_i2c(), 0x26, 0x00); // Mhhh .. muesste das nicht an 0x01 bei Nokia? Geht aber beides nicht mit einer Sagem FB

}

#ifdef MODULE
MODULE_PARM(old_rc, "i");
MODULE_PARM(new_rc, "i");
#endif
