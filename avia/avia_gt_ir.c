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
 *   Revision 1.14  2002/05/11 22:46:16  Jolt
 *   IR fixes
 *
 *   Revision 1.13  2002/05/11 21:25:04  obi 
 *   export symbols for avia_gt_lirc 
 *
 *   Revision 1.12  2002/05/11 21:09:34  Jolt
 *   GTX stuff
 *
 *   Revision 1.11  2002/05/11 20:32:08  Jolt
 *   Some pre-GTX stuff
 *
 *   Revision 1.10  2002/05/11 20:23:22  Jolt
 *   DMA IR mode added
 *
 *   Revision 1.9  2002/05/11 15:28:00  Jolt
 *   IR improvements
 *
 *   Revision 1.8  2002/05/11 01:02:21  Jolt
 *   IR stuff now working for eNX - THX TMB!
 *
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
 *   $Revision: 1.14 $
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
#include <linux/init.h>

#include <dbox/avia_gt.h>
#include <dbox/avia_gt_ir.h>

DECLARE_WAIT_QUEUE_HEAD(tx_wait);

static sAviaGtInfo *gt_info;
u32 duty_cycle = 33;
u16 first_period_low;
u16 first_period_high;
u32 frequency = 38000;
sAviaGtIrPulse *rx_buffer;
sAviaGtIrPulse *tx_buffer;
u8 tx_buffer_pulse_count = 0;
u8 tx_unit_busy = 0;

static void avia_gt_ir_tx_irq(unsigned short irq)
{

	dprintk("avia_gt_ir: tx irq\n");

	tx_unit_busy = 0;
	
	wake_up_interruptible(&tx_wait);

}

static void avia_gt_ir_rx_irq(unsigned short irq)
{

	dprintk("avia_gt_ir: rx irq\n");

}

void avia_gt_ir_enable_rx_dma(unsigned char enable, unsigned char offset)
{

	if (avia_gt_chip(ENX)) {
	
    	enx_reg_s(IRRO)->Offset = 0;
		
    	enx_reg_s(IRRE)->Offset = offset;
	    enx_reg_s(IRRE)->E = enable;
		
	} else if (avia_gt_chip(GTX)) {

    	gtx_reg_s(IRRO)->Offset = 0;
		
    	gtx_reg_s(IRRE)->Offset = offset;
	    gtx_reg_s(IRRE)->E = enable;
	
	}

}

void avia_gt_ir_enable_tx_dma(unsigned char enable, unsigned char length)
{

	if (avia_gt_chip(ENX)) {
	
		enx_reg_s(IRTO)->Offset = 0;
	
		enx_reg_s(IRTE)->Offset = length - 1;
		enx_reg_s(IRTE)->C = 0;
		enx_reg_s(IRTE)->E = enable;
		
	} else if (avia_gt_chip(GTX)) {

		gtx_reg_s(IRTO)->Offset = 0;
	
		gtx_reg_s(IRTE)->Offset = length - 1;
		gtx_reg_s(IRTE)->C = 0;
		gtx_reg_s(IRTE)->E = enable;
	
	}

}

