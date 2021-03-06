/*
 *   avia_gt.h - AViA eNX/GTX driver (dbox-II-project)
 *
 *   Homepage: http://www.tuxbox.org
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

#include "avia_gt_config.h"
#include "avia_gt_enx.h"
#include "avia_gt_gtx.h"

#ifdef DEBUG
#define dprintk(fmt,args...) printk( fmt,## args)
#else
#define dprintk(...)
#endif

typedef struct {
	/* id */
	u8 chip_type;
	/* memory */
	u8 *mem_addr;
	u32 mem_addr_phys;
	u32 mem_size;
	/* registers */
	u8 *reg_addr;
	u32 reg_addr_phys;
	u32 reg_size;
	/* interrupts */
	int irq;
	u16 irq_lock;
	u16 irq_drop;
	u16 irq_pcr;
	u16 irq_it;
	u16 irq_ir;
	u16 irq_pf;
	u16 irq_ad;
	u16 irq_vl1;
	u16 irq_tt;
	/* infrared clock */
	u32 ir_clk;
	/* audio queue read pointer address */
	u16 aq_rptr;
	/* transport demux ram address */
	u16 tdp_ram;
} sAviaGtInfo;

#define avia_gt_chip(CHIP) 		(gt_info->chip_type == AVIA_GT_CHIP_TYPE_## CHIP)

#define AVIA_GT_CHIP_TYPE_ENX		0
#define AVIA_GT_CHIP_TYPE_GTX		1

#define AVIA_GT_HALFSYSCLK		(50 * 1000 * 1000)

#define AVIA_GT_ISR_PROC_NR(irq_reg, irq_bit) (irq_reg * 16 + irq_bit)

#define AVIA_GT_IRQ(reg, bit)		((((reg) & 0xFF) << 8) | ((bit) & 0xFF))
#define AVIA_GT_IRQ_BIT(irq)		((irq) & 0xFF)
#define AVIA_GT_IRQ_REG(irq)		(((irq) >> 8) & 0xFF)

#define AVIA_GT_MEM_SIZE		(2 * 1024 * 1024)
#define AVIA_GT_MEM_ALIGN(addr)		(((addr) + 3) & ~3)

/*
 * memory configuration
 */

#define AVIA_GT_MEM_GV_OFFS		AVIA_GT_MEM_ALIGN(0)
#ifdef CONFIG_AVIA_GT_GV
#define AVIA_GT_MEM_GV_SIZE		(720 * 576 * 2)
#else
#define AVIA_GT_MEM_GV_SIZE		0
#endif

#define AVIA_GT_MEM_CAPTURE_OFFS	AVIA_GT_MEM_ALIGN(AVIA_GT_MEM_GV_OFFS + AVIA_GT_MEM_GV_SIZE)
#ifdef CONFIG_AVIA_GT_CAPTURE
#define AVIA_GT_MEM_CAPTURE_SIZE	(720 * 576 / 2)
#else
#define AVIA_GT_MEM_CAPTURE_SIZE	0
#endif

#define AVIA_GT_MEM_DMX_OFFS		AVIA_GT_MEM_ALIGN(AVIA_GT_MEM_CAPTURE_OFFS + AVIA_GT_MEM_CAPTURE_SIZE)
#ifdef CONFIG_AVIA_GT_DMX
#define AVIA_GT_MEM_DMX_SIZE		(816 * 1024)
#else
#define AVIA_GT_MEM_DMX_SIZE		0
#endif

#define AVIA_GT_MEM_PCM_OFFS		AVIA_GT_MEM_ALIGN(AVIA_GT_MEM_DMX_OFFS + AVIA_GT_MEM_DMX_SIZE)
#ifdef CONFIG_AVIA_GT_PCM
#define AVIA_GT_MEM_PCM_SIZE		(1023 * 4 * 25)
#else
#define AVIA_GT_MEM_PCM_SIZE		0
#endif

#define AVIA_GT_MEM_IR_OFFS1		AVIA_GT_MEM_ALIGN(AVIA_GT_MEM_PCM_OFFS + AVIA_GT_MEM_PCM_SIZE)
#define AVIA_GT_MEM_IR_OFFS		((AVIA_GT_MEM_IR_OFFS1 + 0xFF) & ~0xFF)
#ifdef CONFIG_AVIA_GT_IR
#define AVIA_GT_MEM_IR_SIZE		512
#else
#define AVIA_GT_MEM_IR_SIZE		0
#endif

#if ((AVIA_GT_MEM_IR_OFFS + AVIA_GT_MEM_IR_SIZE) > AVIA_GT_MEM_SIZE)
#error AViA memory pool exceeds chip memory!
#endif

extern int avia_gt_alloc_irq(unsigned short irq, void (*isr_proc)(unsigned short irq));
extern void avia_gt_free_irq(unsigned short irq);
extern sAviaGtInfo *avia_gt_get_info(void);

extern int avia_gt_init(void);
extern void avia_gt_exit(void);

#define avia_gt_readw(reg)						\
({									\
 	u16 __w;							\
	if (avia_gt_chip(GTX))						\
		__w = gtx_reg_16(reg);					\
	else								\
		__w = enx_reg_16(reg);					\
	__w;								\
})

#define avia_gt_readl(reg)						\
({									\
 	u32 __l;							\
	if (avia_gt_chip(GTX))						\
		__l = gtx_reg_32(reg);					\
	else								\
		__l = enx_reg_32(reg);					\
	__l;								\
})

#define avia_gt_writew(reg, val)					\
do {									\
 	if (avia_gt_chip(GTX))						\
		gtx_reg_16(reg) = (val);				\
	else								\
		enx_reg_16(reg) = (val);				\
} while (0)

#define avia_gt_writel(reg, val)					\
do {									\
 	if (avia_gt_chip(GTX))						\
		gtx_reg_32(reg) = (val);				\
	else								\
		enx_reg_32(reg) = (val);				\
} while (0)

#define avia_gt_reg_o(offs)						\
	((volatile void *)(&gt_info->reg_addr[offs]))

#define avia_gt_reg_16n(offset)						\
	(*((volatile u16 *)avia_gt_reg_o(offset)))

#define avia_gt_reg_32n(offset)						\
	(*((volatile u32 *)avia_gt_reg_o(offset)))

#define avia_gt_reg_set(reg, field, val)				\
do {									\
	if (avia_gt_chip(GTX))						\
		gtx_reg_set(reg, field, val);				\
	else								\
		enx_reg_set(reg, field, val);				\
} while (0)

extern inline int avia_gt_supported_chipset(sAviaGtInfo *gt_info)
{
	if ((!gt_info) || ((!avia_gt_chip(GTX)) && (!avia_gt_chip(ENX)))) {
		printk(KERN_ERR "%s: unsupported chipset\n", __FILE__);
		return 0;
	}

	return 1;
}

#endif
