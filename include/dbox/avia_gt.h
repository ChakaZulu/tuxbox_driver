/*
 *   avia_gt.h - AViA eNX/GTX driver (dbox-II-project)
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

#ifndef AVIA_GT_H
#define AVIA_GT_H

#include "enx.h"
#include "gtx.h"

#define dprintk(fmt,args...) if(debug) printk( fmt,## args)

#define AVIA_GT_CHIP_TYPE_ENX 0
#define AVIA_GT_CHIP_TYPE_GTX 1

#define AVIA_GT_ISR_PROC_NR(irq_reg, irq_bit) (irq_reg * 16 + irq_bit)



extern int avia_gt_alloc_irq(unsigned char irq_reg, unsigned char irq_bit, void (*isr_proc)(unsigned char irg_reg, unsigned char irq_bit));
extern unsigned char avia_gt_get_chip_type(void);
extern unsigned char* avia_gt_get_mem_addr(void);
extern unsigned char* avia_gt_get_reg_addr(void);
extern void avia_gt_free_irq(unsigned char irq_reg, unsigned char irq_bit);

extern int avia_gt_init(void);
extern void avia_gt_exit(void);

#endif
	    