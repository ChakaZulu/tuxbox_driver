/*
 * $Id: dbox2_fp_rc.c,v 1.22 2004/03/26 09:50:04 carjay Exp $
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <dbox/dbox2_fp_core.h>

#include "input_fake.h"

enum {
	KEY_RELEASED = 0,
	KEY_PRESSED,
	KEY_AUTOREPEAT
};
#define UP_TIMEOUT (HZ / 4)

static struct input_dev *rc_input_dev;
static int disable_old_rc;
static int disable_new_rc;

static struct rc_key {
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

#define RC_KEY_COUNT	ARRAY_SIZE(rc_key_map)

struct fp_up {
	struct rc_key *last_key;
	u8 toggle_bits;			// only RC_NEW
};

static void dbox2_fp_rc_keyup(unsigned long data)
{
	struct fp_up *fups=(struct fp_up*)data;

	if ((!fups) || (!fups->last_key) || (!test_bit(fups->last_key->code, rc_input_dev->key)))
		return;

	/* "key released" event after timeout */
	fups->toggle_bits=0xff;
	input_event(rc_input_dev, EV_KEY, fups->last_key->code, KEY_RELEASED);
	if (fups->last_key) fups->last_key=NULL;
}

static struct timer_list keyup_timer = { 
	.function = dbox2_fp_rc_keyup
};

static void dbox2_fp_old_rc_queue_handler(u8 queue_nr)
{
	struct rc_key *key;
	static struct fp_up fupsold;
	u16 rc_code;

	fp_cmd(fp_get_i2c(), 0x01, (u8*)&rc_code, sizeof(rc_code));

	if (disable_old_rc)
		return;

	if (rc_code == 0x5cfe) {	// break code
		if (fupsold.last_key) {
			if (timer_pending(&keyup_timer)) del_timer_sync(&keyup_timer);
			if (fupsold.last_key) input_event(rc_input_dev, EV_KEY, fupsold.last_key->code, KEY_RELEASED);
			fupsold.last_key = NULL;
		}
		return;
	}

	for (key = rc_key_map; key < &rc_key_map[RC_KEY_COUNT]; key++) {
		if (key->value_old == (rc_code & 0xff)) {
			if ((fupsold.last_key) && (fupsold.last_key->code == key->code)) {
				input_event(rc_input_dev, EV_KEY, key->code, KEY_AUTOREPEAT);
				break;
			}
			else {
				input_event(rc_input_dev, EV_KEY, key->code, KEY_PRESSED);
				fupsold.last_key = key;
				break;
			}
			keyup_timer.expires = jiffies + (UP_TIMEOUT<<1);	// just in case the break code gets lost
			keyup_timer.data = (unsigned long)&fupsold;
			add_timer(&keyup_timer);
		}
	}
}

static void dbox2_fp_new_rc_queue_handler(u8 queue_nr)
{
	struct rc_key *key;
	static struct fp_up fupsnew={
		.toggle_bits=0xff
	};
	u16 rc_code;
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

	if (disable_new_rc)
		return;

	for (key = rc_key_map; key < &rc_key_map[RC_KEY_COUNT]; key++) {
		if (key->value_new == (rc_code & 0x1f)) {
			if (timer_pending(&keyup_timer)) {
				del_timer_sync(&keyup_timer);
				if ((fupsnew.last_key->code != key->code) || (fupsnew.toggle_bits != ((rc_code >> 6) & 0x03)))
					if (fupsnew.toggle_bits!=0xff) {
						input_event(rc_input_dev, EV_KEY, fupsnew.last_key->code, KEY_RELEASED);
						fupsnew.toggle_bits=0xff;
					}
			}
			if ((fupsnew.toggle_bits!=0xff)&&(fupsnew.toggle_bits == ((rc_code >> 6) & 0x03)))
				input_event(rc_input_dev, EV_KEY, key->code, KEY_AUTOREPEAT);
			else
				input_event(rc_input_dev, EV_KEY, key->code, KEY_PRESSED);
			keyup_timer.expires = jiffies + UP_TIMEOUT;
			keyup_timer.data = (unsigned long)&fupsnew;
			fupsnew.last_key=key;
			fupsnew.toggle_bits = (rc_code >> 6) & 0x03;

			add_timer(&keyup_timer);
		}
	}
}

int __init dbox2_fp_rc_init(struct input_dev *input_dev)
{
	struct rc_key *key;

	rc_input_dev = input_dev;

	set_bit(EV_KEY, rc_input_dev->evbit);

	for (key = rc_key_map; key < &rc_key_map[RC_KEY_COUNT]; key++)
		set_bit(key->code, rc_input_dev->keybit);

	dbox2_fp_queue_alloc(0, dbox2_fp_old_rc_queue_handler);	/* old */
	dbox2_fp_queue_alloc(3, dbox2_fp_new_rc_queue_handler);	/* new */

	/* Enable break codes */
	fp_sendcmd(fp_get_i2c(), 0x26, 0x80);

	return 0;
}

void __exit dbox2_fp_rc_exit(void)
{
	dbox2_fp_queue_free(0);	/* old */
	dbox2_fp_queue_free(3);	/* new */

	/* Disable break codes */
	fp_sendcmd(fp_get_i2c(), 0x26, 0x00);
}

#ifdef MODULE
MODULE_PARM(disable_old_rc, "i");
MODULE_PARM(disable_new_rc, "i");
#endif
