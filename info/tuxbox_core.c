/*
 * tuxbox_core.c - TuxBox hardware info
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
 * $Id: tuxbox_core.c,v 1.2 2003/02/20 21:45:17 waldi Exp $
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/unistd.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <tuxbox/tuxbox_info.h>
#include <tuxbox/tuxbox_hardware.h>

#ifndef CONFIG_PROC_FS
#error Please enable procfs support
#endif

struct proc_dir_entry *proc_bus_tuxbox = NULL;

tuxbox_capabilities_t tuxbox_capabilities;
tuxbox_model_t tuxbox_model;
tuxbox_submodel_t tuxbox_submodel;
tuxbox_vendor_t tuxbox_vendor;

static int tuxbox_proc_create (void);
static void tuxbox_proc_destroy (void);

static int tuxbox_proc_read (char *buf, char **start, off_t offset, int len, int *eof, void *data)
{
	int *_data = data;
	return snprintf(buf, len, "%d\n", *_data);
}

static int tuxbox_proc_create (void)
{
	struct proc_dir_entry *entry;

	if (!proc_bus) {
		printk("tuxbox: /proc/bus does not exist\n");
		return -ENOENT;
	}

	proc_bus_tuxbox = proc_mkdir ("tuxbox", proc_bus);
	if (!proc_bus_tuxbox) goto error;

	entry = create_proc_entry ("capabilities", 0444, proc_bus_tuxbox);
	if (!entry) goto error;
	entry->data = &tuxbox_capabilities;
	entry->read_proc = &tuxbox_proc_read;
	entry->write_proc = NULL;
	entry->owner = THIS_MODULE;

	entry = create_proc_entry ("model", 0444, proc_bus_tuxbox);
	if (!entry) goto error;
	entry->data = &tuxbox_model;
	entry->read_proc = &tuxbox_proc_read;
	entry->write_proc = NULL;
	entry->owner = THIS_MODULE;

	entry = create_proc_entry ("submodel", 0444, proc_bus_tuxbox);
	if (!entry) goto error;
	entry->data = &tuxbox_submodel;
	entry->read_proc = &tuxbox_proc_read;
	entry->write_proc = NULL;
	entry->owner = THIS_MODULE;

	entry = create_proc_entry ("vendor", 0444, proc_bus_tuxbox);
	if (!entry) goto error;
	entry->data = &tuxbox_vendor;
	entry->read_proc = &tuxbox_proc_read;
	entry->write_proc = NULL;
	entry->owner = THIS_MODULE;

	return 0;

error:
	printk("tuxbox: Could not create /proc/bus/tuxbox\n");
	tuxbox_proc_destroy ();
	return -ENOENT;
}

static void tuxbox_proc_destroy (void)
{
	remove_proc_entry ("capabilities", proc_bus_tuxbox);
	remove_proc_entry ("model", proc_bus_tuxbox);
	remove_proc_entry ("submodel", proc_bus_tuxbox);
	remove_proc_entry ("vendor", proc_bus_tuxbox);

	remove_proc_entry ("tuxbox", proc_bus);
}

int __init tuxbox_init(void)
{
	int ret;

	if (tuxbox_hardware_read ()) {
		printk("tuxbox: Could not read hardware info\n");
		return -ENODEV;
	}

	if ((ret = tuxbox_proc_create ()))
		return ret;

	return 0;
}

void __exit tuxbox_exit(void)
{
	tuxbox_proc_destroy ();
}

module_init(tuxbox_init);
module_exit(tuxbox_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>, Bastian Blank <waldi@tuxbox.org>");
MODULE_DESCRIPTION("TuxBox info");
MODULE_LICENSE("GPL");

