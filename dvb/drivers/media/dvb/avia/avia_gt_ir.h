/*
 *   avia_gt_ir.h - ir driver for AViA (dbox-II-project)
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
 */

#ifndef AVIA_GT_IR_H
#define AVIA_GT_IR_H

#define AVIA_GT_IR_MAX_PULSE_COUNT	(128 + 1)

#define USEC_TO_CWP(period)			((((period) * frequency) + 500000) / 1000000)

#define WAIT_IR_UNIT_READY(unit)	if (unit##_unit_busy) { 												\
																											\
										if (block) {														\
																											\
											if (wait_event_interruptible(unit##_wait, !unit##_unit_busy))	\
									               return -ERESTARTSYS;										\
																											\
										} else {															\
																											\
											return -EWOULDBLOCK;											\
																											\
										}																	\
																											\
									}

typedef struct {

	u8 MSPR;
	u8 MSPL;

} sAviaGtIrPulse;

extern int avia_gt_ir_queue_pulse(unsigned short period_high, unsigned short period_low, u8 block);
extern int avia_gt_ir_send_buffer(u8 block);
extern int avia_gt_ir_send_pulse(unsigned short period_high, unsigned short period_low, u8 block);
extern void avia_gt_ir_set_duty_cycle(u32 new_duty_cycle);
extern void avia_gt_ir_set_frequency(u32 new_frequency);
extern int avia_gt_ir_init(void);
extern void avia_gt_ir_exit(void);
	    
#endif
