/*
 * $Id: avia_gt_enx.c,v 1.16 2003/01/14 22:37:58 jolt Exp $
 *
 * AViA eNX core driver (dbox-II-project)
 *
 * Homepage: http://dbox2.elxsi.de
 *
 * Copyright (C) 2000-2002 Florian Schirmer (jolt@tuxbox.org)
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

static int isr[] = {0x0100, 0x0102, 0x0104, 0x0106, 0x0108, 0x010A};
static int imr[] = {0x0110, 0x0112, 0x0114, 0x0116, 0x0118, 0x011A};

void avia_gt_enx_clear_irq(unsigned char irq_reg, unsigned char irq_bit)
{

    enx_reg_write16n(isr[irq_reg], 1 << irq_bit);
	
}

unsigned short avia_gt_enx_get_irq_mask(unsigned char irq_reg)
{

	if (irq_reg <= 5)
		return enx_reg_read16n(imr[irq_reg]);
	else
		return 0;
	
}

unsigned short avia_gt_enx_get_irq_status(unsigned char irq_reg)
{

	if (irq_reg <= 5)
		return enx_reg_read16n(isr[irq_reg]);
	else
		return 0;
	
}

void avia_gt_enx_mask_irq(unsigned char irq_reg, unsigned char irq_bit)
{

	enx_reg_write16n(imr[irq_reg], 1 << irq_bit);
	
}

void avia_gt_enx_unmask_irq(unsigned char irq_reg, unsigned char irq_bit)
{

	enx_reg_write16n(imr[irq_reg], (1 << irq_bit) | 1);
	
}

void enx_dac_init(void)
{

    enx_reg_set(RSTR0, DAC, 0);	// Get dac out of reset state
    enx_reg_write16(DAC_PC, 0x0000);
    enx_reg_write16(DAC_CP, 0x0009);
	
}

void enx_video_init(void)
{

    enx_reg_set(RSTR0, VDEO, 0);		// Get video out of reset state
    enx_reg_write16(VHT, 857 | 0x5000);
    enx_reg_write16(VLT, (623 | (21 << 11)));

}

void enx_irq_enable(void)
{

  enx_reg_write32(EHIDR, 0);	 // IRQs an Hostprozessor weiterreichen
  enx_reg_write32(IPR4, 0x55555555); // alles auf HIRQ0
  enx_reg_write32(IPR5, 0x55555555); // das auch noch

  enx_reg_write16(ISR0, 0xFFFE);		// Clear all irq states
  enx_reg_write16(ISR1, 0xFFFE);		// Clear all irq states
  enx_reg_write16(ISR2, 0xFFFE);		// Clear all irq states
  enx_reg_write16(ISR3, 0xFFFE);		// Clear all irq states
  enx_reg_write16(ISR4, 0xFFFE);		// Clear all irq states
  enx_reg_write16(ISR5, 0xFFFE);		// Clear all irq states

  enx_reg_write16(IMR0, 0x0001);		// mask all IRQ's (=disable them)
  enx_reg_write16(IMR1, 0x0001);
  enx_reg_write16(IMR2, 0x0001);
  enx_reg_write16(IMR3, 0x0001);
  enx_reg_write16(IMR4, 0x0001);
  enx_reg_write16(IMR5, 0x0001);
  enx_reg_write32(IDR, 0);

}

void enx_irq_disable(void) {

  enx_reg_write16(IMR0, 0xFFFE);		// Mask all IRQ's
  enx_reg_write16(IMR1, 0xFFFE);		// Mask all IRQ's
  enx_reg_write16(IMR2, 0xFFFE);		// Mask all IRQ's
  enx_reg_write16(IMR3, 0xFFFE);		// Mask all IRQ's
  enx_reg_write16(IMR4, 0xFFFE);		// Mask all IRQ's
  enx_reg_write16(IMR5, 0xFFFE);		// Mask all IRQ's

  enx_reg_write16(IMR0, 0x0001);		// Mask all IRQ's
  enx_reg_write16(IMR1, 0x0001);		// Mask all IRQ's
  enx_reg_write16(IMR2, 0x0001);		// Mask all IRQ's
  enx_reg_write16(IMR3, 0x0001);		// Mask all IRQ's
  enx_reg_write16(IMR4, 0x0001);		// Mask all IRQ's
  enx_reg_write16(IMR5, 0x0001);		// Mask all IRQ's

}

void enx_reset(void) {

  enx_reg_write32(RSTR0, 0xFCF6BEFF);	// Reset all modules
  
}

void enx_sdram_ctrl_init(void) {

  enx_reg_write32(SCSC, 0x00000000);		// Set sd-ram start address
  enx_reg_set(RSTR0, SDCT, 0);		// Get sd-ram controller out of reset state
  enx_reg_write32(MC, 0x00001011);		// Write memory configuration
//  enx_reg_32n(0x88) |= 0x3E << 4;  <- Mhhhh????
  
}

void avia_gt_enx_init(void)
{

	printk("avia_gt_enx: $Id: avia_gt_enx.c,v 1.16 2003/01/14 22:37:58 jolt Exp $\n");
    
	gt_info = avia_gt_get_info();
    
	if (!avia_gt_chip(ENX)) {
    
		printk("avia_gt_enx: Unsupported chip type\n");
	
		return;
    
	}

	enx_reset();
	enx_sdram_ctrl_init();
	enx_dac_init();
	enx_video_init();
	enx_irq_enable();
  
	memset(gt_info->mem_addr, 0xF, 1024 * 1024 /*ENX_MEM_SIZE*/);

    //bring out of reset state
	enx_reg_set(RSTR0, AVI, 0);  // AV - Decoder
	enx_reg_set(RSTR0, QUE, 0);  // Queue Manager
	enx_reg_set(RSTR0, BLIT, 0);   // Blitter / Color expander

	enx_reg_set(CFGR0, TCP, 0);   // disable clip mode teletext
	enx_reg_set(CFGR0, ACP, 0);   // disable clip mode audio
	enx_reg_set(CFGR0, VCP, 0);   // disable clip mode video
    
}

void avia_gt_enx_exit(void)
{

    enx_irq_disable();
    enx_reset();
    
}

