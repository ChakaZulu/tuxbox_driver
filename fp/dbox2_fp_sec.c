/*
 * $Id: dbox2_fp_sec.c,v 1.5 2003/03/04 21:18:09 waldi Exp $
 *
 * Copyright (C) 2002-2003 Andreas Oberritter <obi@tuxbox.org>
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
#include <linux/sched.h>
#include <linux/string.h>

#include <dbox/dbox2_fp_core.h>
#include <dbox/dbox2_fp_sec.h>

#include <tuxbox/tuxbox_hardware_dbox2.h>

static u8 sec_power;
static u8 sec_voltage;
static u8 sec_high_voltage;
static u8 sec_tone;
static int sec_bus_status;
struct i2c_client * fp_i2c_client;


int
dbox2_fp_sec_get_status (void)
{
	/* < 0 means error: -1 for bus overload, -2 for busy */
	return sec_bus_status;
}


int
dbox2_fp_sec_diseqc_cmd (u8 *cmd, u8 len)
{
	unsigned char msg[8];
	unsigned char status_cmd;
	int c;
	int sleep_perbyte;
	int sleeptime;

	if (sec_bus_status < 0)
		return -1;

	/*
	 * these values are measured/calculated for nokia
	 * dunno wether sagem needs longer or not
	 */
	sleeptime = 2300;
	sleep_perbyte = 300;

	switch (tuxbox_dbox2_mid) {
	case TUXBOX_DBOX2_MID_NOKIA:
		msg[0] = 0x00;
		msg[1] = 0x1B;
		status_cmd = 0x2D;
		break;

	case TUXBOX_DBOX2_MID_SAGEM:
		msg[0] = 0x00;
		msg[1] = 0x25; //28
		status_cmd = 0x22;
		break;

	default:
		return -1;
	}

	if (len > sizeof(msg) - 2)
		len = sizeof(msg) - 2;

	memcpy(msg + 2, cmd, len);

	if (tuxbox_dbox2_mid == TUXBOX_DBOX2_MID_SAGEM) {

		if (len > 1) {
			i2c_master_send(fp_i2c_client, msg, len + 2);
			udelay(1000*100); /* <- ;) */
		}

		return 0;
	}

	sec_bus_status = -2;
	i2c_master_send(fp_i2c_client, msg, 2 + len);

	current->state = TASK_INTERRUPTIBLE;
	schedule_timeout((sleeptime + (len * sleep_perbyte)) / HZ);

	for (c = 1; c <= 5; c++) {

		fp_cmd(fp_i2c_client, status_cmd, msg, 1);

		if (!msg[0])
			break;

		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(sleep_perbyte / HZ);
	}

	sec_bus_status = 0;

	if (c == 5)
		return -1;
	else
		return 0;
}


/*
 * power
 *  0: off
 * !0: on
 *
 * volt
 *  0: 13V
 * !0: 18V
 *
 * high
 *  0: +0V
 * !0: +1V
 *
 * tone
 *  0:  0kHz
 * !0: 22kHz
 *
 */

static int
dbox2_fp_sec_set (u8 power, u8 voltage, u8 high_voltage, u8 tone)
{
	u8 msg[2];

	sec_power = power;
	sec_voltage = voltage;
	sec_high_voltage = high_voltage;
	sec_tone = tone;

	if (sec_bus_status < 0)
		return -1;

	switch (tuxbox_dbox2_mid) {
	case TUXBOX_DBOX2_MID_NOKIA:

		/* power   pol   +1V
		 * ----------------------
		 * 0x40 | 0x00 | 0x00 19V
		 * 0x40 | 0x00 | 0x10 18V
		 * 0x40 | 0x20 | 0x00 14V
		 * 0x40 | 0x20 | 0x10 13V
		 */

		msg[0] = 0x21;
		msg[1] = ((!!power) << 6) | ((!voltage) << 5) | ((!!high_voltage) << 4) | (!!tone);
		break;

	case TUXBOX_DBOX2_MID_SAGEM:

		/*
		 * power   pol   +1V
		 * ----------------------
		 * 0x40 | 0x20 | 0x10 19V
		 * 0x40 | 0x20 | 0x00 18V
		 * 0x40 | 0x00 | 0x10 14V
		 * 0x40 | 0x00 | 0x00 13V
		 */

		msg[0] = 0x04;
		msg[1] = ((!!power) << 6) | ((!!voltage) << 5) | ((!!high_voltage) << 4) | (!!tone);
		break;

	default:
		return -1;
	}

	sec_bus_status = -1;

	if (i2c_master_send(fp_i2c_client, msg, sizeof(msg)) != sizeof(msg))
		return -1;

	sec_bus_status = 0;

	return 0;
}


int
dbox2_fp_sec_set_power (u8 power)
{
	return dbox2_fp_sec_set(power, sec_voltage, sec_high_voltage, sec_tone);
}


int
dbox2_fp_sec_set_voltage (u8 voltage)
{
	return dbox2_fp_sec_set(sec_power, voltage, sec_high_voltage, sec_tone);
}


int
dbox2_fp_sec_set_tone (u8 tone)
{
	return dbox2_fp_sec_set(sec_power, sec_voltage, sec_high_voltage, tone);
}


int
dbox2_fp_sec_set_high_voltage (u8 high_voltage)
{
	return dbox2_fp_sec_set(sec_power, sec_voltage, high_voltage, sec_tone);
}


void
dbox2_fp_sec_init (void)
{
	sec_bus_status = 0;
	fp_i2c_client = fp_get_i2c();

	dbox2_fp_sec_set(0, 0, 0, 0);
}


EXPORT_SYMBOL(dbox2_fp_sec_diseqc_cmd);
EXPORT_SYMBOL(dbox2_fp_sec_get_status);
EXPORT_SYMBOL(dbox2_fp_sec_set_high_voltage);
EXPORT_SYMBOL(dbox2_fp_sec_set_power);
EXPORT_SYMBOL(dbox2_fp_sec_set_tone);
EXPORT_SYMBOL(dbox2_fp_sec_set_voltage);

