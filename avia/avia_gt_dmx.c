/*
 *   avia_gt_dmx.c - AViA eNX/GTX dmx driver (dbox-II-project)
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
 *   $Log: avia_gt_dmx.c,v $
 *   Revision 1.77  2002/05/02 04:56:47  Jolt
 *   Merge
 *
 *   Revision 1.76  2002/05/01 21:53:00  Jolt
 *   Merge
 *
 *
 *
 *
 *   $Revision: 1.77 $
 *
 */

#define __KERNEL_SYSCALLS__ 

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
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/byteorder/swab.h>

#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/unistd.h>
#include <linux/vmalloc.h>
#include <linux/string.h> 
#include <linux/tqueue.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>


#include <dbox/avia_gt.h>
//#include <dbox/avia_gt_dmx.h>

static int errno;
static sAviaGtInfo *gt_info;
static char *ucode = NULL;

#ifdef MODULE
MODULE_PARM(ucode, "s");
#endif

unsigned char avia_gt_dmx_map_queue(unsigned char queue_nr)
{

    if (avia_gt_chip(ENX)) {
    
		if (queue_nr >= 16)
			enx_reg_s(CFGR0)->UPQ = 1;
		else
			enx_reg_s(CFGR0)->UPQ = 0;
	
    } else if (avia_gt_chip(GTX)) {

		if (queue_nr >= 16)
			gtx_reg_s(CR1)->UPQ = 1;
		else
			gtx_reg_s(CR1)->UPQ = 0;

    }
    
    mb();
    
    return (queue_nr & 0x0F);

}

void avia_gt_dmx_force_discontinuity(void)
{

//    discont = 1;

    if (avia_gt_chip(ENX))
		enx_reg_16(FC) |= 0x100;
    else if (avia_gt_chip(GTX))
		rh(FCR) |= 0x100;

}

unsigned char avia_gt_dmx_get_queue_size(unsigned char queue_nr)
{

	queue_nr = avia_gt_dmx_map_queue(queue_nr);
	
	if (avia_gt_chip(ENX))
		return enx_reg_so(QWPnH, 4 * queue_nr)->Q_Size;
	else if (avia_gt_chip(GTX))
		return gtx_reg_so(QWPnH, 4 * queue_nr)->Q_Size;

	return 0;
    
}

unsigned int avia_gt_dmx_get_queue_write_pointer(unsigned char queue_nr)
{

	unsigned int previous_write_pointer;
	unsigned int write_pointer = 0xFFFFFFFF;

	queue_nr = avia_gt_dmx_map_queue(queue_nr);

    /*
     *
     * CAUTION: The correct sequence for accessing Queue
     * Pointer registers is as follows:
     * For reads,
     * Read low word
     * Read high word
     * CAUTION: Not following these sequences will yield
     * invalid data.
     *
     */
     
	do {
    
		previous_write_pointer = write_pointer;
    
		if (avia_gt_chip(ENX)) {
	
			//write_pointer = enx_reg_so(QWPnL, 4 * queue_nr)->Queue_n_Write_Pointer;
			//write_pointer |= ((enx_reg_so(QWPnH, 4 * queue_nr)->Queue_n_Write_Pointer) << 16);

			write_pointer = ((enx_reg_so(QWPnL, 4 * queue_nr)->Queue_n_Write_Pointer) | (enx_reg_so(QWPnH, 4 * queue_nr)->Queue_n_Write_Pointer << 16));

		} else if (avia_gt_chip(GTX)) {
	
			write_pointer = ((gtx_reg_so(QWPnL, 4 * queue_nr)->Queue_n_Write_Pointer) | (gtx_reg_so(QWPnH, 4 * queue_nr)->Upper_WD_n << 16));

		}
	    
	} while (previous_write_pointer != write_pointer);

	return write_pointer;
    
}

