/*
 * $Id: dbox2_fp_irkbd.c,v 1.1 2002/10/21 11:38:58 obi Exp $
 *
 * Copyright (C) 2002 by Andreas Oberritter <obi@tuxbox.org>
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

#include <linux/kbd_ll.h>

#include <dbox/dbox2_fp_core.h>
#include <dbox/dbox2_fp_irkbd.h>


#ifdef CONFIG_INPUT_MODULE
struct input_dev dbox2_fp_irkbd;
#endif


#define dprintk(...)


static unsigned char
keymap[256] =
{
  0,   1,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,   0,   0,   0,   0,
  0,   0,   0,   0,  41,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,
 13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  43,
 58,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  28,  42,  44,  45,
 46,  47,  48,  49,  50,  51,  52,  53,  54,   0,  29, 125,  56,  57, 100,   0,
127,   0,  99,  70, 119, 110,   0,   0, 111,   0,   0,   0,   0,   0,   0, 103,
 86, 105, 108, 106,  69,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  /* Fn Keymap starts here */
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 104,
  0, 102, 109, 107,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};


unsigned char fn_flags[128];
unsigned char irkbd_flags = 0;

#ifdef CONFIG_INPUT_MODULE
unsigned char mouse_directions = 0;
#endif /* CONFIG_INPUT_MODULE */


static u8 manufacturer_id;


void
dbox2_fp_irkbd_init (void)
{
	manufacturer_id = fp_get_info()->mID;
	memset(fn_flags, 0, sizeof(fn_flags));

#ifdef CONFIG_INPUT_MODULE
	memset(&dbox2_fp_irkbd, 0, sizeof(dbox2_fp_irkbd));
	dbox2_fp_irkbd.evbit[0] = BIT(EV_KEY) | BIT(EV_REL); /*BIT(EV_LED) | BIT(EV_REP)*/
	/*dbox2_fp_irkbd.ledbit[0] = BIT(LED_NUML) | BIT(LED_CAPSL) | BIT(LED_SCROLLL);*/
	/*for(i = 0; i <= 255; i++) set_bit(keymap[i], dbox2_fp_irkbd.keybit);*/
	dbox2_fp_irkbd.keybit[LONG(BTN_MOUSE)] = BIT(BTN_LEFT) | BIT(BTN_RIGHT);
	dbox2_fp_irkbd.relbit[0] = BIT(REL_X) | BIT(REL_Y);
	dbox2_fp_irkbd.event = dbox2_fp_irkbd_event;
	dbox2_fp_irkbd.name = "dbox2 infrared keyboard";
	dbox2_fp_irkbd.idbus = BUS_I2C;
	input_register_device(&dbox2_fp_irkbd);
#endif /* CONFIG_INPUT_MODULE */
}


void
dbox2_fp_irkbd_exit (void)
{
#ifdef CONFIG_INPUT_MODULE
	input_unregister_device(&dbox2_fp_irkbd);
#endif
}


int
dbox2_fp_irkbd_setkeycode (unsigned int scancode, unsigned int keycode)
{
	if ((scancode > 255) || (keycode > 127))
		return -EINVAL;

	keymap[scancode & 0xFF] = keycode;

	return 0;
}


int
dbox2_fp_irkbd_getkeycode (unsigned int scancode)
{
	return keymap[scancode & 0xFF];
}


