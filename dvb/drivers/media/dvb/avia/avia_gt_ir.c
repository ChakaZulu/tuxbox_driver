/*
 * $Id: avia_gt_ir.c,v 1.26 2003/07/24 01:14:20 homar Exp $
 *
 * AViA eNX/GTX ir driver (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2002 Florian Schirmer (jolt@tuxbox.org)
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

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/wait.h>

#include "avia_gt.h"
#include "avia_gt_ir.h"

DECLARE_WAIT_QUEUE_HEAD(rx_wait);
DECLARE_WAIT_QUEUE_HEAD(tx_wait);

static sAviaGtInfo *gt_info = NULL;
static u32 duty_cycle = 33;
static u16 first_period_low = 0;
static u16 first_period_high = 0;
static u32 frequency = 38000;
#define RX_MAX 50000
static sAviaGtIrPulse rx_buffer[RX_MAX];
static u32 rx_buffer_read_position = 0;
static u32 rx_buffer_write_position = 0;
static u8 rx_unit_busy = 0;
static sAviaGtTxIrPulse *tx_buffer = NULL;
static u8 tx_buffer_pulse_count = 0;
static u8 tx_unit_busy = 0;

#define IR_TICK_LENGTH (1000 * 1125 / 8)
#define TICK_COUNT_TO_USEC(tick_count) ((tick_count) * IR_TICK_LENGTH / 1000)

static struct timeval last_timestamp;

static void avia_gt_ir_tx_irq(unsigned short irq)
{
	dprintk("avia_gt_ir: tx irq\n");

	tx_unit_busy = 0;

	wake_up_interruptible(&tx_wait);
}

static void avia_gt_ir_rx_irq(unsigned short irq)
{
	struct timeval timestamp;

	do_gettimeofday(&timestamp);

	rx_buffer[rx_buffer_write_position].high = enx_reg_s(RPH)->RTCH * IR_TICK_LENGTH / 1000;
	rx_buffer[rx_buffer_write_position].low = ((timestamp.tv_sec - last_timestamp.tv_sec) * 1000 * 1000) + (timestamp.tv_usec - last_timestamp.tv_usec) - rx_buffer[rx_buffer_write_position].high;

	last_timestamp = timestamp;

	if (rx_buffer_write_position < (RX_MAX - 1))
		rx_buffer_write_position++;
	else
		rx_buffer_write_position = 0;

	rx_unit_busy = 0;

	wake_up_interruptible(&rx_wait);
}

void avia_gt_ir_enable_rx_dma(unsigned char enable, unsigned char offset)
{
	avia_gt_reg_set(IRRO, Offset, 0);
	avia_gt_reg_set(IRRE, Offset, offset >> 1);
	avia_gt_reg_set(IRRE, E, enable);
}

void avia_gt_ir_enable_tx_dma(unsigned char enable, unsigned char length)
{
	avia_gt_reg_set(IRTO, Offset, 0);
	avia_gt_reg_set(IRTE, Offset, length - 1);
	avia_gt_reg_set(IRTE, C, 0);
	avia_gt_reg_set(IRTE, E, enable);
}

u32 avia_gt_ir_get_rx_buffer_read_position(void)
{
	return rx_buffer_read_position;
}

u32 avia_gt_ir_get_rx_buffer_write_position(void)
{
	return rx_buffer_write_position;
}

int avia_gt_ir_queue_pulse(u32 period_high, u32 period_low, u8 block)
{
	WAIT_IR_UNIT_READY(tx);

	if (tx_buffer_pulse_count >= AVIA_GT_IR_MAX_PULSE_COUNT)
		return -EBUSY;

	if (tx_buffer_pulse_count == 0) {
		first_period_high = period_high;
		first_period_low = period_low;
	}
	else {
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

wait_queue_head_t *avia_gt_ir_receive_data(void)
{
	return &rx_wait;
}

int avia_gt_ir_receive_pulse(u32 *period_low, u32 *period_high, u8 block)
{
	if (rx_buffer_write_position == rx_buffer_read_position) {
		rx_unit_busy = 1;
		WAIT_IR_UNIT_READY(rx);
	}

	if (period_low)
		*period_low = rx_buffer[rx_buffer_read_position].low;

	if (period_high)
		*period_high = rx_buffer[rx_buffer_read_position].high;

	if (rx_buffer_read_position < RX_MAX)
		rx_buffer_read_position++;
	else
		rx_buffer_read_position = 0;

	return 0;
}

void avia_gt_ir_reset(int reenable)
{
	avia_gt_reg_set(RSTR0, IR, 1);

	if (reenable)
		avia_gt_reg_set(RSTR0, IR, 0);
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

int avia_gt_ir_send_pulse(u32 period_high, u32 period_low, u8 block)
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
	}
	else if (avia_gt_chip(GTX)) {
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

	avia_gt_reg_set(CWPH, WavePulseHigh, ((gt_info->ir_clk * duty_cycle) / (frequency * 100)) - 1);
}

void avia_gt_ir_set_frequency(u32 new_frequency)
{
	frequency = new_frequency;

	avia_gt_reg_set(CWP, CarrierWavePeriod, (gt_info->ir_clk / frequency) - 1);

	avia_gt_ir_set_duty_cycle(duty_cycle);
}

void avia_gt_ir_set_filter(u8 enable, u8 low, u8 high)
{
	avia_gt_reg_set(RFR, Filt_H, high);
	avia_gt_reg_set(RFR, Filt_L, low);
	avia_gt_reg_set(RTC, S, enable);
}

void avia_gt_ir_set_polarity(u8 polarity)
{
	avia_gt_reg_set(RFR, P, polarity);
}

// Given in usec / 1000
void avia_gt_ir_set_tick_period(u32 tick_period)
{
	avia_gt_reg_set(RTP, TickPeriod, (1000 * 1000 * 1000 / tick_period) - 1);
}

void avia_gt_ir_set_queue(unsigned int addr)
{
	avia_gt_reg_set(IRQA, Addr, addr >> 9);

	//rx_buffer = (sAviaGtRxIrPulse *)(gt_info->mem_addr + addr);
	tx_buffer = (sAviaGtTxIrPulse *)(&gt_info->mem_addr[addr + 256]);
}

int avia_gt_ir_init(void)
{
	dprintk(KERN_INFO "avia_gt_ir: $Id: avia_gt_ir.c,v 1.26 2003/07/24 01:14:20 homar Exp $\n");

	do_gettimeofday(&last_timestamp);

	gt_info = avia_gt_get_info();

	if (!avia_gt_supported_chipset(gt_info))
		return -ENODEV;

	if (avia_gt_alloc_irq(gt_info->irq_irrx, avia_gt_ir_rx_irq)) {
		dprintk(KERN_ERR "avia_gt_ir: unable to get rx interrupt\n");
		return -EIO;
	}

	if (avia_gt_alloc_irq(gt_info->irq_irtx, avia_gt_ir_tx_irq)) {
		dprintk(KERN_ERR "avia_gt_ir: unable to get tx interrupt\n");
		avia_gt_free_irq(gt_info->irq_irrx);
		return -EIO;
	}

	avia_gt_ir_reset(1);

	avia_gt_ir_set_tick_period(IR_TICK_LENGTH);
	avia_gt_ir_set_filter(0, 3, 5);
	avia_gt_ir_set_polarity(0);
	avia_gt_ir_set_queue(AVIA_GT_MEM_IR_OFFS);
	avia_gt_ir_set_frequency(frequency);

	return 0;
}

void avia_gt_ir_exit(void)
{
	avia_gt_free_irq(gt_info->irq_irtx);
	avia_gt_free_irq(gt_info->irq_irrx);
	avia_gt_ir_reset(0);
}

#if defined(STANDALONE)
module_init(avia_gt_ir_init);
module_exit(avia_gt_ir_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("AViA eNX/GTX infrared rx/tx driver");
MODULE_LICENSE("GPL");
#endif

EXPORT_SYMBOL(avia_gt_ir_get_rx_buffer_read_position);
EXPORT_SYMBOL(avia_gt_ir_get_rx_buffer_write_position);
EXPORT_SYMBOL(avia_gt_ir_queue_pulse);
EXPORT_SYMBOL(avia_gt_ir_receive_data);
EXPORT_SYMBOL(avia_gt_ir_receive_pulse);
EXPORT_SYMBOL(avia_gt_ir_send_buffer);
EXPORT_SYMBOL(avia_gt_ir_send_pulse);
EXPORT_SYMBOL(avia_gt_ir_set_duty_cycle);
EXPORT_SYMBOL(avia_gt_ir_set_frequency);
EXPORT_SYMBOL(avia_gt_ir_init);
EXPORT_SYMBOL(avia_gt_ir_exit);
