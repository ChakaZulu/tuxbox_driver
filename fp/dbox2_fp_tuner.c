/*
 * $Id: dbox2_fp_tuner.c,v 1.1 2002/10/21 11:38:58 obi Exp $
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


static u8 frontend_type;
static struct i2c_client * fp_i2c_client;


void
dbox2_fp_tuner_init (void)
{
	frontend_type = fp_get_info()->fe;
	fp_i2c_client = fp_get_i2c();
}


int
dbox2_fp_tuner_write (u8 *buf, u8 len)
{
	u8 msg [len + 3];

	switch (frontend_type) {
	case DBOX_FE_CABLE:
		msg[0] = 0x00;
		msg[1] = 0x07;
		msg[2] = 0xc0;
		memcpy(msg + 3, buf, len);
		len += 3;
		break;

	case DBOX_FE_SAT:
		msg[0] = 0x00;
		msg[1] = 0x05;
		memcpy(msg + 2, buf, len);
		len += 2;
		break;
	}

	if (i2c_master_send(fp_i2c_client, msg, len) != len)
		return -1;

	return 0;
}

#ifdef MODULE
EXPORT_SYMBOL(dbox2_fp_tuner_write);
#endif
