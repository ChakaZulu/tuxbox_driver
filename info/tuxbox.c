/*
 *   tuxbox.c - Tuxbox Hardware info
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2002 Florian Schirmer (jolt@tuxbox.org)
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
 *   $Log: tuxbox.c,v $
 *   Revision 1.4  2003/01/01 22:44:41  Jolt
 *   Typo
 *
 *   Revision 1.3  2002/12/31 21:24:55  obi
 *   mid is stored in the bmon area, not in the dallas chip
 *
 *   Revision 1.2  2002/12/30 22:13:14  Jolt
 *   Style changes
 *
 *   Revision 1.1  2002/12/30 17:46:18  Jolt
 *   Tuxbox info module
 *
 *
 *
 *   $Revision: 1.4 $
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
#include <linux/slab.h>
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
#include <dbox/tuxbox.h>

#ifndef CONFIG_PROC_FS
#error Please enable procfs support
#endif

static unsigned int manufacturer;

static int read_manufacturer_id(void)
{
	unsigned char *conf = (unsigned char *) ioremap(0x1001FFE0, 0x20);

	if (!conf) {
		printk("tuxbox: Could not remap memory\n");
		return -1;
	}

	switch (conf[0]) {
	case DBOX_MID_NOKIA:
		manufacturer = TUXBOX_MANUFACTURER_NOKIA;
		break;
	case DBOX_MID_SAGEM:
		manufacturer = TUXBOX_MANUFACTURER_SAGEM;
		break;
	case DBOX_MID_PHILIPS:
		manufacturer = TUXBOX_MANUFACTURER_PHILIPS;
		break;
	default:
		manufacturer = TUXBOX_MANUFACTURER_UNKNOWN;
		break;
	}

	iounmap(conf);

	return 0;

}

static int tuxbox_read_proc(char *buf, char **start, off_t offset, int len, int *eof, void *private)
{
	return sprintf(buf, "TUXBOX_VERSION=%d\nTUXBOX_MANUFACTURER=%d\nTUXBOX_MODEL=%d\n", TUXBOX_VERSION, manufacturer, TUXBOX_MODEL_DBOX2);
}

int __init tuxbox_init(void)
{

	struct proc_dir_entry *tuxbox_proc_entry;

	if (read_manufacturer_id() < 0) {

		printk("tuxbox: Could not read manufacturer id\n");

		return -ENODEV;

	}

	if (!proc_bus) {
	
		printk("tuxbox: /proc/bus does not exist\n");
		
		return -ENOENT;
		
	}

	tuxbox_proc_entry = create_proc_entry("tuxbox", 0, proc_bus);

	if (!tuxbox_proc_entry)	{
	
		printk("tuxbox: Could not create /proc/bus/tuxbox\n");
		
		return -ENOENT;
		
	}

	tuxbox_proc_entry->read_proc = &tuxbox_read_proc;
	tuxbox_proc_entry->write_proc = 0;
	tuxbox_proc_entry->owner = THIS_MODULE;

	return 0;
	
}

void __exit tuxbox_exit(void)
{

	remove_proc_entry("tuxbox", proc_bus);
	
}

module_init(tuxbox_init);
module_exit(tuxbox_exit);

#ifdef MODULE
MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("tuxbox info");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#endif

