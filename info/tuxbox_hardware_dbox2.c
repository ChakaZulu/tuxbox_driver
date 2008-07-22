/*
 * tuxbox_hardware_dbox2.c - TuxBox hardware info - dbox2
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
 * $Id: tuxbox_hardware_dbox2.c,v 1.8 2008/07/22 19:26:18 dbt Exp $
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/config.h>
#ifdef CONFIG_MTD
#include <linux/mtd/mtd.h>
#endif
#include <asm/io.h>

#include "tuxbox_internal.h"
#include <tuxbox/hardware_dbox2.h>
#include <tuxbox/info_dbox2.h>

#include <asm/8xx_immap.h>
#include <asm/commproc.h>
static uint idebase = 0;
/* address-offsets of features in the CPLD 
  (we can't call them registers...) */
#define CPLD_READ_DATA          0x00000000
#define CPLD_READ_FIFO          0x00000A00
#define CPLD_READ_CTRL          0x00000C00

#define CPLD_WRITE_FIFO         0x00000980
#define CPLD_WRITE_FIFO_HIGH    0x00000900
#define CPLD_WRITE_FIFO_LOW     0x00000880
#define CPLD_WRITE_CTRL_TIMING  0x00000860
#define CPLD_WRITE_CTRL         0x00000840
#define CPLD_WRITE_TIMING       0x00000820
#define CPLD_WRITE_DATA         0x00000800

/* bits in the control word */
#define CPLD_CTRL_WRITING 0x20
#define CPLD_CTRL_ENABLE  0x40
#define CPLD_CTRL_REPEAT  0x80

/* helping macros to access the CPLD */
#define CPLD_OUT(offset, value) ( *(volatile uint*)(idebase+(offset)) = (value))
#define CPLD_IN(offset) ( *(volatile uint*)(idebase+(offset)))
#define CPLD_FIFO_LEVEL() (CPLD_IN( CPLD_READ_CTRL)>>28)

extern struct proc_dir_entry *proc_bus_tuxbox;
struct proc_dir_entry *proc_bus_tuxbox_dbox2 = NULL;

tuxbox_dbox2_gt_t tuxbox_dbox2_gt;
tuxbox_dbox2_mid_t tuxbox_dbox2_mid;


/* 
	If this is ever going to be a kernel built-in we 
	could make sure this was called before MTD so no
	need to	create an unnecessary dependency.
 */

#if defined(MODULE) && defined(CONFIG_MTD)
static struct mtd_info *bmon_part_info;
static char *bmonname = "BR bootloader";

static void mtd_add(struct mtd_info *info)
{
	if (!strncmp(bmonname,info->name,strlen(bmonname))) {
		/* we have a match, store this data */
		bmon_part_info = info;
	}
}

static void mtd_remove(struct mtd_info *info)
{ /* unused, but cannot be NULL */
}

static struct mtd_notifier mtd = {
	.add = mtd_add,
	.remove = mtd_remove
};

static int flash_read_mid(void)
{
	/* 
		If we let MTD manage the flash we cannot simply access
		it (because MTD might have put it in a state that is not
		giving any usable output).
		
		So we need to register ourselves as a user and properly ask
		for permission to access the bootloader.
	*/

	int ret = 0;
	size_t retlen;
	char mid;

	register_mtd_user(&mtd);
	
	if (!bmon_part_info) {
		printk(KERN_ERR "tuxbox: BMon partition \"%s\" not found\n",bmonname);
		ret = -EIO;
		goto frm_exit;
	}

	if (bmon_part_info->size!=0x20000) {
		printk(KERN_ERR "tuxbox: BMon partition has unexpected size %d\n",bmon_part_info->size);
		ret = -EIO;
		goto frm_exit;
	}

	ret = bmon_part_info->read(bmon_part_info, 0x1FFE0, 1, &retlen, &mid);
	if (ret) {
		printk(KERN_ERR "tuxbox: BMon partition read error\n");
		goto frm_exit;
	}
	
	if (!retlen) {
		printk(KERN_ERR "tuxbox: BMon partition read returned 0 bytes\n");
		ret = -EIO;
		goto frm_exit;
	}

	tuxbox_dbox2_mid = mid;

frm_exit:
	unregister_mtd_user(&mtd);
	return ret;
}