int avia_gt_dmx_load_ucode(void)
{

	int fd;
	loff_t file_size;
	mm_segment_t fs;
	void *risc_ram;

    fs = get_fs();
    set_fs(get_ds());
	
	if ((fd = open(ucode, 0, 0)) < 0) {
	
		printk (KERN_ERR "avia_Gt_dmx: Unable to load '%s'\n", ucode);
		
		set_fs(fs);
		
		return -EFAULT;
		
	}

	file_size = lseek(fd, 0L, 2);
	
	if (file_size <= 0) {
	
		printk (KERN_ERR "avia_gt_dmx: Firmware wrong size '%s'\n", ucode);
		
		sys_close(fd);
		set_fs(fs);
		
		return -EFAULT;
		
	}

	lseek(fd, 0L, 0);
	
	if (avia_gt_chip(ENX))
		risc_ram = enx_reg_o(TDP_INSTR_RAM);
	else if (avia_gt_chip(GTX))
		risc_ram = (void *)&rh(RISC);

	if (read(fd, risc_ram, file_size) != file_size) {
	
		printk (KERN_ERR "avia_gt_dmx: Failed to read '%s'\n", ucode);
		
		sys_close(fd);
		set_fs(fs);
		
		return -EIO;
		
	}

	close(fd);
	set_fs(fs);
		
	return 0;

}

void avia_gt_dmx_set_queue(unsigned char queue_nr, unsigned int write_pointer, unsigned char size)
{

	queue_nr = avia_gt_dmx_map_queue(queue_nr);
    
	if (avia_gt_chip(ENX)) {
	
		enx_reg_16(QWPnL + 4 * queue_nr) = (write_pointer & 0xFFFF);
		enx_reg_16(QWPnH + 4 * queue_nr) = ((write_pointer >> 16) & 0x3F) | (size << 6);
		
	} else if (avia_gt_chip(GTX)) {
			    
		gtx_reg_16(QWPnL + 4 * queue_nr) = write_pointer & 0xFFFF;
		gtx_reg_16(QWPnH + 4 * queue_nr) = ((write_pointer >> 16) & 0x3F) | (size << 6);
					    
	}

}

void avia_gt_dmx_set_queue_write_pointer(unsigned char queue_nr, unsigned int write_pointer)
{

    avia_gt_dmx_set_queue(queue_nr, write_pointer, avia_gt_dmx_get_queue_size(queue_nr));

}

void avia_gt_dmx_set_queue_irq(unsigned char queue_nr, unsigned char qim, unsigned int irq_addr)
{

    if (!qim)
		irq_addr = 0;

	queue_nr = avia_gt_dmx_map_queue(queue_nr);
    
	if (avia_gt_chip(ENX))
		enx_reg_16n(0x08C0 + queue_nr * 2) = ((qim << 15) | (irq_addr & 0x7C00));
	else if (avia_gt_chip(GTX))
		gtx_reg_16(QIn + queue_nr * 2) = ((qim << 15) | (irq_addr & 0x7C00));

}

void gtx_set_queue_pointer(int queue, u32 read, u32 write, int size, int halt)
{

    int base;
    
    if (avia_gt_chip(ENX)) {
    
	base = queue * 8 + 0x8E0;
	
	enx_reg_16n(base) = read & 0xFFFF;
	enx_reg_16n(base + 4) = write & 0xFFFF;
	enx_reg_16n(base + 6) = ((write >> 16) & 63) | (size << 6);
	enx_reg_16n(base + 2) = ((read >> 16) & 63);

    } else if (avia_gt_chip(GTX)) {

	base = queue * 8 + 0x1E0;
	
	rhn(base) = read & 0xFFFF;
	rhn(base + 4) = write & 0xFFFF;
	rhn(base + 6) = ((write >> 16) & 63) | (size << 6);
	rhn(base + 2) = ((read >> 16) & 63) | (halt << 15);
	
    }	
	
}

void avia_gt_dmx_reset(unsigned char reenable)
{

    if (avia_gt_chip(ENX)) {
    
//	enx_reg_s(RSTR0)->RISC = 1;
	
    } else if (avia_gt_chip(GTX)) {

//	gtx_reg_s(RR0)->RISC = 1;
    
    }
    
}

