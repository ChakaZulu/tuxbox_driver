/*
 * $Id: avia_av_proc.c,v 1.11 2003/09/30 05:45:35 obi Exp $
 *
 * AViA 500/600 proc driver (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2002 Florian Schirmer <jolt@tuxbox.org>
 * Copyright (C) 2003 Andreas Oberritter <obi@tuxbox.org>
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
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>

#include "avia_av.h"
#include "avia_av_proc.h"

static
int avia_av_proc_read_bitstream_settings(char *buf, char **start, off_t offset, int len, int *eof, void *private)
{
	int nr = 0;

	nr = sprintf(buf, "Bitstream Settings:\n");
	
	nr += sprintf(buf + nr, "H_SIZE:  %d\n", avia_av_dram_read(H_SIZE) & 0xFFFF);
	nr += sprintf(buf + nr, "V_SIZE:  %d\n", avia_av_dram_read(V_SIZE) & 0xFFFF);
	nr += sprintf(buf + nr, "A_RATIO: %d\n", avia_av_dram_read(ASPECT_RATIO) & 0xFFFF);
	nr += sprintf(buf + nr, "F_RATE:  %d\n", avia_av_dram_read(FRAME_RATE) & 0xFFFF);
	nr += sprintf(buf + nr, "B_RATE:  %d\n", avia_av_dram_read(BIT_RATE) & 0xFFFF);
	nr += sprintf(buf + nr, "VB_SIZE: %d\n", avia_av_dram_read(VBV_SIZE) & 0xFFFF);
	nr += sprintf(buf + nr, "A_TYPE:  %d\n", avia_av_dram_read(AUDIO_TYPE) & 0xFFFF);

	return nr;
}

static u32 *dram_copy;

static
int avia_av_proc_read_dram(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int n;

	/* copy dram to buffer on first read */
	if (off == 0) {
		if (!dram_copy)
			dram_copy = vmalloc(0x200000);
		if (!dram_copy)
			return -ENOMEM;
		for (n = 0; n < 512 * 1024; n++)
			dram_copy[n] = avia_av_dram_read(n << 2);
	}
	else if (!dram_copy) {
		return -EINVAL;
	}

	n = 0x200000;

	if (off >= n) {
		*eof = 1;
		vfree(dram_copy);
		dram_copy = NULL;
		return 0;
	}

	if (n > count)
		n = count;
	else
		*eof = 1;

	memcpy(page, &((u8*)dram_copy)[off], n);
	*start = page;

	if (*eof) {
		vfree(dram_copy);
		dram_copy = NULL;
	}

	return n;
}

int avia_av_proc_init(void)
{
	struct proc_dir_entry *proc_bus_avia;
	struct proc_dir_entry *proc_bus_avia_dram;

	printk("avia_av_proc: $Id: avia_av_proc.c,v 1.11 2003/09/30 05:45:35 obi Exp $\n");

	if (!proc_bus) {
		printk("avia_av_proc: /proc/bus does not exist");
		return -ENOENT;
	}

	proc_bus_avia = create_proc_read_entry("bitstream", 0, proc_bus, &avia_av_proc_read_bitstream_settings, NULL);

	if (!proc_bus_avia) {
		printk("avia_av_proc: could not create /proc/bus/bitstream");
		return -ENOENT;
	}

	proc_bus_avia->owner = THIS_MODULE;

	proc_bus_avia_dram = create_proc_read_entry("avia_dram", 0, proc_bus, &avia_av_proc_read_dram, NULL);

	if (!proc_bus_avia_dram) {
		printk("avia_av_proc: could not create /proc/bus/avia_dram");
		return -ENOENT;
	}

	proc_bus_avia_dram->owner = THIS_MODULE;

	return 0;
}

void avia_av_proc_exit(void)
{
	remove_proc_entry("avia_dram", proc_bus);
	remove_proc_entry("bitstream", proc_bus);
}

#if defined(STANDALONE)
module_init(avia_av_proc_init);
module_exit(avia_av_proc_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>, Andreas Oberritter <obi@tuxbox.org>");
MODULE_DESCRIPTION("AViA 500/600 proc interface");
MODULE_LICENSE("GPL");
#endif