#else /* CONFIG_MTD */

static int flash_read_mid(void)
{
	unsigned char *conf = (unsigned char *) ioremap (0x1001FFE0, 0x20);
	if (!conf) {
		printk(KERN_ERR "tuxbox: Could not remap memory\n");
		return -EIO;
	}

	tuxbox_dbox2_mid = conf[0];
	iounmap (conf);
	
	return 0;
}

#endif

static int vendor_read (void)
{
	if (flash_read_mid()<0)
		return -EIO;

	switch (tuxbox_dbox2_mid) {
		case TUXBOX_DBOX2_MID_NOKIA:
			tuxbox_vendor = TUXBOX_VENDOR_NOKIA;
			tuxbox_dbox2_gt = TUXBOX_DBOX2_GT_GTX;
			break;

		case TUXBOX_DBOX2_MID_PHILIPS:
			tuxbox_vendor = TUXBOX_VENDOR_PHILIPS;
			tuxbox_dbox2_gt = TUXBOX_DBOX2_GT_ENX;
			break;

		case TUXBOX_DBOX2_MID_SAGEM:
			tuxbox_vendor = TUXBOX_VENDOR_SAGEM;
			tuxbox_dbox2_gt = TUXBOX_DBOX2_GT_ENX;
			break;
	}

	return 0;
}

/*read out id-code, this value can only get on first read!*/
static unsigned int read_if_idcode(void) 
{
	static unsigned int idcode;
	static int alreadyread = 0;
	if (!alreadyread) {
		idcode = CPLD_IN(CPLD_READ_FIFO);
		alreadyread = 1;
	}
	return idcode;
}


static int write_if_idcodeback(unsigned int idcode) 
{
	CPLD_OUT(CPLD_WRITE_FIFO, idcode);
}

/* detect_cpld: Check that the CPLD really works */
static int detect_cpld(void)
{
	int i;
	uint check, back;
	uint patterns[2] = { 0xCAFEFEED, 0xBEEFC0DE };


	unsigned int idcode = read_if_idcode();

	/* This detection code not only checks that there is a CPLD,
	   but also that it does work more or less as expected.  */

	/* first perform a walking bit test via data register:
	   this checks that there is a data register and
	   that the data bus is correctly connected */

	for (i = 0; i < 31; i++) {
		/* only one bit is 1 */
		check = 1 << i;
		CPLD_OUT(CPLD_WRITE_DATA, check);
		back = CPLD_IN(CPLD_READ_DATA);
		if (check != back) {
			return 0;
		}

		/* only one bit is 0 */
		check = ~check;
		CPLD_OUT(CPLD_WRITE_DATA, check);
		back = CPLD_IN(CPLD_READ_DATA);
		if (check != back) {
			return 0;
		}
	}

	/* Now test the fifo:
	   If there is still data inside, read it out to clear it */
	for (i = 3; (i > 0) && ((back & 0xF0000000) != 0); i--) {
		CPLD_IN(CPLD_READ_FIFO);
		back = CPLD_IN(CPLD_READ_CTRL);
	}

	if (i == 0) {
		return 0;
	}

	/* then write two long words to the fifo */
	CPLD_OUT(CPLD_WRITE_FIFO, patterns[0]);
	CPLD_OUT(CPLD_WRITE_FIFO, patterns[1]);

	/* and read them back */
	back = CPLD_IN(CPLD_READ_FIFO);
	if (back != patterns[0]) {
		return 0;
	}
	back = CPLD_IN(CPLD_READ_FIFO);
	if (back != patterns[1]) {
		return 0;
	}

	/* now the fifo must be empty again */
	back = CPLD_IN(CPLD_READ_CTRL);
	if ((back & 0xF0000000) != 0) {
		return 0;
	}

	/* Clean up: clear bits in fifo */
	check = 0;
	CPLD_OUT(CPLD_WRITE_FIFO, check);
	back = CPLD_IN(CPLD_READ_FIFO);
	if (back != check) {
		return 0;
	}

	/* CPLD is valid!
	   Hopefully the IDE part will also work:
	   A test for that part is not implemented, but the kernel
	   will probe for drives etc, so this will check a lot
	 */

	write_if_idcodeback(idcode);

	return 1;
}

