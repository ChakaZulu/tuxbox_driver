/*
 * $Id: dummyadap.c,v 1.4 2002/10/30 18:22:29 Jolt Exp $
 *
 * Dummy Adapter Driver 
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

static struct dvb_adapter *adap;
static struct dvb_demux demux;
static dmxdev_t dmxdev;
static dmx_frontend_t fe_hw;
static dmx_frontend_t fe_mem;
static struct dvb_i2c_bus *i2c_bus;

int i2c_dbox2_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[], int num);

static int dummyadap_i2c_master_xfer(struct dvb_i2c_bus *i2c, struct i2c_msg msgs[], int num)
{

//	printk("dummyadap: i2c_master_xfer\n");
	
	return i2c_dbox2_xfer(NULL, msgs, num);

}

static void dummyadap_before_after_tune(fe_status_t fe_status, void *data)
{

//	printk("dummyadap: before_after_tune\n");

}

static int dummyadap_after_ioctl(struct dvb_frontend *fe, unsigned int cmd, void *arg)
{

//	printk("dummyadap: after_ioctl\n");

	return 0;

}

static int dummyadap_before_ioctl(struct dvb_frontend *fe, unsigned int cmd, void *arg)
{

//	printk("dummyadap: before_ioctl\n");

	// Returning 0 will skip the fe ioctl
	return -ENODEV;

}

static int dummyadap_write_to_decoder(struct dvb_demux_feed *dvbdmxfeed, u8 *buf, size_t count)
{

	printk("dummyadap: write_to_decoder\n");

	return 0;

}

static int dummyadap_start_feed(struct dvb_demux_feed *dvbdmxfeed)
{

	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;

	printk("dummyadap: start_feed\n");
	
	if (!dvbdmx->dmx.frontend)
		return -EINVAL;

	printk("  pid: 0x%04X\n", dvbdmxfeed->pid);
		
	switch(dvbdmxfeed->type) {
	
		case DMX_TYPE_TS:
			printk("  type: DMX_TYPE_TS\n");
			break;
			
		case DMX_TYPE_SEC:
			printk("  type: DMX_TYPE_SEC\n");
			break;
			
		default:
			printk("  type: unknown (%d)\n", dvbdmxfeed->type);
			return -EINVAL;

	}
	
	printk("  ts_type:");

	if (dvbdmxfeed->ts_type & TS_DECODER)
		printk(" TS_DECODER");
	
	if (dvbdmxfeed->ts_type & TS_PACKET)
		printk(" TS_PACKET");

	if (dvbdmxfeed->ts_type & TS_PAYLOAD_ONLY)
		printk(" TS_PAYLOAD_ONLY");
		
	printk("\n");
	
	switch(dvbdmxfeed->pes_type) {
	
		case DMX_TS_PES_VIDEO:
			printk("  pes_type: DMX_TS_PES_VIDEO\n");
			break;
			
		case DMX_TS_PES_AUDIO:
			printk("  pes_type: DMX_TS_PES_AUDIO\n");
			break;
			
		case DMX_TS_PES_TELETEXT:
			printk("  pes_type: DMX_TS_PES_TELETEXT\n");
			break;
			
		case DMX_TS_PES_PCR:
			printk("  pes_type: DMX_TS_PES_PCR\n");
			break;
			
		case DMX_TS_PES_OTHER:
			printk("  pes_type: DMX_TS_PES_OTHER\n");
			break;
			
		default:
			printk("  pes_type: unknown (%d)\n", dvbdmxfeed->pes_type);
			return -EINVAL;

	}
	
	return 0;

}

static int dummyadap_stop_feed(struct dvb_demux_feed *dvbdmxfeed)
{

	printk("dummyadap: stop_feed\n");

	return 0;

}

static int __init dummyadap_init(void)
{

	int result;

	printk("$Id: dummyadap.c,v 1.4 2002/10/30 18:22:29 Jolt Exp $\n");

	if ((result = dvb_register_adapter(&adap, "dummy adapter")) < 0) {
	
		printk("dummyadap: dvb_register_adapter failed (errno = %d)\n", result);
		
		return result;
		
	}

	if (!(i2c_bus = dvb_register_i2c_bus(dummyadap_i2c_master_xfer, NULL, adap, 0))) {

		printk("dummyadap: dvb_register_i2c_bus failed\n");

		dvb_unregister_adapter(adap);
	
		return -ENOMEM;
	
	}
	
	memset(&demux, 0, sizeof(demux));

	demux.dmx.vendor = "Dummy Vendor";
	demux.dmx.model = "Dummy Model";
	demux.dmx.id = "demux0_0";
	demux.dmx.capabilities = DMX_TS_FILTERING | DMX_SECTION_FILTERING;

	demux.priv = NULL;	
	demux.filternum = 31;
	demux.feednum = 31;
	demux.start_feed = dummyadap_start_feed;
	demux.stop_feed = dummyadap_stop_feed;
	demux.write_to_decoder = dummyadap_write_to_decoder;
	
	if ((result = dvb_dmx_init(&demux)) < 0) {
	
		printk("dummyadap: dvb_dmx_init failed (errno = %d)\n", result);

		dvb_unregister_i2c_bus(dummyadap_i2c_master_xfer, adap, 0);
		dvb_unregister_adapter(adap);
		
		return result;
		
	}

//FIXME dmxdev
	dmxdev.filternum = 32;
	dmxdev.demux = &demux.dmx;
	dmxdev.capabilities = 0;
	
	if ((result = dvb_dmxdev_init(&dmxdev, adap)) < 0) {
	
		printk("dummyadap: dvb_dmxdev_init failed (errno = %d)\n", result);

		dvb_dmx_release(&demux);
		dvb_unregister_i2c_bus(dummyadap_i2c_master_xfer, adap, 0);
		dvb_unregister_adapter(adap);
		
		return result;
	
	}

	fe_hw.id = "hw_frontend";
	fe_hw.vendor = "Dummy Vendor";
	fe_hw.model = "hw";
	fe_hw.source = DMX_FRONTEND_0;

	if ((result = demux.dmx.add_frontend(&demux.dmx, &fe_hw)) < 0) {
	
		printk("dummyadap: dvb_dmx_init failed (errno = %d)\n", result);

		dvb_dmxdev_release(&dmxdev);
		dvb_dmx_release(&demux);
		dvb_unregister_i2c_bus(dummyadap_i2c_master_xfer, adap, 0);
		dvb_unregister_adapter(adap);
		
		return result;
		
	}
	
	fe_mem.id = "mem_frontend";
	fe_mem.vendor = "memory";
	fe_mem.model = "sw";
	fe_mem.source = DMX_MEMORY_FE;

	if ((result = demux.dmx.add_frontend(&demux.dmx, &fe_mem)) < 0) {
	
		printk("dummyadap: dvb_dmx_init failed (errno = %d)\n", result);

		demux.dmx.remove_frontend(&demux.dmx, &fe_hw);
		dvb_dmxdev_release(&dmxdev);
		dvb_dmx_release(&demux);
		dvb_unregister_i2c_bus(dummyadap_i2c_master_xfer, adap, 0);
		dvb_unregister_adapter(adap);
		
		return result;
		
	}

	if ((result = demux.dmx.connect_frontend(&demux.dmx, &fe_hw)) < 0) {
	
		printk("dummyadap: dvb_dmx_init failed (errno = %d)\n", result);

		demux.dmx.remove_frontend(&demux.dmx, &fe_mem);
		demux.dmx.remove_frontend(&demux.dmx, &fe_hw);
		dvb_dmxdev_release(&dmxdev);
		dvb_dmx_release(&demux);
		dvb_unregister_i2c_bus(dummyadap_i2c_master_xfer, adap, 0);
		dvb_unregister_adapter(adap);
		
		return result;
		
	}

	if ((result = dvb_add_frontend_notifier(adap, dummyadap_before_after_tune, NULL)) < 0) {
	
		printk("dummyadap: dvb_add_frontend_notifier failed (errno = %d)\n", result);

		demux.dmx.close(&demux.dmx);
		demux.dmx.remove_frontend(&demux.dmx, &fe_mem);
		demux.dmx.remove_frontend(&demux.dmx, &fe_hw);
		dvb_dmxdev_release(&dmxdev);
		dvb_dmx_release(&demux);
		dvb_unregister_i2c_bus(dummyadap_i2c_master_xfer, adap, 0);
		dvb_unregister_adapter(adap);
		
		return result;
		
	}

	if ((result = dvb_add_frontend_ioctls(adap, dummyadap_before_ioctl, dummyadap_after_ioctl, NULL)) < 0) {
	
		printk("dummyadap: dvb_add_frontend_ioctls failed (errno = %d)\n", result);

		dvb_remove_frontend_notifier(adap, dummyadap_before_after_tune);
		demux.dmx.close(&demux.dmx);
		demux.dmx.remove_frontend(&demux.dmx, &fe_mem);
		demux.dmx.remove_frontend(&demux.dmx, &fe_hw);
		dvb_dmxdev_release(&dmxdev);
		dvb_dmx_release(&demux);
		dvb_unregister_i2c_bus(dummyadap_i2c_master_xfer, adap, 0);
		dvb_unregister_adapter(adap);
		
		return result;
		
	}


//FIXME	dvb_net_register();

	return 0;

}

static void __exit dummyadap_exit(void)
{

//FIXME	dvb_net_release(&net);

	dvb_remove_frontend_ioctls(adap, dummyadap_before_ioctl, dummyadap_after_ioctl);
	dvb_remove_frontend_notifier(adap, dummyadap_before_after_tune);
	demux.dmx.close(&demux.dmx);
	demux.dmx.remove_frontend(&demux.dmx, &fe_mem);
	demux.dmx.remove_frontend(&demux.dmx, &fe_hw);
	dvb_dmxdev_release(&dmxdev);
	dvb_dmx_release(&demux);
	dvb_unregister_i2c_bus(dummyadap_i2c_master_xfer, adap, 0);
	dvb_unregister_adapter(adap);

}

#ifdef MODULE
module_init(dummyadap_init);
module_exit(dummyadap_exit);
MODULE_DESCRIPTION("dummy dvb adapter driver");
MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#endif