int
dbox2_fp_irkbd_translate (unsigned char scancode, unsigned char * keycode, char raw_mode)
{
	/* Fn toggled */
	if ((scancode & 0x7f) == 0x49) {

		if (scancode == 0x49)
			irkbd_flags |= IRKBD_FN;
		else
			irkbd_flags &= ~IRKBD_FN;

		return 0;
	}

	/* mouse button changed */
	if (((scancode & 0x7f) == 0x7e) || ((scancode & 0x7f) == 0x7f))
		return 0;

	if (irkbd_flags & IRKBD_FN) {

		*keycode = keymap[(scancode & 0x7f) | IRKBD_FN];

		/* Fn pressed, other key released */
		if (scancode & 0x80) {

			/* fn pressed, other key released which got pressed before fn */
			if (!fn_flags[scancode & 0x7f]) {
				*keycode = keymap[scancode & 0x7f];
			}

			/* fn pressed, other key released which got pressed during fn */
			else {
				fn_flags[scancode & 0x7f] = 0;
			}
		}

		/* Fn + other key pressed */
		else {
			fn_flags[scancode & 0x7f] = 1;
		}

	}

	/* key got pressed during fn and gets now released after fn has been released */
	else if ((!(irkbd_flags & IRKBD_FN)) && (fn_flags[scancode & 0x7f])) {

		*keycode = keymap[(scancode & 0x7f) | IRKBD_FN];
		fn_flags[scancode & 0x7f] = 0;

	}

	/* no Fn - other key pressed/released */
	else {
		*keycode = keymap[scancode & 0x7f];
	}

	if (*keycode == 0) {

		if (!raw_mode)
			dprintk("fp.o: irkbd: unknown scancode 0x%02X (flags 0x%02X)\n", scancode, irkbd_flags);

		return 0;

	}

	return 1;
}


char
dbox2_fp_irkbd_unexpected_up (unsigned char keycode)
{
	dprintk("fp.o: irkbd_unexpected_up 0x%02X (flags 0x%02X)\n", keycode, irkbd_flags);
	return 0200;
}


void
dbox2_fp_irkbd_leds (unsigned char leds)
{
	dprintk("fp.o: irkbd_leds 0x%02X\n", leds);
}


void
__init dbox2_fp_irkbd_init_hw (void)
{
	dprintk("fp.o: IR-Keyboard initialized\n");
}


#ifdef CONFIG_INPUT_MODULE
int
dbox2_fp_irkbd_event (struct input_dev * dev, unsigned int type, unsigned int code, int value)
{
	/*
	if ((type == EV_SND) && (code == EV_BELL)) {
		printk("BEEP!\n");
		return 0;
	}
	*/

	if (type == EV_LED) {
		dprintk("IR-Keyboard LEDs: [%s|%s|%s]\n",
				(test_bit(LED_NUML,dev->led) ? " NUM " : "     "),
				(test_bit(LED_CAPSL,dev->led) ? "CAPS " : "     "),
				(test_bit(LED_SCROLLL,dev->led) ? "SCROLL" : "     "));
		return 0;
	}

	return -1;
}
#endif



/*****************************************************************************\
 *   Interrupt Handler Functions
\*****************************************************************************/


