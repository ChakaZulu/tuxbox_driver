/*
 * tuxbox_hardware_dbox2.c - TuxBox hardware info - dbox2
 *
 * Copyright (C) 2003 Florian Schirmer <jolt@tuxbox.org>
 *                    Bastian Blank <waldi@tuxbox.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Id: tuxbox_hardware_dbox2.c,v 1.2 2003/03/04 21:18:09 waldi Exp $
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <asm/io.h>

#include <tuxbox/tuxbox_hardware_dbox2.h>

extern struct proc_dir_entry *proc_bus_tuxbox;
struct proc_dir_entry *proc_bus_tuxbox_dbox2 = NULL;

tuxbox_dbox2_av_t tuxbox_dbox2_av;
tuxbox_dbox2_demod_t tuxbox_dbox2_demod;
u8 tuxbox_dbox2_fp_revision;
tuxbox_dbox2_mid_t tuxbox_dbox2_mid;

static struct i2c_driver dummy_i2c_driver =
{
	name: "TuxBox hardware info",
	id: I2C_DRIVERID_EXP2,
	flags: I2C_DF_NOTIFY,
};

static struct i2c_client dummy_i2c_client =
{
	name: "TuxBox hardware info - client",
	id: I2C_DRIVERID_EXP2,
	driver: &dummy_i2c_driver,
};

static int vendor_read (void)
{
	unsigned char *conf = (unsigned char *) ioremap (0x1001FFE0, 0x20);

	if (!conf) {
		printk("tuxbox: Could not remap memory\n");
		return -EIO;
	}

	tuxbox_dbox2_mid = conf[0];

	switch (tuxbox_dbox2_mid) {
		case TUXBOX_DBOX2_MID_NOKIA:
			tuxbox_vendor = TUXBOX_VENDOR_NOKIA;
			break;

		case TUXBOX_DBOX2_MID_PHILIPS:
			tuxbox_vendor = TUXBOX_VENDOR_PHILIPS;
			break;

		case TUXBOX_DBOX2_MID_SAGEM:
			tuxbox_vendor = TUXBOX_VENDOR_SAGEM;
			break;
	}

	iounmap (conf);

	return 0;
}

static u8 msg0[2];
static u8 msg1[1];
static struct i2c_msg msg[] = { { buf: msg0 },
				{ flags: I2C_M_RD, buf: msg1, len: 1 } };

static int frontend_probe_AT76C651_attach (struct i2c_adapter *adapter)
{
	dummy_i2c_client.adapter = adapter;

	msg[0].addr = msg[1].addr = dummy_i2c_client.addr = 0x0d;
	msg[0].len = 2;
	msg0[0] = 0;
	msg0[1] = 0x0e;

	if ((i2c_transfer(adapter, msg, 2) & 0xff) == 0x65)
		dummy_i2c_client.data = (void *) 1;
	else
		dummy_i2c_client.data = (void *) 0;

	return -1;
}

static int frontend_probe_VES1820_attach (struct i2c_adapter *adapter)
{
	dummy_i2c_client.adapter = adapter;

	msg[0].addr = msg[1].addr = dummy_i2c_client.addr = 0x08;
	msg[0].len = 1;
	msg0[0] = 0x1a;

	if ((i2c_transfer(adapter, msg, 2) & 0xf0) == 0x70)
		dummy_i2c_client.data = (void *) 1;
	else
		dummy_i2c_client.data = (void *) 0;

	return -1;
}

static int frontend_probe_VES1993_attach (struct i2c_adapter *adapter)
{
	dummy_i2c_client.adapter = adapter;

	msg[0].addr = msg[1].addr = dummy_i2c_client.addr = 0x08;
	msg[0].len = 1;
	msg0[0] = 0x1e;

	if ((i2c_transfer(adapter, msg, 2) & 0x65) == 0x65)
		dummy_i2c_client.data = (void *) 1;
	else
		dummy_i2c_client.data = (void *) 0;

	return -1;
}

static int frontend_probe (void)
{
	i2c_add_driver (&dummy_i2c_driver);
	i2c_del_driver (&dummy_i2c_driver);
	return (int) dummy_i2c_client.data;
}

static inline int frontend_probe_AT76C651 (void)
{
	dummy_i2c_driver.attach_adapter = &frontend_probe_AT76C651_attach;
	return frontend_probe ();
}

static inline int frontend_probe_VES1820 (void)
{
	dummy_i2c_driver.attach_adapter = &frontend_probe_VES1820_attach;
	return frontend_probe ();
}

static inline int frontend_probe_VES1993 (void)
{
	dummy_i2c_driver.attach_adapter = &frontend_probe_VES1993_attach;
	return frontend_probe ();
}

static int frontend_read (void)
{
	switch (tuxbox_vendor) {
		case TUXBOX_VENDOR_NOKIA:
			if (frontend_probe_VES1820 ())
			{
				tuxbox_frontend = TUXBOX_FRONTEND_CABLE;
				tuxbox_dbox2_demod = TUXBOX_DBOX2_DEMOD_VES1820;
			}
			else if (frontend_probe_VES1993 ())
			{
				tuxbox_frontend = TUXBOX_FRONTEND_SATELLITE;
				tuxbox_dbox2_demod = TUXBOX_DBOX2_DEMOD_VES1993;
			}
			else
			{
				tuxbox_frontend = TUXBOX_FRONTEND_SATELLITE;
				tuxbox_dbox2_demod = TUXBOX_DBOX2_DEMOD_VES1893;
			}

			tuxbox_dbox2_av = TUXBOX_DBOX2_AV_GTX;
			tuxbox_dbox2_fp_revision = 0x81;
			break;

		case TUXBOX_VENDOR_PHILIPS:
			tuxbox_frontend = TUXBOX_FRONTEND_SATELLITE;
			tuxbox_dbox2_demod = TUXBOX_DBOX2_DEMOD_TDA8044H;
			tuxbox_dbox2_av = TUXBOX_DBOX2_AV_ENX;
			tuxbox_dbox2_fp_revision = 0x30;
			break;

		case TUXBOX_VENDOR_SAGEM:
			if (frontend_probe_AT76C651 ())
			{
				tuxbox_frontend = TUXBOX_FRONTEND_CABLE;
				tuxbox_dbox2_demod = TUXBOX_DBOX2_DEMOD_AT76C651;
			}
			else
			{
				tuxbox_frontend = TUXBOX_FRONTEND_SATELLITE;
				tuxbox_dbox2_demod = TUXBOX_DBOX2_DEMOD_VES1x93;
			}

			tuxbox_dbox2_av = TUXBOX_DBOX2_AV_ENX;
			tuxbox_dbox2_fp_revision = 0x23;
			break;

		default:
			return -EINVAL;
	}

	return 0;
}

int tuxbox_hardware_read (void)
{
	int ret;

	tuxbox_model = TUXBOX_MODEL_DBOX2;
	tuxbox_submodel = TUXBOX_SUBMODEL_DBOX2;

	if ((ret = vendor_read ()))
		return ret;
	if ((ret = frontend_read ()))
		return ret;

	tuxbox_capabilities = TUXBOX_HARDWARE_DBOX2_CAPABILITIES;

	return 0;
}

int tuxbox_hardware_proc_create (void)
{
	if (!(proc_bus_tuxbox_dbox2 = proc_mkdir ("dbox2", proc_bus_tuxbox)))
		goto error;

	if (tuxbox_proc_create_entry ("av", 0444, proc_bus_tuxbox_dbox2, &tuxbox_dbox2_av, &tuxbox_proc_read, NULL))
		goto error;

	if (tuxbox_proc_create_entry ("demod", 0444, proc_bus_tuxbox_dbox2, &tuxbox_dbox2_demod, &tuxbox_proc_read, NULL))
		goto error;

	return 0;

error:
	printk("tuxbox: Could not create /proc/bus/tuxbox/dbox2\n");
	return -ENOENT;
}

void tuxbox_hardware_proc_destroy (void)
{
	remove_proc_entry ("av", proc_bus_tuxbox_dbox2);
	remove_proc_entry ("demod", proc_bus_tuxbox_dbox2);

	remove_proc_entry ("dbox2", proc_bus_tuxbox);
}

