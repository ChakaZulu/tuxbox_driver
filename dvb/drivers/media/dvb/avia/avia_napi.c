/*
 * $Id: avia_napi.c,v 1.6 2002/11/11 02:56:27 obi Exp $
 *
 * AViA GTX/eNX NAPI driver
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

#include "../dvb-core/dmxdev.h"
#include "../dvb-core/dvbdev.h"
#include "../dvb-core/dvb_demux.h"
#include "../dvb-core/dvb_frontend.h"
#include "../dvb-core/dvb_i2c.h"

#include "avia_av_napi.h"
#include "avia_gt_napi.h"

static struct dvb_adapter *adap;
static struct dvb_i2c_bus *i2c_bus;

int i2c_dbox2_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[], int num);

static int avia_napi_i2c_master_xfer(struct dvb_i2c_bus *i2c, struct i2c_msg msgs[], int num)
{

//	printk("avia_napi: i2c_master_xfer\n");
	
	return i2c_dbox2_xfer(NULL, msgs, num);

}

struct dvb_adapter *avia_napi_get_adapter(void)
{

	return adap;
	
}

static int __init avia_napi_init(void)
{

	int result;

	printk("$Id: avia_napi.c,v 1.6 2002/11/11 02:56:27 obi Exp $\n");
	
	if ((result = dvb_register_adapter(&adap, "C-Cube AViA GTX/eNX with AViA 500/600")) < 0) {
	
		printk("avia_napi: dvb_register_adapter failed (errno = %d)\n", result);
		
		return result;
		
	}

	if (!(i2c_bus = dvb_register_i2c_bus(avia_napi_i2c_master_xfer, NULL, adap, 0))) {

		printk("avia_napi: dvb_register_i2c_bus failed\n");

		dvb_unregister_adapter(adap);
	
		return -ENOMEM;
	
	}

	if ((result = avia_av_napi_register(adap, NULL)) < 0) {
	
		printk("avia_napi: avia_av_napi_register failed (errno = %d)\n", result);

		dvb_unregister_i2c_bus(avia_napi_i2c_master_xfer, adap, 0);
		dvb_unregister_adapter(adap);
		
		return result;
		
	}

//FIXME	dvb_net_register();

	return 0;

}

static void __exit avia_napi_exit(void)
{

//FIXME	dvb_net_release(&net);

	avia_av_napi_unregister();
	dvb_unregister_i2c_bus(avia_napi_i2c_master_xfer, adap, 0);
	dvb_unregister_adapter(adap);

}

#ifdef MODULE
module_init(avia_napi_init);
module_exit(avia_napi_exit);
MODULE_DESCRIPTION("dummy dvb adapter driver");
MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#endif
