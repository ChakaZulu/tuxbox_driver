/*
 * $Id: dbox2_i2c_napi.c,v 1.1 2002/11/11 06:26:35 Jolt Exp $
 *
 * MPC8xx DVB I2C interface
 *
 * Homepage: http://dbox2.elxsi.de
 *
 * Copyright (C) 2002 Florian Schirmer <jolt@tuxbox.org>
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

#include <asm/bitops.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include "../dvb/drivers/media/dvb/dvb-core/dvbdev.h"
#include "../dvb/drivers/media/dvb/dvb-core/dvb_i2c.h"
#include "../dvb/drivers/media/dvb/avia/avia_napi.h"
#include "dbox2_i2c.h"

static struct dvb_i2c_bus *i2c_bus;

static int dbox2_i2c_napi_master_xfer(struct dvb_i2c_bus *i2c, struct i2c_msg msgs[], int num)
{

	return dbox2_i2c_xfer(NULL, msgs, num);

}

static int __init dbox2_i2c_napi_init(void)
{

	printk("$Id: dbox2_i2c_napi.c,v 1.1 2002/11/11 06:26:35 Jolt Exp $\n");
	
	if (!(i2c_bus = dvb_register_i2c_bus(dbox2_i2c_napi_master_xfer, NULL, avia_napi_get_adapter(), 0))) {

		printk("avia_napi: dvb_register_i2c_bus failed\n");

		return -ENOMEM;
	
	}

	return 0;

}

static void __exit dbox2_i2c_napi_exit(void)
{

	dvb_unregister_i2c_bus(dbox2_i2c_napi_master_xfer, avia_napi_get_adapter(), 0);

}

#ifdef MODULE
module_init(dbox2_i2c_napi_init);
module_exit(dbox2_i2c_napi_exit);
MODULE_DESCRIPTION("MPC8xx I2C DVB driver");
MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#endif
