/*
 *   avia_gt_gtx.c - AViA GTX core driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2001 Felix "tmbinc" Domke (tmbinc@gmx.net)
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
 *   $Log: avia_gt_gtx.c,v $
 *   Revision 1.10  2002/04/22 17:40:01  Jolt
 *   Major cleanup
 *
 *   Revision 1.9  2002/04/16 15:57:23  Jolt
 *   GTX bugfix
 *
 *   Revision 1.8  2002/04/15 21:58:57  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.7  2002/04/13 23:19:05  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.6  2002/04/13 14:47:19  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.5  2002/04/12 23:20:25  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.4  2002/04/12 21:31:37  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.3  2002/03/06 09:04:10  gillem
 *   - clean module unload
 *
 *   Revision 1.2  2001/12/12 17:20:38  obi
 *   version history was gone
 *
 *   Revision 1.1  2001/12/12 01:47:10  obi
 *   re-added with correct file rights
 *
 *   Revision 1.2  2001/10/15 21:04:33  tmbinc
 *   sorry, CRLF sucks   
 *
 *   Revision 1.1  2001/10/15 20:59:47  tmbinc
 *   re-added because of lameness
 *
 *   Revision 1.26  2001/08/18 18:21:10  TripleDES
 *   moved the ucode loading to dmx
 *
 *   Revision 1.25  2001/07/22 09:55:38  gillem
 *   - define bug fixed
 *
 *   Revision 1.24  2001/07/19 22:22:46  gillem
 *   - add proc fs
 *
 *   Revision 1.23  2001/05/15 22:42:03  kwon
 *   make do_firmread() do a printk on error even if not loaded with debug=1
 *
 *   Revision 1.22  2001/04/20 01:20:19  Jolt
 *   Final Merge :-)
 *
 *   Revision 1.21  2001/04/19 23:32:27  Jolt
 *   Merge Part II
 *
 *   Revision 1.20  2001/04/17 22:55:05  Jolt
 *   Merged framebuffer
 *
 *   Revision 1.19  2001/04/03 22:38:32  kwon
 *   make /proc/bus/gtx writable, just in case...
 *
 *   Revision 1.18  2001/03/21 15:30:25  tmbinc
 *   Added SYNC-delay for avia, resulting in faster zap-time.
 *
 *   Revision 1.17  2001/03/18 00:03:35  Hunz
 *   framebuffer fix
 *
 *   Revision 1.16  2001/03/04 13:02:25  tmbinc
 *   Added uCode interface for debugging.
 *
 *   Revision 1.15  2001/03/03 11:27:17  gillem
 *   - fix dprintk
 *
 *   Revision 1.14  2001/02/16 20:05:44  gillem
 *   - add new options ucode,debug
 *
 *   Revision 1.13  2001/02/11 15:53:25  tmbinc
 *   section filtering (not yet working)
 *
 *   Revision 1.12  2001/02/03 16:39:17  tmbinc
 *   sound fixes
 *
 *   Revision 1.11  2001/02/03 14:48:16  gillem
 *   - more audio fixes :-/
 *
 *   Revision 1.10  2001/02/03 11:30:10  gillem
 *   - fix audio
 *
 *   Revision 1.9  2001/01/31 17:17:46  tmbinc
 *   Cleaned up avia drivers. - tmb
 *
 *
 *   $Revision: 1.10 $
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
#include <linux/proc_fs.h>

#include "dbox/avia_gt.h"

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_PROC_FS

static int gtx_proc_init(void);
static int gtx_proc_cleanup(void);

static int read_bus_gtx  (char *buf, char **start, off_t offset, int len,
                          int *eof , void *private);
static int write_bus_gtx (struct file *file, const char *buffer,
                          unsigned long count, void *data);
static int read_bus_gtx_reg  (char *buf, char **start, off_t offset, int len,
                          int *eof , void *private);

static int gtx_proc_initialized;

#else /* undef CONFIG_PROC_FS */

#define gtx_proc_init() 0
#define gtx_proc_cleanup() 0

#endif /* CONFIG_PROC_FS */

static sAviaGtInfo *gt_info;

static int isr[] = {gISR0, gISR1, gISR2, gISR3};
static int imr[] = {gIMR0, gIMR1, gIMR2, gIMR3};

void avia_gt_gtx_clear_irq(unsigned char irq_reg, unsigned char irq_bit)
{

    gtx_reg_16n(isr[irq_reg]) |= (1 << irq_bit);
	
}

unsigned short avia_gt_gtx_get_irq_mask(unsigned char irq_reg)
{

    if (irq_reg <= 3)
	return gtx_reg_16n(imr[irq_reg]);
    else
	return 0;
	
}

unsigned short avia_gt_gtx_get_irq_status(unsigned char irq_reg)
{

    if (irq_reg <= 2) // FIXME mhh tmb??
	return gtx_reg_16n(isr[irq_reg]);
    else
	return 0;
	
}

void avia_gt_gtx_mask_irq(unsigned char irq_reg, unsigned char irq_bit)
{

    gtx_reg_16n(imr[irq_reg]) &= ~(1 << irq_bit);
	
}

