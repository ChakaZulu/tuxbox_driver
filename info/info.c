/*
 *   info.c - d-Box Hardware info
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2001 Felix "tmbinc" Domke (tmbinc@gmx.net)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *   $Log: info.c,v $
 *   Revision 1.7  2001/07/07 23:05:34  fnbrd
 *   Probe auf AT/VES1993 und kleine Fehler bei Philips behoben
 *
 *   Revision 1.6  2001/06/09 23:51:44  tmbinc
 *   added fe.
 *
 *   Revision 1.5  2001/06/03 20:41:55  kwon
 *   indent
 *
 *   Revision 1.4  2001/04/23 00:24:45  fnbrd
 *   /proc/bus/dbox.sh an die sh der BusyBox angepasst.
 *
 *   Revision 1.3  2001/04/04 17:43:58  fnbrd
 *   /proc/bus/dbox.sh implementiert.
 *
 *   Revision 1.2  2001/04/03 17:48:24  tmbinc
 *   improved /proc/bus/info (philips support)
 *
 *   Revision 1.1  2001/03/28 23:33:31  tmbinc
 *   added /proc/bus/info.
 *
 *
 *   $Revision: 1.7 $
 *
 */

#include <linux/string.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <linux/fcntl.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/malloc.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/i2c.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <dbox/info.h>

	/* ich weiss dass dieses programm suckt, aber was soll man machen ... */

int fe=0;
static struct dbox_info_struct info;

#ifdef CONFIG_PROC_FS

static int info_proc_init(void);
static int info_proc_cleanup(void);

static int read_bus_info(char *buf, char **start, off_t offset, int len,
												int *eof , void *private);

#else /* undef CONFIG_PROC_FS */

#define info_proc_init() 0
#define info_proc_cleanup() 0

#endif /* CONFIG_PROC_FS */

static int attach_dummy_adapter(struct i2c_adapter *);

static struct i2c_driver dummy_i2c_driver = {
        "FOR_PROBE_ONLY",
        I2C_DRIVERID_EXP2, // experimental use id
	I2C_DF_NOTIFY,
        &attach_dummy_adapter,
        0,
        0,
	0,
	0
};

static struct i2c_client dummy_i2c_client = {
        "FOR_PROBE_ONLY_CLIENT",
        I2C_DRIVERID_EXP2, // experimental use id
        0,
        0,
        NULL,
        &dummy_i2c_driver,
        NULL
};

static u8 i2c_addr_of_device;
static u8 i2c_device_addr_to_read;
static u8 i2c_should_value;
static int i2c_found;

// k.A. ob das noch wer anders so (wegen Protokoll) gebrauchen kann,
// deswegen lass ich das mal so hier drin
// Routine sieht etwas merkwuerdig aus, habe ich aber groesstenteils so
// aus ves1820 uebernommen und keine Lust das zu cleanen
static u8 readreg(struct i2c_client *client, u8 reg)
{
        struct i2c_adapter *adap=client->adapter;
        unsigned char mm1[1];
        unsigned char mm2[] = {0x00};
        struct i2c_msg msgs[2];

        msgs[0].flags=0;
        msgs[1].flags=I2C_M_RD;
        msgs[0].addr=msgs[1].addr=client->addr;
        mm1[0]=reg;
        msgs[0].len=1; msgs[1].len=1;
        msgs[0].buf=mm1; msgs[1].buf=mm2;
        i2c_transfer(adap, msgs, 2);

        return mm2[0];
}

static int attach_dummy_adapter(struct i2c_adapter *adap)
{
  dummy_i2c_client.adapter=adap;
  dummy_i2c_client.addr=i2c_addr_of_device;
  if (readreg(&dummy_i2c_client, i2c_device_addr_to_read)!=i2c_should_value) {
//    printk("device not found\n");
    i2c_found=0;
  }
  else {
//    printk("device found\n");
    i2c_found=1;
  }
  return -1; // we don't need to attach, probing was done
}

static void probeDevice(void)
{
  i2c_add_driver(&dummy_i2c_driver); // fails allways
  i2c_del_driver(&dummy_i2c_driver);
}

static int checkForAT76C651(void)
{
  i2c_addr_of_device=0x0d; // =0x1a >> 1
  i2c_device_addr_to_read=0x0e;
  i2c_should_value=0x65;
  probeDevice();
  return i2c_found;
}