static int ide_if_present(void)
{
	int ret = 0;
	unsigned long mem_addr = 0x02000000;
	unsigned long mem_size = 0x00001000;
	immap_t *immap = (immap_t *) IMAP_ADDR;
	memctl8xx_t *memctl = &immap->im_memctl;
	uint br2 = memctl->memc_br2;

	if (br2 & 0x1) {
//		printk(KERN_ERR "tuxbox: IDE memory-bank already in use - assuming active IDE if\n");
		return 0;
	}

	mem_addr = (br2 & 0xFFFF8000);

	/* ioremap also activates the guard bit in the mmu,
	   so the MPC8xx core does not do speculative reads
	   to these addresses
	 */

	/* the CPLD is connected to CS2, which should be inactive.
	   if not there might be something using that hardware and
	   we don't want to disturb that */
	br2 |= 0x1;
	memctl->memc_br2 = br2;

	/* map_memory: we know the physical address of our chip. 
	   But the kernel has to give us a virtual address. */
	idebase = (uint) ioremap(mem_addr, mem_size);

	if ((idebase) && detect_cpld()) {
		ret = 1;
	}

	if (idebase) {
		/* unmap_memory: we will not use these virtual addresses anymore */
		iounmap((uint *) idebase);
		idebase = 0;
	}

	/* Deactivate CS2 when driver is not loaded */
	br2 &= ~1;
	memctl->memc_br2 = br2;
	return ret;
}

int tuxbox_hardware_read (void)
{
	int ret;

	tuxbox_model = TUXBOX_MODEL_DBOX2;
	tuxbox_submodel = TUXBOX_SUBMODEL_DBOX2;

	if ((ret = vendor_read ()))
		return ret;

	tuxbox_capabilities = TUXBOX_HARDWARE_DBOX2_CAPABILITIES;

	if (ide_if_present()){
		unsigned int idcode = read_if_idcode();
		char vendor[30];
		if ((idcode==0) || (idcode==0x50505050))
			strcpy(vendor, "Gurgel\0");
		else if (idcode == 0x556c6954)
			strcpy(vendor, "DboxBaer or kpt.ahab/Stingray\0");
		else
			strcpy(vendor, "Unknown\0");
		printk("tuxbox: IDE-Interface detected, Vendor: %s\n", vendor);

		tuxbox_capabilities |= TUXBOX_CAPABILITIES_HDD;
	} else {
		printk("tuxbox: no IDE-Interface detected\n");
	}
	return 0;
}

int tuxbox_hardware_proc_create (void)
{
	if (!(proc_bus_tuxbox_dbox2 = proc_mkdir ("dbox2", proc_bus_tuxbox)))
		goto error;

	if (tuxbox_proc_create_entry ("gt", 0444, proc_bus_tuxbox_dbox2, &tuxbox_dbox2_gt, &tuxbox_proc_read, NULL))
		goto error;

	if (tuxbox_proc_create_entry ("mid", 0444, proc_bus_tuxbox_dbox2, &tuxbox_dbox2_mid, &tuxbox_proc_read, NULL))
		goto error;

	return 0;

error:
	printk("tuxbox: Could not create /proc/bus/tuxbox/dbox2\n");
	return -ENOENT;
}

void tuxbox_hardware_proc_destroy (void)
{
	remove_proc_entry ("gt", proc_bus_tuxbox_dbox2);
	remove_proc_entry ("mid", proc_bus_tuxbox_dbox2);

	remove_proc_entry ("dbox2", proc_bus_tuxbox);
}