void
dbox2_fp_handle_ir_keyboard (struct fp_data * dev)
{
	u16 scancode = 0xffff;
	u8 keycode = 0;

	switch (manufacturer_id) {
	case DBOX_MID_NOKIA:
		fp_cmd(dev->client, 0x03, (u8 *) &scancode, 2);
		break;
	case DBOX_MID_PHILIPS:
	case DBOX_MID_SAGEM:
		fp_cmd(dev->client, 0x28, (u8 *) &scancode, 2);
		break;
	}

	dprintk("keyboard scancode: %02x\n", scancode);

#ifdef CONFIG_INPUT_MODULE
	/* mouse buttons */
	if (((scancode & 0x7f) == 0x7e) || ((scancode & 0x7f) == 0x7f)) {
		
		if ((scancode & 0x7f) == 0x7e)
			input_report_key(&dbox2_fp_irkbd, BTN_RIGHT, !(scancode & 0x80));
		else
			input_report_key(&dbox2_fp_irkbd, BTN_LEFT, !(scancode & 0x80));

		return;

	}
#endif

	handle_scancode(scancode & 0xFF, !((scancode & 0xFF) & 0x80));

	/* no inputdev yet */
	return;

	/* Fn toggled */
	if ((scancode & 0x7f) == 0x49) {

		if (scancode == 0x49)
			irkbd_flags |= IRKBD_FN;
		else
			irkbd_flags &=~ IRKBD_FN;
	
		return;
	}

	if (irkbd_flags & IRKBD_FN) {

		keycode = keymap[(scancode & 0x7f) | IRKBD_FN];

		/* Fn pressed, other key released */
		if (scancode & 0x80) {

			/* fn pressed, other key released which got pressed before fn */
			if (!fn_flags[scancode & 0x7f]) {
				keycode=keymap[scancode & 0x7f];
			}

			/* fn pressed, other key released which got pressed during fn */
			else {
				fn_flags[scancode & 0x7f] = 0;
			}
		}

		/* Fn + other key pressed */
		else {
			fn_flags[scancode & 0x7f] = 1;
		}
	}

	/* key got pressed during fn and gets now released after fn has been released*/
	else if ((!(irkbd_flags & IRKBD_FN)) && (fn_flags[scancode & 0x7f])) {

		keycode = keymap[(scancode & 0x7f) | IRKBD_FN];
		fn_flags[scancode & 0x7f] = 0;

	}

	/* no Fn - other key pressed/released */
	else {
		keycode=keymap[scancode & 0x7f];
	}

	if (keycode == 0) {
		dprintk("fp.o: irkbd: unknown scancode 0x%02X (flags 0x%02X)\n", scancode, irkbd_flags);
	}

#ifdef CONFIG_INPUT_MODULE
	else {
		input_report_key(&dbox2_fp_irkbd, keycode, !(scancode & 0x80));
	}
#endif
}


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


