/*
 * $Id: avia_gt_core.c,v 1.46 2004/05/19 21:32:20 derget Exp $
 *
 * AViA eNX/GTX core driver (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
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

#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/io.h>

#include <tuxbox/info_dbox2.h>
#include "dvb_functions.h"

#include "avia_gt.h"
#include "avia_gt_accel.h"
#include "avia_gt_dmx.h"
#include "avia_gt_gv.h"
#include "avia_gt_pcm.h"
#include "avia_gt_capture.h"
#include "avia_gt_pig.h"
#include "avia_gt_vbi.h"
#include "avia_gt_ucode.h"

TUXBOX_INFO(dbox2_gt);

static int chip_type = -1;
static int init_state;
static sAviaGtInfo *gt_info;
static wait_queue_head_t avia_gt_wdt_sleep;
static void (* gt_isr_proc_list[128])(unsigned short irq);

sAviaGtInfo *avia_gt_get_info(void)
{
	return gt_info;
}

/* GTX/eNX dependent functions */
static void (* avia_gt_clear_irq)(unsigned char irq_reg, unsigned char irq_bit);
static void (* avia_gt_mask_irq)(unsigned char irq_reg, unsigned char irq_bit);
static void (* avia_gt_unmask_irq)(unsigned char irq_reg, unsigned char irq_bit);
static unsigned short (* avia_gt_get_irq_mask)(unsigned char irq_reg);
static unsigned short (* avia_gt_get_irq_status)(unsigned char irq_reg);

int avia_gt_alloc_irq(unsigned short irq, void (*isr_proc)(unsigned short irq))
{
	dprintk("avia_gt_core: alloc_irq reg %d bit %d\n", AVIA_GT_IRQ_REG(irq), AVIA_GT_IRQ_BIT(irq));

	if (gt_isr_proc_list[AVIA_GT_ISR_PROC_NR(AVIA_GT_IRQ_REG(irq), AVIA_GT_IRQ_BIT(irq))]) {
		printk("avia_gt_core: irq already used\n");
		return -EBUSY;
	}

	gt_isr_proc_list[AVIA_GT_ISR_PROC_NR(AVIA_GT_IRQ_REG(irq), AVIA_GT_IRQ_BIT(irq))] = isr_proc;

	avia_gt_unmask_irq(AVIA_GT_IRQ_REG(irq), AVIA_GT_IRQ_BIT(irq));
	avia_gt_clear_irq(AVIA_GT_IRQ_REG(irq), AVIA_GT_IRQ_BIT(irq));

	return 0;
}

void avia_gt_free_irq(unsigned short irq)
{
	dprintk("avia_gt_core: free_irq reg %d bit %d\n", AVIA_GT_IRQ_REG(irq), AVIA_GT_IRQ_BIT(irq));

	avia_gt_mask_irq(AVIA_GT_IRQ_REG(irq), AVIA_GT_IRQ_BIT(irq));

	gt_isr_proc_list[AVIA_GT_ISR_PROC_NR(AVIA_GT_IRQ_REG(irq), AVIA_GT_IRQ_BIT(irq))] = NULL;
}

static
void avia_gt_irq(int irq, void *dev, struct pt_regs *regs)
{
	u8 irq_reg = 0;
	u8 irq_bit = 0;
	u16 irq_mask = 0;
	u16 irq_status = 0;

	for (irq_reg = 0; irq_reg < 6; irq_reg++) {
		irq_mask = avia_gt_get_irq_mask(irq_reg);
		irq_status = avia_gt_get_irq_status(irq_reg);

		for (irq_bit = 0; irq_bit < 16; irq_bit++) {
			if (irq_status & (1 << irq_bit)) {
				if (gt_isr_proc_list[AVIA_GT_ISR_PROC_NR(irq_reg, irq_bit)]) {
					gt_isr_proc_list[AVIA_GT_ISR_PROC_NR(irq_reg, irq_bit)](AVIA_GT_IRQ(irq_reg, irq_bit));
				}
				else if (irq_mask & (1 << irq_bit)) {
					printk(KERN_NOTICE "avia_gt_core: masking unhandled irq reg %d bit %d\n", irq_reg, irq_bit);
					avia_gt_mask_irq(irq_reg, irq_bit);
				}

				avia_gt_clear_irq(irq_reg, irq_bit);
			}
		}
	}
}

