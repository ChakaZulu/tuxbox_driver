/*
 * $Id: dbox2_fp_mouse.c,v 1.3 2003/03/05 09:52:17 waldi Exp $
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

static struct input_dev *mouse_input_dev;
static u32 mouse_directions;

//#define dprintk printk
#define dprintk if (0) printk

#define DIR_RIGHT_UP	0x01
#define DIR_UP_RIGHT	0x02
#define DIR_UP_LEFT	0x04
#define DIR_LEFT_UP	0x08
#define DIR_LEFT_DOWN	0x10
#define DIR_DOWN_LEFT	0x20
#define DIR_DOWN_RIGHT	0x40
#define DIR_RIGHT_DOWN	0x80

/*
 * mouse codes:
 * ------------
 * 1st halfbyte: acceleration
 * 2nd halfbyte: direction
 * 
 *         4
 *       5   3
 *     6       2
 *   7           1
 * 8               0
 *   9           F
 *     A       E
 *       B   D
 *         C
 *
 */

static void dbox2_fp_mouse_queue_handler(u8 queue_nr)
{
	u16 mousecode = 0xFFFF;

	dprintk("event on queue %d\n", queue_nr);

	if (queue_nr != 2)
		return;

	switch (mid) {

		case TUXBOX_DBOX2_MID_NOKIA:

			fp_cmd(fp_get_i2c(), 0x05, (u8 *)&mousecode, 2);

			break;

		case TUXBOX_DBOX2_MID_PHILIPS:
		case TUXBOX_DBOX2_MID_SAGEM:

			fp_cmd(fp_get_i2c(), 0x2A, (u8 *)&mousecode, 2);

			break;
			
	}
	
	dprintk("mousecode: 0x%02X accel: 0x%01X dir: 0x%01X\n", mousecode, (mousecode & 0xF0) >> 4, mousecode & 0x0F);

	switch (mousecode & 0x0F) {

		case 0x00: /* right */

			input_report_rel(mouse_input_dev, REL_X, ((mousecode & 0xFF) >> 4) + 1);
			
			mouse_directions = 0;
			
			break;

		case 0x01: /* right up */

			input_report_rel(mouse_input_dev, REL_X, ((mousecode & 0xFF) >> 4) + 1);

			if (mouse_directions == DIR_RIGHT_UP) {

				input_report_rel(mouse_input_dev, REL_Y, -((mousecode & 0xFF) >> 4) - 1);
				mouse_directions = 0;

			} else {

				mouse_directions = DIR_RIGHT_UP;

			}

			break;

		case 0x02: /* right+up */

			input_report_rel(mouse_input_dev, REL_X, ((mousecode & 0xFF) >> 4) + 1);
			input_report_rel(mouse_input_dev, REL_Y, -((mousecode & 0xFF) >> 4) - 1);
			mouse_directions = 0;

			break;

		case 0x03: /* up right */

			input_report_rel(mouse_input_dev, REL_Y, -((mousecode & 0xFF) >> 4) - 1);
			
			if (mouse_directions == DIR_UP_RIGHT) {
			
				input_report_rel(mouse_input_dev, REL_X, ((mousecode & 0xFF) >> 4) + 1);
				mouse_directions = 0;
				
			} else {
			
				mouse_directions = DIR_UP_RIGHT;
				
			}
			
			break;

		case 0x04: /* up */
		
			input_report_rel(mouse_input_dev, REL_Y, -((mousecode & 0xFF) >> 4) - 1);
			mouse_directions = 0;
			
			break;

		case 0x05: /* up left */
		
			input_report_rel(mouse_input_dev, REL_Y, -((mousecode & 0xFF) >> 4) - 1);
			
			if (mouse_directions == DIR_UP_LEFT) {
			
				input_report_rel(mouse_input_dev, REL_X, -((mousecode & 0xFF) >> 4) - 1);
				mouse_directions = 0;
				
			} else {
			
				mouse_directions = DIR_UP_LEFT;
				
			}
			
			break;

		case 0x06: /* up+left */

			input_report_rel(mouse_input_dev, REL_X, -((mousecode & 0xFF) >> 4) - 1);
			input_report_rel(mouse_input_dev, REL_Y, -((mousecode & 0xFF) >> 4) - 1);
			mouse_directions = 0;

			break;

		case 0x07: /* left up */

			input_report_rel(mouse_input_dev, REL_X, -((mousecode & 0xFF) >> 4) - 1);

			if (mouse_directions == DIR_LEFT_UP) {
	
				input_report_rel(mouse_input_dev, REL_Y, -((mousecode & 0xFF) >> 4) - 1);
				mouse_directions = 0;
	
			} else {
			
				mouse_directions = DIR_LEFT_UP;
				
			}
			
			break;

		case 0x08: /* left */
		
			input_report_rel(mouse_input_dev, REL_X, -((mousecode & 0xFF) >> 4) - 1);
			mouse_directions = 0;
		
			break;

		case 0x09: /* left down */

			input_report_rel(mouse_input_dev, REL_X, -((mousecode & 0xFF) >> 4) - 1);

			if (mouse_directions == DIR_LEFT_DOWN) {
	
				input_report_rel(mouse_input_dev, REL_Y, ((mousecode & 0xFF) >> 4) + 1);
				mouse_directions = 0;
	
			} else {

				mouse_directions = DIR_LEFT_DOWN;
		
			}
	
			break;

		case 0x0A: /* left+down */
	
			input_report_rel(mouse_input_dev, REL_X, -((mousecode & 0xFF) >> 4) - 1);
			input_report_rel(mouse_input_dev, REL_Y, ((mousecode & 0xFF) >> 4) + 1);
			mouse_directions = 0;

			break;

		case 0x0B: /* down left */

			input_report_rel(mouse_input_dev, REL_Y, ((mousecode & 0xFF) >> 4) + 1);
	
			if (mouse_directions == DIR_DOWN_LEFT) {

				input_report_rel(mouse_input_dev, REL_X, -((mousecode & 0xFF) >> 4) - 1);
				mouse_directions = 0;

			} else {

				mouse_directions = DIR_DOWN_LEFT;
	
			}

			break;

		case 0x0C: /* down */

			input_report_rel(mouse_input_dev, REL_Y, ((mousecode & 0xFF) >> 4) + 1);
			mouse_directions = 0;

			break;

		case 0x0D: /* down right */

			input_report_rel(mouse_input_dev, REL_Y, ((mousecode & 0xFF) >> 4) + 1);
	
			if (mouse_directions == DIR_DOWN_RIGHT) {

				input_report_rel(mouse_input_dev, REL_X, ((mousecode & 0xFF) >> 4) + 1);
				mouse_directions = 0;
	
			} else {
			
				mouse_directions = DIR_DOWN_RIGHT;
		
			}
			
			break;

		case 0x0E: /* down+right */
		
			input_report_rel(mouse_input_dev, REL_X, ((mousecode & 0xFF) >> 4) + 1);
			input_report_rel(mouse_input_dev, REL_Y, ((mousecode & 0xFF) >> 4) + 1);
			mouse_directions = 0;
			
			break;

		case 0x0F: /* right down */

			input_report_rel(mouse_input_dev, REL_X, ((mousecode & 0xFF) >> 4) + 1);
		
			if (mouse_directions == DIR_RIGHT_DOWN) {
			
				input_report_rel(mouse_input_dev, REL_Y, ((mousecode & 0xFF) >> 4) + 1);
				mouse_directions = 0;
			
			} else {
			
				mouse_directions = DIR_RIGHT_DOWN;
		
			}
			
			break;

	}

}

int __init dbox2_fp_mouse_init(struct input_dev *input_dev)
{

	mouse_input_dev = input_dev;

	set_bit(EV_REL, mouse_input_dev->evbit);

	set_bit(REL_X, mouse_input_dev->relbit);
	set_bit(REL_Y, mouse_input_dev->relbit);

	dbox2_fp_queue_alloc(2, dbox2_fp_mouse_queue_handler);
	
	return 0;

}

void __exit dbox2_fp_mouse_exit(void)
{

	dbox2_fp_queue_free(2);

}