void enx_tdp_start(void)
{

	enx_reg_32(RSTR0) &= ~(1 << 22); //clear tdp-reset bit
//	enx_tdp_trace();
	enx_reg_16(EC)=0;  // dann mal los...	
	
}

void enx_tdp_stop(void)
{

	enx_reg_32(EC) = 2;			//stop tdp
	
}

int __init avia_gt_dmx_init(void)
{

	int result;

    printk("avia_gt_dmx: $Id: avia_gt_dmx.c,v 1.77 2002/05/02 04:56:47 Jolt Exp $\n");

    gt_info = avia_gt_get_info();
    
    if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {
    
		printk("avia_gt_dmx: Unsupported chip type\n");
	
		return -EIO;
	
    }

    avia_gt_dmx_reset(1);


    if (avia_gt_chip(ENX)) {
    
		enx_reg_32(RSTR0)|=(1<<31)|(1<<23)|(1<<22);
		enx_tdp_start();

    } else if (avia_gt_chip(GTX)) {

		rh(RR1) |= 1 << 5;				 // reset RISC
		udelay (10);
		rh(RR1) &= ~(1 << 5);

    }
	
	if ((result = avia_gt_dmx_load_ucode()))
		return result;
    
    if (avia_gt_chip(ENX)) {
    
		enx_reg_32(RSTR0) &= ~(1 << 27);
		enx_reg_32(RSTR0) &= ~(1 << 13);
		enx_reg_32(RSTR0) &= ~(1 << 11);
		enx_reg_32(RSTR0) &= ~(1 << 9);
		enx_reg_32(RSTR0) &= ~(1 << 23);
		enx_reg_32(RSTR0) &= ~(1 << 31);
		
		enx_reg_32(CFGR0) &= ~(1 << 3);
		enx_reg_32(CFGR0) &= ~(1 << 1);
		enx_reg_32(CFGR0) &= ~(1 << 0);
	
		enx_reg_16(FC) = 0x9147;
		enx_reg_16(SYNC_HYST) =0x21;
		enx_reg_16(BQ) = 0x00BC;
	
		enx_reg_32(CFGR0) |= 1 << 24;		// enable dac output

		enx_reg_16(AVI_0) = 0xF;					// 0x6CF geht nicht (ordentlich)
		enx_reg_16(AVI_1) = 0xA;
	
		enx_reg_32(CFGR0) &= ~3; 				// disable clip mode

		printk("ENX-INITed -> %x\n", enx_reg_16(FIFO_PDCT));

		if (!enx_reg_16(FIFO_PDCT))
			printk("there MIGHT be no TS :(\n");

    } else if (avia_gt_chip(GTX)) {

	//	rh(RR1)&=~0x1C;							 // take framer, ci, avi module out of reset
		rh(RR1)|=1<<6;
		rh(RR1)&=~(1<<6);
		rh(RR0)=0;						// autsch, das muss so. kann das mal wer überprüfen?
		rh(RR1)=0;
		rh(RISCCON)=0;

		rh(FCR)=0x9147;							 // byte wide input
		rh(SYNCH)=0x21;

		rh(AVI)=0x71F;
		rh(AVI+2)=0xF;
	
    }

    return 0;
    
}

void __exit avia_gt_dmx_exit(void)
{

    avia_gt_dmx_reset(0);

}

#ifdef MODULE
EXPORT_SYMBOL(avia_gt_dmx_force_discontinuity);
EXPORT_SYMBOL(avia_gt_dmx_get_queue_size);
EXPORT_SYMBOL(avia_gt_dmx_get_queue_write_pointer);
EXPORT_SYMBOL(avia_gt_dmx_set_queue);
EXPORT_SYMBOL(avia_gt_dmx_set_queue_irq);
EXPORT_SYMBOL(avia_gt_dmx_set_queue_write_pointer);
EXPORT_SYMBOL(gtx_set_queue_pointer);
#endif

#if defined(MODULE) && defined(STANDALONE)
module_init(avia_gt_dmx_init);
module_exit(avia_gt_dmx_exit);
#endif
