/*
 *   avia_gt_ir.c - AViA eNX ir driver (dbox-II-project)
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
 *   $Log: avia_gt_ir.c,v $
 *   Revision 1.7  2002/05/09 19:18:05  Jolt
 *   IR stuff
 *
 *   Revision 1.6  2002/05/08 13:26:58  Jolt
 *   Disable ir-dos :)
 *
 *   Revision 1.5  2002/05/07 19:41:02  Jolt
 *   IR tests
 *
 *   Revision 1.4  2002/05/07 17:03:48  Jolt
 *   Small fix
 *
 *   Revision 1.3  2002/05/07 16:59:19  Jolt
 *   Misc stuff and cleanups
 *
 *   Revision 1.2  2002/05/07 16:40:32  Jolt
 *   IR stuff
 *
 *   Revision 1.1  2002/04/01 22:28:09  Jolt
 *   Basic IR support for eNX - more 2 come
 *
 *
 *
 *   $Revision: 1.7 $
 *
 */

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
#include <linux/init.h>

#include <dbox/avia_gt.h>

static sAviaGtInfo *gt_info;
unsigned int rx_buf_offs;
unsigned int tx_buf_offs;

void avia_gt_ir_transmit_pulse(unsigned short period, unsigned char duty_cycle);

static void avia_gt_ir_tx_irq(unsigned short irq)
{

  //  printk("avia_gt_ir: tx irq\n");
    printk("T");

}

static void avia_gt_ir_rx_irq(unsigned short irq)
{

  //  printk("avia_gt_ir: rx irq\n");
    printk("R");

}

void avia_gt_ir_reset(unsigned char reenable)
{

	if (avia_gt_chip(ENX))
        enx_reg_s(RSTR0)->IR = 1;
	else if (avia_gt_chip(GTX))
		gtx_reg_s(RR0)->IR = 1;
						
    if (reenable) {

		if (avia_gt_chip(ENX))
	        enx_reg_s(RSTR0)->IR = 0;
		else if (avia_gt_chip(GTX))
			gtx_reg_s(RR0)->IR = 0;

	}

}

void avia_gt_ir_set_dma(unsigned char enable, unsigned char offset)
{

    enx_reg_s(IRRE)->Offset = offset;
    enx_reg_s(IRRE)->E = enable;

}

void avia_gt_ir_set_filter(unsigned char enable, unsigned char polarity, unsigned char low, unsigned char high)
{

    enx_reg_s(RFR)->P = polarity;
    enx_reg_s(RFR)->Filt_H = high;
    enx_reg_s(RFR)->Filt_L = low;
    
    enx_reg_s(RTC)->S = enable;

}

void avia_gt_ir_set_tick_period(unsigned short tick_period)
{

    enx_reg_s(RTP)->TickPeriod = tick_period - 1;

}

void avia_gt_ir_set_queue(unsigned int addr)
{

    enx_reg_s(IRQA)->Addr = addr >> 9;
    enx_reg_s(IRRO)->Offset = 0 >> 1;
    enx_reg_s(IRTO)->Offset = 0 >> 1;
    
    rx_buf_offs = 0;
    tx_buf_offs = 256;

}

void avia_gt_ir_transmit_pulse(unsigned short period, unsigned char duty_cycle)
{

	enx_reg_16(MSPL) = duty_cycle;
	enx_reg_16(MSPR) = (1 << 10) | (period & 0x03FF);

}

int __init avia_gt_ir_init(void)
{

	u16 rx_irq;
	u16 tx_irq;

    printk("avia_gt_ir: $Id: avia_gt_ir.c,v 1.7 2002/05/09 19:18:05 Jolt Exp $\n");
	
	gt_info = avia_gt_get_info();
		
	if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {
			
		printk("avia_gt_ir: Unsupported chip type\n");
					
		return -EIO;
							
	}

	// REMOVE THIS IF GTX SUPPORT IS AVAILIBLE!!!!!!	
	if (avia_gt_chip(GTX))
		return 0;
	
	if (avia_gt_chip(ENX)) {
	
		rx_irq = ENX_IRQ_IR_RX;
		tx_irq = ENX_IRQ_IR_TX;
	
	} else if (avia_gt_chip(GTX)) {

		rx_irq = GTX_IRQ_IR_RX;
		tx_irq = GTX_IRQ_IR_TX;
	
	}

	// For testing only
	avia_gt_free_irq(rx_irq);	
	avia_gt_free_irq(tx_irq);	
	
    if (avia_gt_alloc_irq(rx_irq, avia_gt_ir_rx_irq)) {

		printk("avia_gt_ir: unable to get rx interrupt\n");

		return -EIO;
	
    }
	
    if (avia_gt_alloc_irq(tx_irq, avia_gt_ir_tx_irq)) {

		printk("avia_gt_ir: unable to get tx interrupt\n");

		avia_gt_free_irq(rx_irq);
	
		return -EIO;
	
    }
		
	avia_gt_ir_reset(1);
	
    avia_gt_ir_set_tick_period(7032);
    avia_gt_ir_set_filter(0, 0, 3, 5);
    avia_gt_ir_set_queue(AVIA_GT_MEM_IR_OFFS);
    avia_gt_ir_set_dma(1, rx_buf_offs + 1);

	enx_reg_16(CWP) = 0x0523;
	enx_reg_16(CWPH) = 0x01B5;
	
//	avia_gt_ir_transmit_pulse(0x3FF, 0xFF);

    return 0;
    
}

void __exit avia_gt_ir_exit(void)
{

	if (avia_gt_chip(ENX)) {
	
		avia_gt_free_irq(ENX_IRQ_IR_TX);
		avia_gt_free_irq(ENX_IRQ_IR_RX);
		
	} else if (avia_gt_chip(GTX)) {

		avia_gt_free_irq(GTX_IRQ_IR_TX);
		avia_gt_free_irq(GTX_IRQ_IR_RX);

	}
    
	avia_gt_ir_reset(0);

}

#if defined(MODULE) && defined(STANDALONE)
module_init(avia_gt_ir_init);
module_exit(avia_gt_ir_exit);
#endif