static int dbox_info_init(void)
{
	unsigned char *conf=(unsigned char*)ioremap(0x1001FFE0, 0x20);
	if (!conf)
	{
		printk(KERN_ERR "couldn't remap memory for getting device info.\n");
		return -1;
	}
	memset(info.dsID, 0, 8);		// todo
	info.mID=conf[0];
	info.fe=fe;
	if (info.mID==DBOX_MID_SAGEM)								// das suckt hier, aber ich kenn keinen besseren weg.
	{
		info.feID=0;
		info.fpID=0x52;
		info.enxID=3;
		info.gtxID=-1;
		info.hwREV=fe?0x21:0x41;
		info.fpREV=0x23;
		info.demod= checkForAT76C651() ? DBOX_DEMOD_AT76C651 : DBOX_DEMOD_VES1993 ;
//		info.demod=fe?DBOX_DEMOD_VES1993:DBOX_DEMOD_AT76C651;
	}	else if (info.mID==DBOX_MID_PHILIPS)		// never seen a cable-philips
	{
		info.feID=0;
		info.fpID=0x52;
		info.enxID=3;
		info.gtxID=-1;
		info.hwREV=fe?0x01:-1;
		info.fpREV=0x30;
		info.demod=fe?DBOX_DEMOD_TDA8044H:-1;
	}	else if (info.mID==DBOX_MID_NOKIA)
	{
		info.feID=fe?0xdd:0x7a;
		info.fpID=0x5a;
		info.enxID=-1;
		info.gtxID=0xB;
		info.fpREV=0x81;
		info.hwREV=0x5;
		info.demod=fe?DBOX_DEMOD_VES1893:DBOX_DEMOD_VES1820;
	}
	iounmap(conf);
	printk(KERN_DEBUG "mID: %02x feID: %02x fpID: %02x enxID: %02x gtxID: %02x hwREV: %02x fpREV: %02x\n",
		info.mID, info.feID, info.fpID, info.enxID, info.gtxID, info.hwREV, info.fpREV);
	info_proc_init();
	return 0;
}

int dbox_get_info(struct dbox_info_struct *dinfo)
{
	memcpy(dinfo, &info, sizeof(info));
	return 0;
}

int dbox_get_info_ptr(struct dbox_info_struct **dinfo)
{
	if (!dinfo)
		return -EFAULT;
	*dinfo=&info;
	return 0;
}

#ifdef CONFIG_PROC_FS

static char *demod_table[5]={"VES1820", "VES1893", "AT76C651", "VES1993", "TDA8044H"};

static int read_bus_info(char *buf, char **start, off_t offset, int len,
												int *eof , void *private)
{
	return sprintf(buf, "mID=%02x\nfeID=%02x\nfpID=%02x\nenxID=%02x\ngtxID=%02x\nhwREV=%02x\nfpREV=%02x\nDEMOD=%s\nfe=%d\n",
		info.mID, info.feID, info.fpID, info.enxID, info.gtxID, info.hwREV, info.fpREV, info.demod==-1 ? "UNKNOWN" : demod_table[info.demod], info.fe);
}

static int read_bus_info_sh(char *buf, char **start, off_t offset, int len,
												int *eof , void *private)
{
	return sprintf(buf, "#!/bin/sh\nexport mID=%02x\nexport feID=%02x\nexport fpID=%02x\nexport enxID=%02x\nexport gtxID=%02x\nexport hwREV=%02x\nexport fpREV=%02x\nexport DEMOD=%s\nexport fe=%d\n",
//	return sprintf(buf, "#!/bin/sh\nexport mID=%02x feID=%02x fpID=%02x enxID=%02x gtxID=%02x hwREV=%02x fpREV=%02x DEMOD=%s\n\n",
//	return sprintf(buf, "#!/bin/sh\nmID=%02x\nfeID=%02x\nfpID=%02x\nenxID=%02x\ngtxID=%02x\nhwREV=%02x\nfpREV=%02x\nDEMOD=%s\nexport mID feID fpID enxID gtxID hwREV fpREV DEMOD\n\n",
		info.mID, info.feID, info.fpID, info.enxID, info.gtxID, info.hwREV, info.fpREV, info.demod==-1 ? "UNKNOWN" : demod_table[info.demod], info.fe);
}

int info_proc_init(void)
{
	struct proc_dir_entry *proc_bus_info;
	struct proc_dir_entry *proc_bus_info_sh;

	if (! proc_bus) {
		printk("info.o: /proc/bus/ does not exist");
 		return -ENOENT;
        }

	proc_bus_info = create_proc_entry("dbox", 0, proc_bus);

	if (!proc_bus_info)
	{
		printk("info.o: Could not create /proc/bus/dbox");
		return -ENOENT;
	}

	proc_bus_info_sh = create_proc_entry("dbox.sh", 0, proc_bus);
	if (!proc_bus_info_sh)
	{
		printk("info.o: Could not create /proc/bus/dbox.sh");
		return -ENOENT;
	}


	proc_bus_info->read_proc = &read_bus_info;
	proc_bus_info->write_proc = 0;
	proc_bus_info->owner = THIS_MODULE;
	proc_bus_info_sh->read_proc = &read_bus_info_sh;
	proc_bus_info_sh->write_proc = 0;
	proc_bus_info_sh->mode|=S_IXUGO;
	proc_bus_info_sh->owner = THIS_MODULE;
	return 0;
}

int info_proc_cleanup(void)
{
	remove_proc_entry("dbox", proc_bus);
	remove_proc_entry("dbox.sh", proc_bus);
	return 0;
}
#endif /* def CONFIG_PROC_FS */

#ifdef MODULE
MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("d-Box info");
MODULE_PARM(fe, "i");
EXPORT_SYMBOL(dbox_get_info);
EXPORT_SYMBOL(dbox_get_info_ptr);

int init_module(void)
{
	return dbox_info_init();
}

void cleanup_module(void)
{
	info_proc_cleanup();
	return;
}

EXPORT_SYMBOL(cleanup_module);

#endif