void avia_gt_gtx_unmask_irq(unsigned char irq_reg, unsigned char irq_bit)
{

    gtx_reg_16n(imr[irq_reg]) |= (1 << irq_bit);
	
}

static void gtx_intialize_interrupts(void)
{

    gtx_reg_16(IPR0) = -1;
    gtx_reg_16(IPR1) = -1;
    gtx_reg_16(IPR2) = -1;
    gtx_reg_16(IPR3) = -1;

    gtx_reg_16(IMR0) = 0;
    gtx_reg_16(IMR1) = 0;
    gtx_reg_16(IMR2) = 0;
    gtx_reg_16(IMR3) = 0;

    gtx_reg_16(ISR0) = 0;
    gtx_reg_16(ISR1) = 0;
    gtx_reg_16(ISR2) = 0;
    gtx_reg_16(ISR3) = 0;
    gtx_reg_16(RR0) &= ~(1 << 4);

    gtx_reg_16(IMR0) = 0xFFFF;
    gtx_reg_16(IMR1) = 0xFFFF;

}

static void gtx_close_interrupts(void)
{

    gtx_reg_16(RR0) |= (1 << 4);

    gtx_reg_16(IMR0) = 0;
    gtx_reg_16(IMR1) = 0;
    gtx_reg_16(IMR2) = 0;
    gtx_reg_16(IMR3) = 0;

    gtx_reg_16(IPR0) = -1;
    gtx_reg_16(IPR1) = -1;
    gtx_reg_16(IPR2) = -1;
    gtx_reg_16(IPR3) = -1;

    gtx_reg_16(ISR0) = 0;
    gtx_reg_16(ISR1) = 0;
    gtx_reg_16(ISR2) = 0;
    gtx_reg_16(ISR3) = 0;

}

void avia_gt_gtx_reset(void) 
{

    gtx_reg_16(RR0) = 0xFFFF;
    gtx_reg_16(RR1) = 0x00FF;

}

void avia_gt_gtx_init(void)
{

    int cr;

    printk("avia_gt_gtx: $Id: avia_gt_gtx.c,v 1.10 2002/04/22 17:40:01 Jolt Exp $\n");
	
    avia_gt_gtx_reset();

    udelay (500);
    gtx_reg_16(RR0) &= ~( (1 << 13) | (1 << 12) | (1 << 10)
                      | (1 <<  9) | (1 << 6) | 1);   // DRAM, VIDEO, GRAPHICS

    udelay (500);

    memset (gt_info->mem_addr, 0xFF, 2 * 1024 * 1024);          // clear ram
	
    gtx_proc_init ();

    cr = gtx_reg_16(CR0);
    
    cr |= 1 << 11;           // enable graphics
    cr &= ~(1 << 10);        // enable sar
    cr |= 1 << 9;            // disable pcm
    cr &= ~(1 << 5);         // enable dac output
    cr |= 1 << 3;

    cr &= ~(3 << 6);
    cr |= 1 << 6;
    cr &= ~(1 << 2);

    gtx_reg_16(CR0) = cr;

    gtx_intialize_interrupts ();

    // workaround for framebuffer?
    atomic_set (&THIS_MODULE->uc.usecount, 1);

}

void avia_gt_gtx_exit(void)
{

    gtx_proc_cleanup ();

    gtx_close_interrupts ();

    gtx_reg_16(CR0) = 0x0030;
    gtx_reg_16(CR1) = 0x0000;

    // take modules in reset state
    gtx_reg_16(RR0) = 0xFBFF;
    gtx_reg_16(RR1) = 0x00FF;

    // disable dram module, don't work :-/ why ????
    //gtx_reg_16(RR0) |= (1<<10);

}

#ifdef CONFIG_PROC_FS
int gtx_proc_init(void)
{

    struct proc_dir_entry *proc_bus_gtx;
    struct proc_dir_entry *proc_bus_gtx_reg;

    gtx_proc_initialized = 0;

    if (!proc_bus) {
    
        printk("%s: %s: /proc/bus/ does not exist", __FILE__, __FUNCTION__);
	
        gtx_proc_cleanup ();
	
        return -ENOENT;
		
    }

    /* read and write allowed for anybody */
    proc_bus_gtx = create_proc_entry("gtx", S_IRUGO | S_IWUGO | S_IFREG, proc_bus);

    if (!proc_bus_gtx) {
    
        printk("%s: %s: Could not create /proc/bus/gtx", __FILE__, __FUNCTION__);
	
        gtx_proc_cleanup ();
	
        return -ENOENT;
	
    }

    proc_bus_gtx->read_proc = &read_bus_gtx;
    proc_bus_gtx->write_proc = &write_bus_gtx;
    proc_bus_gtx->owner = THIS_MODULE;
    gtx_proc_initialized += 2;

    /* read allowed for anybody */
    proc_bus_gtx_reg = create_proc_entry("gtx-reg", S_IRUGO | S_IFREG, proc_bus);

    if (!proc_bus_gtx_reg) {
    
        printk("%s: %s: Could not create /proc/bus/gtx-reg", __FILE__, __FUNCTION__);
	
        gtx_proc_cleanup ();
		
        return -ENOENT;
	
    }

    proc_bus_gtx_reg->read_proc = &read_bus_gtx_reg;
    proc_bus_gtx_reg->owner = THIS_MODULE;
    gtx_proc_initialized += 2;

    return 0;
    
}

