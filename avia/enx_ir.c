/*
 *   enx_ir.c - AViA eNX ir driver (dbox-II-project)
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
 *   $Log: enx_ir.c,v $
 *   Revision 1.1  2002/04/01 22:28:09  Jolt
 *   Basic IR support for eNX - more 2 come
 *
 *
 *
 *   $Revision: 1.1 $
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

#include <dbox/enx.h>

unsigned int rx_buf_offs;

static void enx_ir_rx_irq(int reg, int bit)
{

    //printk("enx_ir: rx irq (reg=%d, bit=%d)\n", reg, bit);
    printk("enx_ir: rx irq (RTCH=%d, RTC=%d)\n", enx_reg_s(RPH)->RTCH, enx_reg_s(RTC)->RTC);

}

static void enx_ir_tx_irq(int reg, int bit)
{

    printk("enx_ir: tx irq (reg=%d, bit=%d)\n", reg, bit);

}

void enx_ir_reset_tick_count(void)
{

    enx_reg_s(RTC)->RTC = 0;

}

void enx_ir_set_dma(unsigned char enable, unsigned char offset)
{

    enx_reg_s(IRRE)->Offset = offset;
    enx_reg_s(IRRE)->E = enable;

}

void enx_ir_set_filter(unsigned char enable, unsigned char polarity, unsigned char low, unsigned char high)
{

    enx_reg_s(RFR)->P = polarity;
    enx_reg_s(RFR)->Filt_H = high;
    enx_reg_s(RFR)->Filt_L = low;
    
    enx_reg_s(RTC)->S = enable;

}

void enx_ir_set_tick_period(unsigned short tick_period)
{

    enx_reg_s(RTP)->TickPeriod = tick_period - 1;

}

void enx_ir_set_queue(unsigned int addr)
{

    enx_reg_s(IRQA)->Addr = addr >> 9;
    enx_reg_s(IRRO)->Offset = 0;
    
    rx_buf_offs = 0;

}

static int enx_ir_init(void)
{

    printk("enx_ir: $Id: enx_ir.c,v 1.1 2002/04/01 22:28:09 Jolt Exp $\n");

    if (enx_allocate_irq(ENX_IRQ_IR_RX, enx_ir_rx_irq) != 0) {

	printk("enx_ir: unable to get rx interrupt\n");
	
	return -EIO;
	
    }
		
    if (enx_allocate_irq(ENX_IRQ_IR_TX, enx_ir_tx_irq) != 0) {

	printk("enx_ir: unable to get tx interrupt\n");

	enx_free_irq(ENX_IRQ_IR_RX);
	
	return -EIO;
	
    }
		
    // Reset IR module
    enx_reg_s(RSTR0)->IR = 1;
    enx_reg_s(RSTR0)->IR = 0;
    
    enx_ir_set_tick_period(7032);
    enx_ir_set_filter(0, 0, 3, 5);
    enx_ir_set_queue(ENX_IR_OFFSET);
    enx_ir_reset_tick_count();
    enx_ir_set_dma(1, rx_buf_offs + 1);

    return 0;
    
}

static void __exit enx_ir_cleanup(void)
{

    printk("enx_ir: cleanup\n");

    enx_free_irq(ENX_IRQ_IR_RX);
    enx_free_irq(ENX_IRQ_IR_TX);
    
    enx_reg_s(RSTR0)->IR = 1;

}

#ifdef MODULE
module_init(enx_ir_init);
module_exit(enx_ir_cleanup);
#endif