int avia_gt_wdt_thread(void)
{
	dvb_kernel_thread_setup ("avia_gt_wdt");
	dprintk ("avia_av_core: Starting avia_gt_wdt thread.\n");
	for(;;)
	{

		interruptible_sleep_on_timeout(&avia_gt_wdt_sleep, 200);

		if((enx_reg_16(FIFO_PDCT)&0x7F) == 0x00) {
		printk("avia_gt_wdt_thread: FIFO_PDCT = 0 ==> framer crashed .. restarting\n");
        		avia_gt_dmx_risc_reset(1);
		}
		if((enx_reg_16(FIFO_PDCT)&0x7F) == 0x7F) {
		printk("avia_gt_wdt_thread: FIFO_PDCT = 127 ==> risc crashed .. restarting\n");
		avia_gt_dmx_risc_reset(1);
		}			

        
	}
	return 0;
}

int __init avia_gt_init(void)
{
	int result = 0;

	printk(KERN_INFO "avia_gt_core: $Id: avia_gt_core.c,v 1.46 2004/05/19 21:32:20 derget Exp $\n");

	if (chip_type == -1) {
		printk(KERN_INFO "avia_gt_core: autodetecting chip type... ");
		switch (tuxbox_dbox2_gt) {
		case TUXBOX_DBOX2_GT_ENX:
			chip_type = AVIA_GT_CHIP_TYPE_ENX;
			printk("eNX\n");
			break;
		case TUXBOX_DBOX2_GT_GTX:
			chip_type = AVIA_GT_CHIP_TYPE_GTX;
			printk("GTX\n");
			break;
		default:
			printk("none\n");
			break;
		}
	}

	if ((chip_type != AVIA_GT_CHIP_TYPE_ENX) && (chip_type != AVIA_GT_CHIP_TYPE_GTX)) {
		printk(KERN_ERR "avia_gt_core: Unsupported chip type\n");
		return -EIO;
	}

	memset(gt_isr_proc_list, 0, sizeof(gt_isr_proc_list));

	gt_info = kmalloc(sizeof(sAviaGtInfo), GFP_KERNEL);

	if (!gt_info) {
		printk(KERN_ERR "avia_gt_core: Could not allocate info memory!\n");
		avia_gt_exit();
		return -ENOMEM;
	}

	memset(gt_info, 0, sizeof(sAviaGtInfo));

	gt_info->chip_type = chip_type;

	if (avia_gt_chip(ENX)) {
		gt_info->mem_addr_phys = ENX_MEM_BASE;
		gt_info->mem_size = ENX_MEM_SIZE;
		gt_info->reg_addr_phys = ENX_REG_BASE;
		gt_info->reg_size = ENX_REG_SIZE;

		gt_info->irq = ENX_INTERRUPT;
		gt_info->irq_lock = ENX_IRQ_LOCK;
		gt_info->irq_drop = ENX_IRQ_DROP;
		gt_info->irq_pcr = ENX_IRQ_PCR;
		gt_info->irq_it = ENX_IRQ_IT;
		gt_info->irq_ir = ENX_IRQ_IR;
		gt_info->irq_pf = ENX_IRQ_PF;
		gt_info->irq_ad = ENX_IRQ_AD;
		gt_info->irq_vl1 = ENX_IRQ_VL1;
		gt_info->irq_tt = ENX_IRQ_TT;

		gt_info->ir_clk = AVIA_GT_ENX_IR_CLOCK;
		gt_info->aq_rptr = ENX_REG_AQRPL;
		gt_info->tdp_ram = TDP_INSTR_RAM;

		avia_gt_clear_irq = avia_gt_enx_clear_irq;
		avia_gt_mask_irq = avia_gt_enx_mask_irq;
		avia_gt_unmask_irq = avia_gt_enx_unmask_irq;
		avia_gt_get_irq_mask = avia_gt_enx_get_irq_mask;
		avia_gt_get_irq_status = avia_gt_enx_get_irq_status;
	}
	else if (avia_gt_chip(GTX)) {
		gt_info->mem_addr_phys = GTX_MEM_BASE;
		gt_info->mem_size = GTX_MEM_SIZE;
		gt_info->reg_addr_phys = GTX_REG_BASE;
		gt_info->reg_size = GTX_REG_SIZE;

		gt_info->irq = GTX_INTERRUPT;
		gt_info->irq_lock = GTX_IRQ_LOCK;
		gt_info->irq_drop = GTX_IRQ_DROP;
		gt_info->irq_pcr = GTX_IRQ_PCR;
		gt_info->irq_it = GTX_IRQ_IT;
		gt_info->irq_ir = GTX_IRQ_IR;
		gt_info->irq_pf = GTX_IRQ_PF;
		gt_info->irq_ad = GTX_IRQ_AD;
		gt_info->irq_vl1 = GTX_IRQ_VL1;
		gt_info->irq_tt = GTX_IRQ_TT;

		gt_info->ir_clk = AVIA_GT_GTX_IR_CLOCK;
		gt_info->aq_rptr = GTX_REG_AQRPL;
		gt_info->tdp_ram = GTX_REG_RISC;

		avia_gt_clear_irq = avia_gt_gtx_clear_irq;
		avia_gt_mask_irq = avia_gt_gtx_mask_irq;
		avia_gt_unmask_irq = avia_gt_gtx_unmask_irq;
		avia_gt_get_irq_mask = avia_gt_gtx_get_irq_mask;
		avia_gt_get_irq_status = avia_gt_gtx_get_irq_status;
	}

	init_state = 1;

	if (!request_mem_region(gt_info->reg_addr_phys, gt_info->reg_size, "avia_gt_reg")) {
		printk(KERN_ERR "avia_gt_core: Failed to request register space.\n");
		avia_gt_exit();
		return -1;
	}

	init_state = 2;

	if (!(gt_info->reg_addr = (unsigned char *)ioremap(gt_info->reg_addr_phys, gt_info->reg_size))) {
		printk(KERN_ERR "avia_gt_core: Failed to remap register space.\n");
		avia_gt_exit();
		return -1;
	}

	init_state = 3;

	if (!request_mem_region(gt_info->mem_addr_phys, gt_info->mem_size, "avia_gt_mem")) {
		printk(KERN_ERR "avia_gt_core: Failed to request memory space.\n");
		avia_gt_exit();
		return -1;
	}

	init_state = 4;

	if (!(gt_info->mem_addr = (unsigned char *)ioremap(gt_info->mem_addr_phys, gt_info->mem_size))) {
		printk(KERN_ERR "avia_gt_core: Failed to remap memory space.\n");
		avia_gt_exit();
		return -1;
	}

	init_state = 5;

	result = request_irq(gt_info->irq, avia_gt_irq, 0, "avia_gt", 0);

	if (result) {
		printk(KERN_ERR "avia_gt_core: Could not allocate IRQ!\n");
		avia_gt_exit();
		return -1;
	}

	init_state = 6;

	if (avia_gt_chip(ENX))
		avia_gt_enx_init();
	else if (avia_gt_chip(GTX))
		avia_gt_gtx_init();

	init_state = 7;

#if (!defined(MODULE)) || (defined(MODULE) && !defined(STANDALONE))
	if (avia_gt_accel_init()) {
		printk(KERN_ERR "avia_gt_core: avia_gt_accel_init failed\n");
		avia_gt_exit();
		return -1;
	}

	init_state = 8;

#if defined(CONFIG_AVIA_GT_DMX)
	if (avia_gt_dmx_init()) {
		printk(KERN_ERR "avia_gt_core: avia_gt_dmx_init failed\n");
		avia_gt_exit();
		return -1;
	}

	init_state = 9;
#endif

#if defined(CONFIG_AVIA_GT_GV)
	if (avia_gt_gv_init()) {
		printk(KERN_ERR "avia_gt_core: avia_gt_gv_init failed\n");
		avia_gt_exit();
		return -1;
	}

	init_state = 10;
#endif

#if defined(CONFIG_AVIA_GT_PCM)
	if (avia_gt_pcm_init()) {
		printk(KERN_ERR "avia_gt_core: avia_gt_pcm_init failed\n");
		avia_gt_exit();
		return -1;
	}

	init_state = 11;
#endif

#if defined(CONFIG_AVIA_GT_CAPTURE)
	if (avia_gt_capture_init()) {
		printk(KERN_ERR "avia_gt_core: avia_gt_capture_init failed\n");
		avia_gt_exit();
		return -1;
	}

	init_state = 12;

	if (avia_gt_pig_init()) {
		printk(KERN_ERR "avia_gt_core: avia_gt_pig_init failed\n");
		avia_gt_exit();
		return -1;
	}

	init_state = 13;
#endif

#if defined(CONFIG_AVIA_GT_DMX)
	if (avia_gt_vbi_init()) {
		printk(KERN_ERR "avia_gt_core: avia_gt_vbi_init failed\n");
		avia_gt_exit();
		return -1;
	}
	
	init_state = 14;
#endif

#endif /* !STANDALONE */

        /* init avia_av_wdt_sleep queue */
        init_waitqueue_head(&avia_gt_wdt_sleep);

	if (avia_gt_chip(ENX)) {
	        /* start avia_av_wdt_sleep  kernel_thread */
        	kernel_thread ((int (*)(void *)) avia_gt_wdt_thread, NULL, 0);
	}	
	printk(KERN_NOTICE "avia_gt_core: Loaded AViA eNX/GTX driver\n");

	return 0;
}