void
dbox2_fp_handle_ir_mouse (struct fp_data * dev)
{
	u16 mousecode = 0xffff;

	switch (manufacturer_id) {
	case DBOX_MID_NOKIA:
		fp_cmd(dev->client, 0x05, (u8 *) &mousecode, 2);
		break;
	case DBOX_MID_PHILIPS:
	case DBOX_MID_SAGEM:
		fp_cmd(dev->client, 0x2A, (u8 *) &mousecode, 2);
		break;
	}

#ifdef CONFIG_INPUT_MODULE
	switch (mousecode & 0x0F) {
	case 0x00: /* right */
		input_report_rel(&dbox2_fp_irkbd, REL_X, ((mousecode & 0xFF) >> 4) + 1);
		mouse_directions = 0;
		break;

	case 0x01: /* right up */
		input_report_rel(&dbox2_fp_irkbd, REL_X, ((mousecode & 0xFF) >> 4) + 1);
		if (mouse_directions == DIR_RIGHT_UP) {
			input_report_rel(&dbox2_fp_irkbd, REL_Y, -((mousecode & 0xFF) >> 4) - 1);
			mouse_directions = 0;
		}
		else {
			mouse_directions = DIR_RIGHT_UP;
		}
		break;

	case 0x02: /* right+up */
		input_report_rel(&dbox2_fp_irkbd, REL_X, ((mousecode & 0xFF) >> 4) + 1);
		input_report_rel(&dbox2_fp_irkbd, REL_Y, -((mousecode & 0xFF) >> 4) - 1);
		mouse_directions = 0;
		break;

	case 0x03: /* up right */
		input_report_rel(&dbox2_fp_irkbd, REL_Y, -((mousecode & 0xFF) >> 4) - 1);
		if (mouse_directions == DIR_UP_RIGHT) {
			input_report_rel(&dbox2_fp_irkbd, REL_X, ((mousecode & 0xFF) >> 4) + 1);
			mouse_directions = 0;
		}
		else {
			mouse_directions = DIR_UP_RIGHT;
		}
		break;

	case 0x04: /* up */
		input_report_rel(&dbox2_fp_irkbd, REL_Y, -((mousecode & 0xFF) >> 4) - 1);
		mouse_directions = 0;
		break;

	case 0x05: /* up left */
		input_report_rel(&dbox2_fp_irkbd, REL_Y, -((mousecode & 0xFF) >> 4) - 1);
		if (mouse_directions==DIR_UP_LEFT) {
			input_report_rel(&dbox2_fp_irkbd, REL_X, -((mousecode & 0xFF) >> 4) - 1);
			mouse_directions = 0;
		}
		else {
			mouse_directions=DIR_UP_LEFT;
		}
		break;

	case 0x06: /* up+left */
		input_report_rel(&dbox2_fp_irkbd, REL_X, -((mousecode & 0xFF) >> 4) - 1);
		input_report_rel(&dbox2_fp_irkbd, REL_Y, -((mousecode & 0xFF) >> 4) - 1);
		mouse_directions = 0;
		break;

	case 0x07: /* left up */
		input_report_rel(&dbox2_fp_irkbd, REL_X, -((mousecode & 0xFF) >> 4) - 1);
		if (mouse_directions == DIR_LEFT_UP) {
			input_report_rel(&dbox2_fp_irkbd, REL_Y, -((mousecode & 0xFF) >> 4) - 1);
			mouse_directions = 0;
		}
		else {
			mouse_directions = DIR_LEFT_UP;
		}
		break;

	case 0x08: /* left */
		input_report_rel(&dbox2_fp_irkbd, REL_X, -((mousecode & 0xFF) >> 4) - 1);
		mouse_directions = 0;
		break;

	case 0x09: /* left down */
		input_report_rel(&dbox2_fp_irkbd, REL_X, -((mousecode & 0xFF) >> 4) - 1);
		if (mouse_directions == DIR_LEFT_DOWN) {
			input_report_rel(&dbox2_fp_irkbd, REL_Y, ((mousecode & 0xFF) >> 4) + 1);
			mouse_directions = 0;
		}
		else {
			mouse_directions = DIR_LEFT_DOWN;
		}
		break;

	case 0x0A: /* left+down */
		input_report_rel(&dbox2_fp_irkbd, REL_X, -((mousecode & 0xFF) >> 4) - 1);
		input_report_rel(&dbox2_fp_irkbd, REL_Y, ((mousecode & 0xFF) >> 4) + 1);
		mouse_directions = 0;
		break;

	case 0x0B: /* down left */
		input_report_rel(&dbox2_fp_irkbd, REL_Y, ((mousecode & 0xFF) >> 4) + 1);
		if (mouse_directions == DIR_DOWN_LEFT) {
			input_report_rel(&dbox2_fp_irkbd, REL_X, -((mousecode & 0xFF) >> 4) - 1);
			mouse_directions = 0;
		}
		else {
			mouse_directions = DIR_DOWN_LEFT;
		}
		break;

	case 0x0C: /* down */
		input_report_rel(&dbox2_fp_irkbd, REL_Y, ((mousecode & 0xFF) >> 4) + 1);
		mouse_directions = 0;
		break;

	case 0x0D: /* down right */
		input_report_rel(&dbox2_fp_irkbd, REL_Y, ((mousecode & 0xFF) >> 4) + 1);
		if (mouse_directions == DIR_DOWN_RIGHT) {
			input_report_rel(&dbox2_fp_irkbd, REL_X, ((mousecode & 0xFF) >> 4) + 1);
			mouse_directions = 0;
		}
		else {
			mouse_directions = DIR_DOWN_RIGHT;
		}
		break;

	case 0x0E: /* down+right */
		input_report_rel(&dbox2_fp_irkbd, REL_X, ((mousecode & 0xFF) >> 4) + 1);
		input_report_rel(&dbox2_fp_irkbd, REL_Y, ((mousecode & 0xFF) >> 4) + 1);
		mouse_directions = 0;
		break;

	case 0x0F: /* right down */
		input_report_rel(&dbox2_fp_irkbd, REL_X, ((mousecode & 0xFF) >> 4) + 1);
		if (mouse_directions == DIR_RIGHT_DOWN) {
			input_report_rel(&dbox2_fp_irkbd, REL_Y, ((mousecode & 0xFF) >> 4) + 1);
			mouse_directions = 0;
		}
		else {
			mouse_directions = DIR_RIGHT_DOWN;
		}
		break;
	}
#endif
}