/****************************************************************************
 * The /proc functions
 ****************************************************************************/
int read_bus_gtx(char *buf, char **start, off_t offset, int len, int *eof, void *private)
{

    if (offset < 0)
        return -EINVAL;
	
    if (len < 0)
        return -EINVAL;
    
    if (offset > 2048)
        return -EINVAL;
	
    if (offset + len > 2048)
	len = 2048 - offset;
	
    memcpy(buf, gt_info->reg_addr + 0x1000 + offset, len);

    return len;
	
}

int write_bus_gtx(struct file *file, const char *buffer, unsigned long count, void *data)
{

    return -EPERM;
    
}

int read_bus_gtx_reg(char *buf, char **start, off_t offset, int len, int *eof, void *private)
{

    s32 hi,lo;
    int nr = 0;

        nr += sprintf(buf+nr,"GTX-Control-Register:\n");
        nr += sprintf(buf+nr,"RR0:  %04X\n",rh(RR0)&0xFFFF);
        nr += sprintf(buf+nr,"RR1:  %04X\n",rh(RR1)&0xFFFF);
        nr += sprintf(buf+nr,"CR0:  %04X\n",rh(CR0)&0xFFFF);
        nr += sprintf(buf+nr,"CR1:  %04X\n",rh(CR1)&0xFFFF);
        nr += sprintf(buf+nr,"COCR: %04X\n",rh(C0CR)&0xFFFF);
        nr += sprintf(buf+nr,"C1CR: %04X\n",rh(C1CR)&0xFFFF);

		hi =rh(STC2)<<16;
		hi|=rh(STC1);
		lo =rh(STC0)&0x81FF;

        nr += sprintf(buf+nr,"GTX-Clock-Register:\n");
        nr += sprintf(buf+nr,"STC:  %08X:%04X\n",hi,lo);

		hi =rh(LSTC2)<<16;
		hi|=rh(LSTC1);
		lo =rh(LSTC0)&0x81FF;

        nr += sprintf(buf+nr,"LSTC: %08X:%04X\n",hi,lo);

		hi =rh(PCR2)<<16;
		hi|=rh(PCR1);
		lo =rh(PCR0)&0x81FF;

        nr += sprintf(buf+nr,"PCR:  %08X:%04X\n",hi,lo);

		hi  = ((rh(TTSR)&0x8000)<<16);
		hi |= (rh(PTS1) << 15);
		hi |= (rh(PTS0) >> 1);
		lo  = ((rh(PTS0)&1)<<15);

        nr += sprintf(buf+nr,"PTS:  %08X:%04X\n",hi,lo);

        nr += sprintf(buf+nr,"GTX-Teletext-Register:\n");
        nr += sprintf(buf+nr,"PTS0: %04X\n",rh(PTS0)&0xFFFF);
        nr += sprintf(buf+nr,"PTS1: %04X\n",rh(PTS1)&0xFFFF);
        nr += sprintf(buf+nr,"PTSO: %04X\n",rh(PTSO)&0xFFFF);
        nr += sprintf(buf+nr,"TTCR: %04X\n",rh(TTCR)&0xFFFF);
        nr += sprintf(buf+nr,"TTSR: %04X\n",rh(TTSR)&0xFFFF);

        nr += sprintf(buf+nr,"GTX-Queue-Register:\n");
        nr += sprintf(buf+nr,"TQP(R/W): %04X:%04X %04X:%04X\n",rh(TQRPH)&0xFFFF,rh(TQRPL)&0xFFFF,rh(TQWPH)&0xFFFF,rh(TQWPL)&0xFFFF);
        nr += sprintf(buf+nr,"AQP(R/W): %04X:%04X %04X:%04X\n",rh(AQRPH)&0xFFFF,rh(AQRPL)&0xFFFF,rh(AQWPH)&0xFFFF,rh(AQWPL)&0xFFFF);
        nr += sprintf(buf+nr,"VQP(R/W): %04X:%04X %04X:%04X\n",rh(VQRPH)&0xFFFF,rh(VQRPL)&0xFFFF,rh(VQWPH)&0xFFFF,rh(VQWPL)&0xFFFF);
	
    return nr;
	
}

int gtx_proc_cleanup(void)
{

    if (gtx_proc_initialized >= 1) {
    
        remove_proc_entry ("gtx", proc_bus);
        gtx_proc_initialized -= 2;
        remove_proc_entry ("gtx-reg", proc_bus);
        gtx_proc_initialized -= 2;
	
    }

    return 0;
    
}
#endif /* CONFIG_PROC_FS */
