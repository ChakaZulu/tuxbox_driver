/*
 * $Id: avia_napi.c,v 1.17 2003/11/14 21:20:27 carjay Exp $
 *
 * AViA GTX/eNX dvb api driver
 *
 * Homepage: http://www.tuxbox.org
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "../dvb-core/dvbdev.h"
#include "../dvb-core/dvb_i2c_bridge.h"

static struct dvb_adapter *adap;

struct dvb_adapter *avia_napi_get_adapter(void)
{
	return adap;
}

static
int __init avia_napi_init(void)
{
	int result;

	printk(KERN_INFO "$Id: avia_napi.c,v 1.17 2003/11/14 21:20:27 carjay Exp $\n");
	
	if ((result = dvb_register_adapter(&adap, "C-Cube AViA GTX/eNX with AViA 500/600")) < 0) {
		printk(KERN_ERR "avia_napi: dvb_register_adapter failed (errno = %d)\n", result);
		return result;
	}

	if ((result = dvb_i2c_bridge_register(adap)) < 0) {
		printk(KERN_ERR "avia_napi: dvb_i2c_bridge_register failed (errno = %d)\n", result);
		dvb_unregister_adapter(adap);
		return result;
	}

	return 0;

}

static
void __exit avia_napi_exit(void)
{
	dvb_i2c_bridge_unregister(adap);
	dvb_unregister_adapter(adap);
}

module_init(avia_napi_init);
module_exit(avia_napi_exit);

MODULE_DESCRIPTION("AViA dvb adapter driver");
MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_LICENSE("GPL");
