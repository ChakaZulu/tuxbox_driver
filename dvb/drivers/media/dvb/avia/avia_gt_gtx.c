/*
 * $Id: avia_gt_gtx.c,v 1.19 2003/01/11 22:45:16 obi Exp $
 *
 * AViA GTX core driver (dbox-II-project)
 *
 * Homepage: http://dbox2.elxsi.de
 *
 * Copyright (C) 2000-2001 Felix "tmbinc" Domke (tmbinc@gmx.net)
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

#define __KERNEL_SYSCALLS__

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
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>

#include "avia_gt.h"

static sAviaGtInfo *gt_info = (sAviaGtInfo *)NULL;

static int isr[] = {GTX_REG_ISR0, GTX_REG_ISR1, GTX_REG_ISR2, GTX_REG_ISR3};
static int imr[] = {GTX_REG_IMR0, GTX_REG_IMR1, GTX_REG_IMR2, GTX_REG_IMR3};

void avia_gt_gtx_clear_irq(unsigned char irq_reg, unsigned char irq_bit)
{

	gtx_reg_16n(isr[irq_reg]) |= (1 << irq_bit);

}

unsigned short avia_gt_gtx_get_irq_mask(unsigned char irq_reg)
{

	if (irq_reg <= 3)
		return gtx_reg_16n(imr[irq_reg]);
	else
		return 0;

}

unsigned short avia_gt_gtx_get_irq_status(unsigned char irq_reg)
{

	if (irq_reg <= 3)
		return gtx_reg_16n(isr[irq_reg]);
	else
		return 0;

}

void avia_gt_gtx_mask_irq(unsigned char irq_reg, unsigned char irq_bit)
{

	gtx_reg_16n(imr[irq_reg]) &= ~(1 << irq_bit);

}

void avia_gt_gtx_unmask_irq(unsigned char irq_reg, unsigned char irq_bit)
{

	gtx_reg_16n(imr[irq_reg]) |= (1 << irq_bit);

}

static void avia_gt_gtx_intialize_interrupts(void)
{

	gtx_reg_16(IPR0) = -1;
	gtx_reg_16(IPR1) = -1;
	gtx_reg_16(IPR2) = -1;
	gtx_reg_16(IPR3) = -1;

	gtx_reg_16(IMR0) = 0;
	gtx_reg_16(IMR1) = 0;
	gtx_reg_16(IMR2) = 0;
	gtx_reg_16(IMR3) = 0;

	gtx_reg_16(ISR0) = 0;
	gtx_reg_16(ISR1) = 0;
	gtx_reg_16(ISR2) = 0;
	gtx_reg_16(ISR3) = 0;

	gtx_reg_set(RR0, INT, 0);

	gtx_reg_16(IMR0) = 0xFFFF;
	gtx_reg_16(IMR1) = 0xFFFF;

}

static void avia_gt_gtx_close_interrupts(void)
{

	gtx_reg_set(RR0, INT, 1);

	gtx_reg_16(IMR0) = 0;
	gtx_reg_16(IMR1) = 0;
	gtx_reg_16(IMR2) = 0;
	gtx_reg_16(IMR3) = 0;

	gtx_reg_16(IPR0) = -1;
	gtx_reg_16(IPR1) = -1;
	gtx_reg_16(IPR2) = -1;
	gtx_reg_16(IPR3) = -1;

	gtx_reg_16(ISR0) = 0;
	gtx_reg_16(ISR1) = 0;
	gtx_reg_16(ISR2) = 0;
	gtx_reg_16(ISR3) = 0;

}

void avia_gt_gtx_reset(void)
{
	gtx_reg_set(RR0, PIG, 1);
	gtx_reg_set(RR0, VCAP, 1);
	gtx_reg_set(RR0, VID, 1);
	gtx_reg_set(RR0, ACLK, 1);
	gtx_reg_set(RR0, COPY, 1);
	gtx_reg_set(RR0, DRAM, 1);
	gtx_reg_set(RR0, PCM, 1);
	gtx_reg_set(RR0, SPI, 1);
	gtx_reg_set(RR0, IR, 1);
	gtx_reg_set(RR0, BLIT, 1);
	gtx_reg_set(RR0, CRC, 1);
	gtx_reg_set(RR0, INT, 1);
	gtx_reg_set(RR0, SCD, 1);
	gtx_reg_set(RR0, SRX, 1);
	gtx_reg_set(RR0, STX, 1);
	gtx_reg_set(RR0, GV, 1);
	gtx_reg_set(RR1, TTX, 1);
	gtx_reg_set(RR1, DAC, 1);
	gtx_reg_set(RR1, RISC, 1);
	gtx_reg_set(RR1, FRMR, 1);
	gtx_reg_set(RR1, CHAN, 1);
	gtx_reg_set(RR1, AVD, 1);
	gtx_reg_set(RR1, IDC, 1);
	gtx_reg_set(RR1, DESC, 1);
}

void avia_gt_gtx_init(void)
{

	printk("avia_gt_gtx: $Id: avia_gt_gtx.c,v 1.19 2003/01/11 22:45:16 obi Exp $\n");

	gt_info = avia_gt_get_info();

	if (!avia_gt_chip(GTX)) {

		printk("avia_gt_gtx: Unsupported chip type\n");

		return;

	}

	avia_gt_gtx_reset();

	// solle nach avia_gt_gtx_reset() wenn ueberhaupt noetig ...
	udelay (500);

	gtx_reg_set(RR0, VID, 0);
	gtx_reg_set(RR0, DRAM, 0);
	gtx_reg_set(RR0, BLIT, 0);

	// ?
	udelay (500);

	memset (gt_info->mem_addr, 0xFF, GTX_MEM_SIZE);	  // clear ram

	gtx_reg_set(CR0, DOD, 0);	// DAC Output Disable (0: enable)
	gtx_reg_set(CR0, _16M, 1);	// 16 Mbit DRAM Select (1: 16 Mbit)
	gtx_reg_set(CR0, DD1, 0);	// Delay DTACK (2 clocks delay)
	gtx_reg_set(CR0, DD0, 1);
	gtx_reg_set(CR0, RFD, 0);	// Refresh Disable

	avia_gt_gtx_intialize_interrupts ();

	// workaround for framebuffer?
	//atomic_set (&THIS_MODULE->uc.usecount, 1);

}

void avia_gt_gtx_exit(void)
{

	avia_gt_gtx_close_interrupts();

	//gtx_reg_16(CR0) = 0x0030;
	//gtx_reg_16(CR1) = 0x0000;
	gtx_reg_set(CR0, DOD, 1);
	gtx_reg_set(CR0, RFD, 1);

	avia_gt_gtx_reset();

	// take modules in reset state
	//gtx_reg_16(RR0) = 0xFBFF;
	//gtx_reg_16(RR1) = 0x00FF;

	// disable dram module, don't work :-/ why ????
	//gtx_reg_16(RR0) |= (1<<10);

}

