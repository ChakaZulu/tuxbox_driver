/*
 * $Id: dbox2_fp_button.c,v 1.1 2002/12/27 17:32:44 Jolt Exp $
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

static struct input_dev *button_input_dev;

static void dbox2_fp_button_queue_handler(u8 queue_nr)
{
	u8 button;
	u16 key;
	u8 state = 0;

	if (queue_nr != 4)
		return;

	fp_cmd(fp_get_i2c(), 0x25, (u8 *)&button, sizeof(button));
	
	switch(button) {
	
		case 0x9D:
		
			state = 1;

		case 0x9F:
		
			key = KEY_POWER;
			
			break;
	
		case 0xAB:
		
			state = 1;

		case 0xAF:
		
			key = KEY_DOWN;
			
			break;
	
		case 0xC7:
		
			state = 1;

		case 0xCF:
		
			key = KEY_UP;
			
			break;
	
		default:
		
			printk("dbox2_fp_button: unkown panel button code 0x%02X\n", button);
	
			return;
					
	}

	if (state) {
	
		clear_bit(key, button_input_dev->key);
		input_event(button_input_dev, EV_KEY, key, !0);
		
	} else {
	
		input_event(button_input_dev, EV_KEY, key, !!0);
		
	}

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
