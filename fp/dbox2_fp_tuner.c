/*
 * $Id: dbox2_fp_tuner.c,v 1.3 2003/03/04 23:05:30 waldi Exp $
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

#include <linux/module.h>
#include <linux/string.h>

#include <dbox/dbox2_fp_core.h>
#include <dbox/dbox2_fp_tuner.h>

#include <tuxbox/tuxbox_hardware.h>

static struct i2c_client * fp_i2c_client;


void
dbox2_fp_tuner_init (void)
{
	fp_i2c_client = fp_get_i2c();
}


int
dbox2_fp_tuner_write_qam (u8 *buf, u8 len)
{
	u8 msg [len + 3];

	msg[0] = 0x00;
	msg[1] = 0x07;
	msg[2] = 0xc0;
	memcpy(msg + 3, buf, len);
	len += 3;

	if (i2c_master_send(fp_i2c_client, msg, len) != len)
		return -1;

	return 0;
}

int
dbox2_fp_tuner_write_qpsk (u8 *buf, u8 len)
{
	u8 msg [len + 2];

	msg[0] = 0x00;
	msg[1] = 0x05;
	memcpy(msg + 2, buf, len);
	len += 2;

	if (i2c_master_send(fp_i2c_client, msg, len) != len)
		return -1;

	return 0;
}

#ifdef MODULE
EXPORT_SYMBOL(dbox2_fp_tuner_write_qam);
EXPORT_SYMBOL(dbox2_fp_tuner_write_qpsk);
#endif