void avia_gt_exit(void)
{
#if (!defined(MODULE)) || (defined(MODULE) && !defined(STANDALONE))

#if defined(CONFIG_AVIA_GT_DMX)
	if (init_state >= 14)
		avia_gt_vbi_exit();
#endif

#if defined(CONFIG_AVIA_GT_CAPTURE)
	if (init_state >= 13)
		avia_gt_pig_exit();

	if (init_state >= 12)
		avia_gt_capture_exit();
#endif

#if defined(CONFIG_AVIA_GT_PCM)
	if (init_state >= 11)
		avia_gt_pcm_exit();
#endif

#if defined(CONFIG_AVIA_GT_GV)
	if (init_state >= 10)
		avia_gt_gv_exit();
#endif

#if defined(CONFIG_AVIA_GT_DMX)
	if (init_state >= 9)
		avia_gt_dmx_exit();
#endif

	if (init_state >= 8)
		avia_gt_accel_exit();

#endif /* !STANDALONE */

	if (init_state >= 7) {
		if (avia_gt_chip(ENX))
			avia_gt_enx_exit();
		else if (avia_gt_chip(GTX))
			avia_gt_gtx_exit();
	}

	if (init_state >= 6) {
		if (avia_gt_chip(ENX))
			free_irq(ENX_INTERRUPT, 0);
		else if (avia_gt_chip(GTX))
			free_irq(GTX_INTERRUPT, 0);
	}

	if (init_state >= 5)
		iounmap(gt_info->mem_addr);

	if (init_state >= 4)
		release_mem_region(gt_info->mem_addr_phys, gt_info->mem_size);

	if (init_state >= 3)
		iounmap(gt_info->reg_addr);

	if (init_state >= 2)
		release_mem_region(gt_info->reg_addr_phys, gt_info->reg_size);

	if (init_state >= 1)
		kfree(gt_info);
}

module_init(avia_gt_init);
module_exit(avia_gt_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("AViA eNX/GTX driver");
MODULE_LICENSE("GPL");
MODULE_PARM(chip_type, "i");
MODULE_PARM_DESC(chip_type, "-1: autodetect, 0: eNX, 1: GTX");

EXPORT_SYMBOL(avia_gt_alloc_irq);
EXPORT_SYMBOL(avia_gt_free_irq);
EXPORT_SYMBOL(avia_gt_get_info);
