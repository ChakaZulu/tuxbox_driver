/*
 *   tuxbox.c - Dreambox Hardware info
 *
 *   Homepage: http://www.dream-multimedia-tv.de
 *
 *   Copyright (C) 2002, 2003 Florian Schirmer (jolt@tuxbox.org)
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
 */

#include <linux/string.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <linux/fcntl.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/proc_fs.h>

#include "tuxbox.h"

#ifndef CONFIG_PROC_FS
#error PLease enable proc_fs support
#endif

static int read_bus_tuxbox(char *buf, char **start, off_t offset, int len, int *eof , void *private)
{
	int buf_len = 0;
	u32 model = TUXBOX_MODEL_UNKNOWN;
	u32 caps = 0;

	switch(mfspr(PVR)) {
	
		case 0x418108D1:

			model = TUXBOX_MODEL_DREAMBOX_DM7000;
			caps = 0;
			
			break;

		case 0x51510950:
		
			model = TUXBOX_MODEL_DREAMBOX_DM5600;
			caps = 0;
			
			break;

	}
			
	buf_len += sprintf(buf + buf_len, "%s=%d\n", TUXBOX_TAG_VERSION, TUXBOX_VERSION);	
	buf_len += sprintf(buf + buf_len, "%s=%d\n", TUXBOX_TAG_VENDOR, TUXBOX_VENDOR_DREAM_MM);
	buf_len += sprintf(buf + buf_len, "%s=%d\n", TUXBOX_TAG_MODEL, model);	
	buf_len += sprintf(buf + buf_len, "%s=%d\n", TUXBOX_TAG_CAPABILITIES, caps);	

	return buf_len;
}

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("Dreambox Info");
MODULE_LICENSE("GPL");

int init_module(void)
{
	struct proc_dir_entry *proc_bus_tuxbox;

	if (!proc_bus) 
	{
		printk("[info] /proc/bus/ does not exist");
		return -ENOENT;
	}

	proc_bus_tuxbox = create_proc_entry("tuxbox", 0, proc_bus);

	if (!proc_bus_tuxbox)
	{
		printk("[info] Could not create /proc/bus/tuxbox");
		return -ENOENT;
	}

	proc_bus_tuxbox->read_proc = &read_bus_tuxbox;
	proc_bus_tuxbox->write_proc = 0;
	proc_bus_tuxbox->owner = THIS_MODULE;
	
	return 0;
}

void cleanup_module(void)
{
	remove_proc_entry("tuxbox", proc_bus);
	
	return;
}

EXPORT_SYMBOL(cleanup_module);
