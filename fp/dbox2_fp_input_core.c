/*
 * $Id: dbox2_fp_input_core.c,v 1.4 2003/03/06 10:03:32 waldi Exp $
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

TUXBOX_INFO(dbox2_mid);
tuxbox_dbox2_mid_t mid;

static struct input_dev input_dev;

extern int dbox2_fp_button_init(struct input_dev *input_dev);
extern void dbox2_fp_button_exit(void);
extern int dbox2_fp_keyboard_init(struct input_dev *input_dev);
extern void dbox2_fp_keyboard_exit(void);
extern int dbox2_fp_mouse_init(struct input_dev *input_dev);
extern void dbox2_fp_mouse_exit(void);
extern int dbox2_fp_rc_init(struct input_dev *input_dev);
extern void dbox2_fp_rc_exit(void);

int __init dbox2_fp_input_init(void)
{

	memset(input_dev.keybit, 0, sizeof(input_dev.keybit));

	mid = tuxbox_dbox2_mid;

	input_dev.name = "DBOX-2 FP IR";
	input_dev.idbus = BUS_I2C;
	
	dbox2_fp_rc_init(&input_dev);
	dbox2_fp_keyboard_init(&input_dev);
	dbox2_fp_mouse_init(&input_dev);
	dbox2_fp_button_init(&input_dev);

	input_register_device(&input_dev);

	return 0;

}

void __exit dbox2_fp_input_exit(void)
{

	dbox2_fp_button_exit();
	dbox2_fp_mouse_exit();
	dbox2_fp_keyboard_exit();
	dbox2_fp_rc_exit();

	input_unregister_device(&input_dev);

}

#ifdef MODULE
module_init(dbox2_fp_input_init);
module_exit(dbox2_fp_input_exit);
MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("DBOX-2 IR input driver");
MODULE_LICENSE("GPL");
#endif
