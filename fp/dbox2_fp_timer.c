/*
 * $Id: dbox2_fp_timer.c,v 1.6 2002/12/03 00:19:29 Zwen Exp $
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

#include <linux/types.h>

#include <dbox/dbox2_fp_core.h>
#include <dbox/dbox2_fp_timer.h>

static u8 boot_trigger;
static u8 manufacturer_id;
static struct i2c_client *fp_i2c_client;


void
dbox2_fp_timer_init (void)
{
	boot_trigger = BOOT_TRIGGER_USER;
	manufacturer_id = fp_get_info()->mID;
	fp_i2c_client = fp_get_i2c();
	dbox2_fp_timer_clear();
}


u8
dbox2_fp_timer_get_boot_trigger (void)
{
	return boot_trigger;
}


int
dbox2_fp_timer_set (u16 minutes)
{
	u8 cmd [] = { 0x00, minutes & 0xFF, minutes >> 8 };

	switch (manufacturer_id) {
	case DBOX_MID_NOKIA:
		cmd[0] = FP_WAKEUP_NOKIA;
		break;
	case DBOX_MID_PHILIPS:
		cmd[0] = FP_WAKEUP_PHILIPS;
		break;
	case DBOX_MID_SAGEM:
		cmd[0] = FP_WAKEUP_SAGEM;
		break;
	}

	if (i2c_master_send(fp_i2c_client, cmd, sizeof(cmd)) != sizeof(cmd))
		return -1;

	return 0;
}


int
dbox2_fp_timer_get (void)
{
	u8 id [] ={ 0x00, 0x00 };
	u8 cmd;

	switch (manufacturer_id) {
	case DBOX_MID_NOKIA:
		cmd = FP_WAKEUP_NOKIA;
		break;
	case DBOX_MID_PHILIPS:
		cmd = FP_WAKEUP_PHILIPS;
		break;
	case DBOX_MID_SAGEM:
		cmd = FP_WAKEUP_SAGEM;
		break;
	}

	if (fp_cmd(fp_i2c_client, cmd, id, sizeof(id)))
		return -1;

	return ((id[0] + id[1]) << 8);
}


int
dbox2_fp_timer_clear (void)
{
	u8 id [] = { 0x00, 0x00 };
	u8 cmd;

	// clear wakeup timer (neccesary to clear any timer when booted manually)
	dbox2_fp_timer_set(0);

	// this cmd reads the boot trigger & restores the normal shutdown behaviour for sagem/phillips
	cmd = FP_CLEAR_WAKEUP;
	if (fp_cmd(fp_i2c_client, cmd, id, sizeof(id)))
		return -1;
	boot_trigger = (id[0] == 0x80) ? BOOT_TRIGGER_TIMER : BOOT_TRIGGER_USER;

	// nokia needs an additional read to restore normal shutdown behavior
	if(manufacturer_id==DBOX_MID_NOKIA)
	{
		cmd = FP_CLEAR_WAKEUP_NOKIA;
		if (fp_cmd(fp_i2c_client, cmd, id, sizeof(id)))
			return -1;
	}

	return 0;
}