int avia_gt_ir_queue_pulse(unsigned short period_high, unsigned short period_low, u8 block)
{

	WAIT_IR_UNIT_READY(tx);
	
	if (tx_buffer_pulse_count >= AVIA_GT_IR_MAX_PULSE_COUNT)
		return -EBUSY;
		
	if (tx_buffer_pulse_count == 0) {
	
		first_period_high = period_high;
		first_period_low = period_low;
	
	} else {
	
		tx_buffer[tx_buffer_pulse_count - 1].MSPR = USEC_TO_CWP(period_high + period_low) - 1;

		if (period_low != 0)
			tx_buffer[tx_buffer_pulse_count - 1].MSPL = USEC_TO_CWP(period_low) - 1;
		else
			tx_buffer[tx_buffer_pulse_count - 1].MSPL = 0;
			// Mhhh doesnt work :(
			//tx_buffer[tx_buffer_pulse_count - 1].MSPL = USEC_TO_CWP(period_high) - 1;

	}
	
	tx_buffer_pulse_count++;
	
	return 0;
	
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

int avia_gt_ir_send_buffer(u8 block)
{

	WAIT_IR_UNIT_READY(tx);

	if (tx_buffer_pulse_count == 0)
		return 0;
	
	if (tx_buffer_pulse_count >= 2)
		avia_gt_ir_enable_tx_dma(1, tx_buffer_pulse_count);

	avia_gt_ir_send_pulse(first_period_high, first_period_low, block);
	
	tx_buffer_pulse_count = 0;
	
	return 0;
	
}

int avia_gt_ir_send_pulse(unsigned short period_high, unsigned short period_low, u8 block)
{

	WAIT_IR_UNIT_READY(tx);

	tx_unit_busy = 1;
					
	if (avia_gt_chip(ENX)) {

		// Verify this	
		if (period_low != 0)
			enx_reg_16(MSPL) = USEC_TO_CWP(period_low) - 1;
		else
			enx_reg_16(MSPL) = USEC_TO_CWP(period_high) - 1;
			
		enx_reg_16(MSPR) = (1 << 10) | (USEC_TO_CWP(period_high + period_low) - 1);

	} else if (avia_gt_chip(GTX)) {

		// Verify this	
		if (period_low != 0)
			gtx_reg_16(MSPL) = USEC_TO_CWP(period_low) - 1;
		else
			gtx_reg_16(MSPL) = USEC_TO_CWP(period_high) - 1;
			
		gtx_reg_16(MSPR) = (1 << 10) | (USEC_TO_CWP(period_high + period_low) - 1);
	
	}
	
	return 0;

}

void avia_gt_ir_set_duty_cycle(u32 new_duty_cycle)
{

	duty_cycle = new_duty_cycle;

	if (avia_gt_chip(ENX))
		enx_reg_s(CWPH)->WavePulseHigh = ((AVIA_GT_HALFSYSCLK * duty_cycle) / (frequency * 100)) - 1;
	else if (avia_gt_chip(GTX))
		gtx_reg_s(CWPH)->WavePulseHigh = ((AVIA_GT_HALFSYSCLK * duty_cycle) / (frequency * 100)) - 1;

}

void avia_gt_ir_set_frequency(u32 new_frequency)
{

	frequency = new_frequency;

	if (avia_gt_chip(ENX))
		enx_reg_s(CWP)->CarrierWavePeriod = (AVIA_GT_HALFSYSCLK / frequency) - 1;
	else if (avia_gt_chip(GTX))
		gtx_reg_s(CWP)->CarrierWavePeriod = (AVIA_GT_HALFSYSCLK / frequency) - 1;

	avia_gt_ir_set_duty_cycle(duty_cycle);

}

void avia_gt_ir_set_filter(unsigned char enable, unsigned char polarity, unsigned char low, unsigned char high)
{

	if (avia_gt_chip(ENX)) {
	
	    enx_reg_s(RFR)->P = polarity;
	    enx_reg_s(RFR)->Filt_H = high;
	    enx_reg_s(RFR)->Filt_L = low;
    
	    enx_reg_s(RTC)->S = enable;
	
	} else if (avia_gt_chip(GTX)) {

	    gtx_reg_s(RFR)->P = polarity;
	    gtx_reg_s(RFR)->Filt_H = high;
	    gtx_reg_s(RFR)->Filt_L = low;
    
	    gtx_reg_s(RTC)->S = enable;

	}

}

void avia_gt_ir_set_tick_period(unsigned short tick_period)
{

	if (avia_gt_chip(ENX))
	    enx_reg_s(RTP)->TickPeriod = tick_period - 1;
	else if (avia_gt_chip(GTX))
	    gtx_reg_s(RTP)->TickPeriod = tick_period - 1;

}

void avia_gt_ir_set_queue(unsigned int addr)
{

	if (avia_gt_chip(ENX))
	    enx_reg_s(IRQA)->Addr = addr >> 9;
	else if (avia_gt_chip(GTX))
	    gtx_reg_s(IRQA)->Address = addr >> 9;

	rx_buffer = (sAviaGtIrPulse *)(gt_info->mem_addr + addr);
	tx_buffer = (sAviaGtIrPulse *)(gt_info->mem_addr + addr + 256);
    
}

int __init avia_gt_ir_init(void)
{

	u16 rx_irq;
	u16 tx_irq;

    printk("avia_gt_ir: $Id: avia_gt_ir.c,v 1.14 2002/05/11 22:46:16 Jolt Exp $\n");
	
	gt_info = avia_gt_get_info();
		
	if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {
			
		printk("avia_gt_ir: Unsupported chip type\n");
					
		return -EIO;
							
	}
	
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

	avia_gt_ir_set_frequency(frequency);
	
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

#ifdef MODULE
EXPORT_SYMBOL(avia_gt_ir_queue_pulse);
EXPORT_SYMBOL(avia_gt_ir_send_buffer);
EXPORT_SYMBOL(avia_gt_ir_send_pulse);
EXPORT_SYMBOL(avia_gt_ir_set_duty_cycle);
EXPORT_SYMBOL(avia_gt_ir_set_frequency);
#endif

#if defined(MODULE) && defined(STANDALONE)
module_init(avia_gt_ir_init);
module_exit(avia_gt_ir_exit);
#endif
