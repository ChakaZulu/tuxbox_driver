/*
 *   avia_gt_core.c - AViA eNX/GTX core driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2002 Florian Schirmer (jolt@tuxbox.org)
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
 *   $Log: avia_gt_core.c,v $
 *   Revision 1.7  2002/04/15 10:40:50  Jolt
 *   eNX/GTX
 *
 *   Revision 1.6  2002/04/14 18:06:19  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.5  2002/04/13 23:19:05  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.4  2002/04/13 14:47:19  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.3  2002/04/12 21:29:35  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.2  2002/04/12 18:59:29  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.1  2002/04/12 13:50:37  Jolt
 *   eNX/GTX merge
 *
 *
 *   $Revision: 1.7 $
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

#include "dbox/avia_gt.h"
#include "dbox/avia_gt_gv.h"
#include "dbox/avia_gt_pcm.h"
#include "dbox/avia_gt_capture.h"
#include "dbox/avia_gt_pig.h"

#ifdef MODULE
int chip_type = -1;
static int debug = 0;
unsigned char init_state = 0;

MODULE_PARM(debug, "i");
MODULE_PARM(chip_type, "i");
#endif

unsigned char *gt_mem_addr = NULL;
unsigned char *gt_reg_addr = NULL;

void (*gt_isr_proc_list[128])(unsigned short irq);

void avia_gt_clear_irq(unsigned char irq_reg, unsigned char irq_bit)
{

    if (chip_type == AVIA_GT_CHIP_TYPE_ENX)
	avia_gt_enx_clear_irq(irq_reg, irq_bit);
    else if (chip_type == AVIA_GT_CHIP_TYPE_GTX)
	avia_gt_gtx_clear_irq(irq_reg, irq_bit);
    
}

unsigned char avia_gt_get_chip_type(void)
{

    return chip_type;
    
}

unsigned short avia_gt_get_irq_mask(unsigned char irq_reg)
{

    if (chip_type == AVIA_GT_CHIP_TYPE_ENX)
	return avia_gt_enx_get_irq_mask(irq_reg);
    else if (chip_type == AVIA_GT_CHIP_TYPE_GTX)
	return avia_gt_gtx_get_irq_mask(irq_reg);

    return 0;

}
			    
unsigned short avia_gt_get_irq_status(unsigned char irq_reg)
{

    if (chip_type == AVIA_GT_CHIP_TYPE_ENX)
	return avia_gt_enx_get_irq_status(irq_reg);
    else if (chip_type == AVIA_GT_CHIP_TYPE_GTX)
	return avia_gt_gtx_get_irq_status(irq_reg);

    return 0;

}

unsigned char *avia_gt_get_mem_addr(void)
{

    return gt_mem_addr;
	
}

unsigned char *avia_gt_get_reg_addr(void)
{

    return gt_reg_addr;
	
}

void avia_gt_mask_irq(unsigned char irq_reg, unsigned char irq_bit)
{
    
    if (chip_type == AVIA_GT_CHIP_TYPE_ENX)
	avia_gt_enx_mask_irq(irq_reg, irq_bit);
    else if (chip_type == AVIA_GT_CHIP_TYPE_GTX)
	avia_gt_gtx_mask_irq(irq_reg, irq_bit);
	
}
	
void avia_gt_unmask_irq(unsigned char irq_reg, unsigned char irq_bit)
{

    if (chip_type == AVIA_GT_CHIP_TYPE_ENX)
	avia_gt_enx_unmask_irq(irq_reg, irq_bit);
    else if (chip_type == AVIA_GT_CHIP_TYPE_GTX)
	avia_gt_gtx_unmask_irq(irq_reg, irq_bit);

}
	
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

static void avia_gt_irq_handler(int irq, void *dev, struct pt_regs *regs)
{

    unsigned char irq_reg;
    unsigned char irq_bit;
    unsigned short irq_mask;
    unsigned short irq_status;
  
    for (irq_reg = 0; irq_reg < 6; irq_reg++) {
  
        irq_mask = avia_gt_get_irq_mask(irq_reg);
	irq_status = avia_gt_get_irq_status(irq_reg);
    
	for (irq_bit = 1; irq_bit < 16; irq_bit++) {
    
    	    if (irq_status & (1 << irq_bit)) {
      
		dprintk("avia_gt_core: interrupt reg %d bit %d\n", irq_reg, irq_bit);

    		if (gt_isr_proc_list[AVIA_GT_ISR_PROC_NR(irq_reg, irq_bit)]) {

		    gt_isr_proc_list[AVIA_GT_ISR_PROC_NR(irq_reg, irq_bit)](AVIA_GT_IRQ(irq_reg, irq_bit));

		} else {

		    if (irq_mask & (1 << irq_bit)) {
		    
			printk("avia_gt_core: masking unhandled irq reg %d bit %d\n", irq_reg, irq_bit);

			avia_gt_mask_irq(irq_reg, irq_bit);
			
		    }

		}

		avia_gt_clear_irq(irq_reg, irq_bit);

	    }

	}

    }

}

int __init avia_gt_init(void)
{

    int result;

    printk("avia_gt_core: $Id: avia_gt_core.c,v 1.7 2002/04/15 10:40:50 Jolt Exp $\n");
    
    if ((chip_type != AVIA_GT_CHIP_TYPE_ENX) && (chip_type != AVIA_GT_CHIP_TYPE_GTX)) {
    
        printk("avia_gt_core: Unsupported chip type (chip_type = %d)\n", chip_type);
	    
        return -EIO;
		    
    }
			    
    memset(gt_isr_proc_list, 0, sizeof(gt_isr_proc_list));
     
    if (chip_type == AVIA_GT_CHIP_TYPE_ENX)
	gt_reg_addr = (unsigned char*)ioremap(ENX_REG_BASE, ENX_REG_SIZE);
    else if (chip_type == AVIA_GT_CHIP_TYPE_GTX)
	gt_reg_addr = (unsigned char*)ioremap(GTX_REG_BASE, GTX_REG_SIZE);

    if (!gt_reg_addr) {
  
	printk(KERN_ERR "avia_gt_core: Failed to remap register space.\n");
    
	return -1;
    
    }
    
    init_state = 1;

    if (chip_type == AVIA_GT_CHIP_TYPE_ENX)
	gt_mem_addr = (unsigned char*)ioremap(ENX_MEM_BASE, ENX_MEM_SIZE);
    else if (chip_type == AVIA_GT_CHIP_TYPE_GTX)
	gt_mem_addr = (unsigned char*)ioremap(GTX_MEM_BASE, GTX_MEM_SIZE);

    if (!gt_mem_addr) {
  
	printk(KERN_ERR "avia_gt_core: Failed to remap memory space.\n");
	
	avia_gt_exit();
    
	return -1;
    
    }

    init_state = 2;

    if (chip_type == AVIA_GT_CHIP_TYPE_ENX)
	result = request_8xxirq(ENX_INTERRUPT, avia_gt_irq_handler, 0, "avia_gt", 0);
    else if (chip_type == AVIA_GT_CHIP_TYPE_GTX)
	result = request_8xxirq(GTX_INTERRUPT, avia_gt_irq_handler, 0, "avia_gt", 0);

    if (result) {
    
        printk(KERN_ERR "avia_gt_core: Could not allocate IRQ!\n");

	avia_gt_exit();
      
	return -1;
	
    }

    init_state = 3;

    if (chip_type == AVIA_GT_CHIP_TYPE_ENX)
	avia_gt_enx_init();
    else if (chip_type == AVIA_GT_CHIP_TYPE_GTX)
	avia_gt_gtx_init();

    init_state = 4;

#if (!defined(MODULE)) || (defined(MODULE) && !defined(STANDALONE))
    if (avia_gt_gv_init()) {

	avia_gt_exit();
      
	return -1;
	
    }

    init_state = 5;
    
    if (avia_gt_pcm_init()) {

	avia_gt_exit();
      
	return -1;
	
    }

    init_state = 6;
    
    if (avia_gt_capture_init()) {

	avia_gt_exit();
      
	return -1;
	
    }

    init_state = 7;
    
    if (avia_gt_pig_init()) {

	avia_gt_exit();
      
	return -1;
	
    }

    init_state = 8;
    
#endif
	    
    printk(KERN_NOTICE "avia_gt_core: Loaded AViA eNX/GTX driver\n");

    return 0;
  
}

void avia_gt_exit(void)
{

#if (!defined(MODULE)) || (defined(MODULE) && !defined(STANDALONE))
    if (init_state >= 8)
        avia_gt_pig_exit();
	
    if (init_state >= 7)
        avia_gt_capture_exit();
	
    if (init_state >= 6)
	avia_gt_pcm_exit();
	
    if (init_state >= 5)
	avia_gt_gv_exit();
#endif

    if (init_state >= 4) {
    
	if (chip_type == AVIA_GT_CHIP_TYPE_ENX)
    	    avia_gt_enx_exit();
   	else if (chip_type == AVIA_GT_CHIP_TYPE_GTX)
		avia_gt_gtx_exit();

    }

    if (init_state >= 3) {
    
        if (chip_type == AVIA_GT_CHIP_TYPE_ENX)
	    free_irq(ENX_INTERRUPT, 0);
    	else if (chip_type == AVIA_GT_CHIP_TYPE_GTX)
	    free_irq(GTX_INTERRUPT, 0);

    }

    if (init_state >= 2)
	iounmap(gt_mem_addr);
	
    if (init_state >= 1)
	iounmap(gt_reg_addr);
    
}

#ifdef MODULE
MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("Avia eNX/GTX driver");

EXPORT_SYMBOL(avia_gt_alloc_irq);
EXPORT_SYMBOL(avia_gt_free_irq);
EXPORT_SYMBOL(avia_gt_get_chip_type);
EXPORT_SYMBOL(avia_gt_get_mem_addr);
EXPORT_SYMBOL(avia_gt_get_reg_addr);

module_init(avia_gt_init);
module_exit(avia_gt_exit);
#endif
