/*
 * $Id: dbox2_fp_button.c,v 1.2 2003/01/03 16:00:34 Jolt Exp $
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
#include <linux/input.h>

#include <dbox/dbox2_fp_core.h>

static u16 button_new_code_map[] = {

	/* 000-007 */	KEY_RESERVED, KEY_POWER, KEY_UP, KEY_RESERVED, KEY_DOWN, KEY_RESERVED, KEY_RESERVED,
	
};

static u16 button_old_code_map[] = {

	/* 000-007 */	KEY_RESERVED, KEY_POWER, KEY_DOWN, KEY_RESERVED, KEY_UP, KEY_RESERVED, KEY_RESERVED,
	
};

static struct input_dev *button_input_dev;

static void dbox2_fp_button_queue_handler(u8 queue_nr)
{
	u8 button;
	u16 key;
	u8 pressed = 0;

	if (queue_nr != 4)
		return;

	fp_cmd(fp_get_i2c(), 0x25, (u8 *)&button, sizeof(button));
	
	if (button & 0x80) {
	
		pressed = !!((((button >> 1) ^ 0x07) & 0x07) & ((button >> 4) & 0x07));
		key = button_old_code_map[(button >> 4) & 0x07];
		
	} else {
	
		pressed = !!(((button >> 1) & 0x07) & ((button >> 4) & 0x07));
		key = button_new_code_map[(button >> 4) & 0x07];
	
	}

	
	if (pressed)
		clear_bit(key, button_input_dev->key);

	input_event(button_input_dev, EV_KEY, key, pressed);

}

int __init dbox2_fp_button_init(struct input_dev *input_dev)
{

	button_input_dev = input_dev;
		
	set_bit(EV_KEY, button_input_dev->evbit);

	set_bit(KEY_UP, button_input_dev->keybit);
	set_bit(KEY_DOWN, button_input_dev->keybit);
	set_bit(KEY_POWER, button_input_dev->keybit);
		
	dbox2_fp_queue_alloc(4, dbox2_fp_button_queue_handler);

	return 0;

}

void __exit dbox2_fp_button_exit(void)
{

	dbox2_fp_queue_free(4);

}
