/*
 * $Id: dbox2_fp_reset.c,v 1.4 2003/03/05 09:52:17 waldi Exp $
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


#include <linux/delay.h>
#include <linux/module.h>

#include <dbox/dbox2_fp_core.h>
#include <dbox/dbox2_fp_reset.h>

static struct i2c_client * fp_i2c_client;

void
dbox2_fp_reset_init (void)
{
	fp_i2c_client = fp_get_i2c();
}


void
dbox2_fp_restart (char * cmd)
{
	switch (mid) {
	case TUXBOX_DBOX2_MID_NOKIA:
		fp_sendcmd(fp_i2c_client, 0x00, 0x14);
		break;
	case TUXBOX_DBOX2_MID_PHILIPS:
	case TUXBOX_DBOX2_MID_SAGEM:
		fp_sendcmd(fp_i2c_client, 0x00, 0x09);
		break;
	}

	for (;;);
}


void
dbox2_fp_power_off (void)
{
	switch (mid) {
	case TUXBOX_DBOX2_MID_NOKIA:
		fp_sendcmd(fp_i2c_client, 0x00, 0x03);
		break;
	case TUXBOX_DBOX2_MID_PHILIPS:
	case TUXBOX_DBOX2_MID_SAGEM:
		fp_sendcmd(fp_i2c_client, 0x00, 0x00);
		break;
	}

	for (;;);
}


int
dbox2_fp_reset_cam (void) /* needed for sagem / philips? */
{
	u8 msg [] = { 0x05, 0xef };

	if (mid == TUXBOX_DBOX2_MID_NOKIA) {
	
		return dbox2_fp_reset(0xAF);
		
	} else {

		if (i2c_master_send(fp_i2c_client, msg, sizeof(msg)) != sizeof(msg))
			return -1;

		msg[1] = 0xff;

		if (i2c_master_send(fp_i2c_client, msg, sizeof(msg)) != sizeof(msg))
			return -1;

		return 0;
			
	}

}


int
dbox2_fp_reset (u8 type)
{
	u8 msg [] = { 0x22, type };

	if (i2c_master_send(fp_i2c_client, msg, sizeof(msg)) != sizeof(msg))
		return -1;

	/* TODO: make better */
	udelay(1000);

	msg[1] = 0xbf;

	if (i2c_master_send(fp_i2c_client, msg, sizeof(msg)) != sizeof(msg))
		return -1;

	return 0;
}

EXPORT_SYMBOL(dbox2_fp_reset);
EXPORT_SYMBOL(dbox2_fp_reset_cam);
