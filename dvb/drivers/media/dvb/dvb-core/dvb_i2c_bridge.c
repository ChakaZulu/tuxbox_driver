/*
 * $Id: dvb_i2c_bridge.c,v 1.1 2003/01/04 11:05:31 Jolt Exp $
 *
 * DVB I2C bridge
 *
 * Copyright (C) 2002 Florian Schirmer <schirmer@taytron.net>
 * Copyright (C) 2002 Felix Domke <tmbinc@elitedvb.net>
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
#include <linux/init.h>
#include <linux/i2c.h>
#include "dvb_i2c.h"
#include "dvb_i2c_bridge.h"

static struct dvb_adapter *dvb_adapter = NULL;
static struct i2c_client dvb_i2c_client;

static int dvb_i2c_bridge_master_xfer(struct dvb_i2c_bus *i2c, const struct i2c_msg msgs[], int num)
{

	return adapter->algo->master_xfer((struct i2c_adapter *)i2c->data, (struct i2c_msg *)msgs, num);

}

static int dvb_i2c_bridge_attach_adapter(struct i2c_adapter *adapter)
{

	struct i2c_client *client;
	
	if ((client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)) == 0)
		return -ENOMEM;
		
	memcpy(client, &dvb_i2c_client, sizeof(struct i2c_client));
	
	client->adapter = adapter;
	
	if (!(client->data = dvb_register_i2c_bus(dvb_i2c_bridge_master_xfer, adapter, dvb_adapter, 0))) {

		kfree(client);

		printk("dvb_i2c_bridge: dvb_register_i2c_bus failed\n");

		return -ENOMEM;

	}

	i2c_attach_client(client);
	
	printk("dvb_i2c_bridge: enabled DVB i2c bridge to %s\n", adapter->name);
	
	return 0;

}

static int dvb_i2c_bridge_detach_client(struct i2c_client *client)
{

	dvb_unregister_i2c_bus(dvb_i2c_bridge_master_xfer, dvb_adapter, 0);

	kfree(client);

	return 0;

}

static void dvb_i2c_bridge_inc_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void dvb_i2c_bridge_dec_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

static struct i2c_driver dvb_i2c_driver = {

	"DVB I2C bridge",
	0xF0,
	I2C_DF_NOTIFY,
	dvb_i2c_bridge_attach_adapter,
	dvb_i2c_bridge_detach_client,
	0,
	dvb_i2c_bridge_inc_use,
	dvb_i2c_bridge_dec_use,
	
};

static struct i2c_client dvb_i2c_client = {

	.name = "dvb",
	.id = 0xF0,
	.flags = 0,
	.addr = 0,
	.adapter = NULL,
	.driver = &dvb_i2c_driver,

};

int dvb_i2c_bridge_register(struct dvb_adapter *adap)
{

	if (dvb_adapter)
		return -EBUSY;

	dvb_adapter = adap;

	i2c_add_driver(&dvb_i2c_driver);

	return 0;
	
}

void dvb_i2c_bridge_unregister(struct dvb_adapter *adap)
{

	if (dvb_adapter != adap)
		return;

	i2c_del_driver(&dvb_i2c_driver);
	
	dvb_adapter = NULL;

}

EXPORT_SYMBOL(dvb_i2c_bridge_register);
EXPORT_SYMBOL(dvb_i2c_bridge_unregister);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DVB I2C bridge");
MODULE_AUTHOR("Florian Schirmer <schirmer@taytron.net>");
