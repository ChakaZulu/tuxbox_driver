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
 * $Id: tuxbox_hardware_dbox2.c,v 1.1 2003/02/19 16:38:05 waldi Exp $
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <asm/io.h>

#include <tuxbox/tuxbox_hardware_dbox2.h>

static int vendor_read (void)
{
	unsigned char *conf = (unsigned char *) ioremap (0x1001FFE0, 0x20);

	if (!conf) {
		printk("tuxbox: Could not remap memory\n");
		return -EIO;
	}

	switch (conf[0]) {
		case TUXBOX_HARDWARE_DBOX2_MID_NOKIA:
			tuxbox_vendor = TUXBOX_VENDOR_NOKIA;
			break;

		case TUXBOX_HARDWARE_DBOX2_MID_SAGEM:
			tuxbox_vendor = TUXBOX_VENDOR_SAGEM;
			break;

		case TUXBOX_HARDWARE_DBOX2_MID_PHILIPS:
			tuxbox_vendor = TUXBOX_VENDOR_PHILIPS;
			break;
	}

	iounmap (conf);

	return 0;
}

int tuxbox_hardware_read (void)
{
	int ret;

	tuxbox_model = TUXBOX_MODEL_DBOX2;
	tuxbox_submodel = TUXBOX_SUBMODEL_DBOX2;

	if ((ret = vendor_read ()))
		return ret;

	tuxbox_capabilities = TUXBOX_HARDWARE_DBOX2_CAPABILITIES;

	return 0;
}

