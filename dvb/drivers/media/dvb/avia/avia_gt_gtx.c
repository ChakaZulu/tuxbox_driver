/*
 *   gtx-core.c - AViA GTX core driver (dbox-II-project)
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
 *   $Revision: 1.3 $
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

#include "dbox/gtx.h"

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

#ifdef MODULE
static char *ucode = NULL;
#endif

static int debug = 0;
#define dprintk if (debug) printk

/* interrupt stuff */
#define GTX_INTERRUPT SIU_IRQ1

unsigned char *gtxmem, *gtxreg;
static int rambeg, ramsize;

/* ---------------------------------------------------------------------- */

extern void gtxfb_init    (void);
extern void gtxfb_close   (void);
extern void gtx_dmx_init  (void);
extern void gtx_dmx_close (void);

static void gtx_interrupt (int irq, void *dev, struct pt_regs * regs);
static int  gtx_intialize_interrupts (void);
static int  gtx_close_interrupts (void);
static void gtx_close (void);

/* ---------------------------------------------------------------------- */
unsigned char
*gtx_get_mem (void)
{
        return gtxmem;
}

unsigned char
*gtx_get_reg (void)
{
        return gtxreg;
}

/* ---------------------------------------------------------------------- */
void LoaduCode (u8 * microcode)
{
        unsigned short *dst   = (unsigned short*) &rh (RISC);
        unsigned short *src   = (unsigned short*) microcode;
        int             words = 0x800 / 2;

        rh (RR1) |= 1 << 5;         // reset RISC
        udelay (10);
        rh(RR1) &= ~(1 << 5);

        while (words--) {
                udelay (100);
                *dst++ = *src++;
        }

        dst   = (unsigned short*) &rh (RISC);
        src   = (unsigned short*) microcode;
        words = 0x800 / 2;
        while (words--)
                if (*dst++ != *src++)
                        break;

        if (words >= 0) {
                printk (KERN_CRIT "microcode validation failed at %x\n",
                        0x800 - words);
                return;
        }
}

/* ---------------------------------------------------------------------- */
/* shamelessly stolen from sound_firmware.c */
static int errno;

static int
do_firmread (const char *fn, char **fp)
{
        int    fd;
        loff_t l;
        char  *dp;

        if ((fd = open (fn, 0, 0)) < 0) {
                printk (KERN_ERR "%s: %s: Unable to load '%s'.\n",
                        __FILE__, __FUNCTION__, fn);
                return 0;
        }


        if ((l = lseek (fd, 0L, 2)) != 2 * 1024) {
                printk (KERN_ERR "%s: %s: Firmware wrong size '%s'.\n",
                        __FILE__, __FUNCTION__, fn);
                sys_close (fd);
                return 0;
        }

        lseek (fd, 0L, 0);
        dp = vmalloc (l);
        if (dp == NULL) {
                printk (KERN_ERR "%s: %s: Out of memory loading '%s'.\n",
                        __FILE__, __FUNCTION__, fn);
                sys_close (fd);
                return 0;
        }

        if (read (fd, dp, l) != l) {
                printk (KERN_ERR "%s: %s: Failed to read '%s'.\n",
                        __FILE__, __FUNCTION__, fn);
                vfree (dp);
                sys_close (fd);
                return 0;
        }

        close (fd);
        *fp = dp;
        return (int) l;
}

/* ---------------------------------------------------------------------- */
static int
init_gtx (void)
{
        int cr;
        //u8 *microcode;
        //mm_segment_t fs;

        dprintk (KERN_INFO "gtx-core: loading AViA GTX core driver\n");

        //fs = get_fs ();
        //set_fs(get_ds());

        /* read firmware */
        //if (do_firmread (ucode, (char**) &microcode) == 0) {
        //        set_fs (fs);
        //        return -EIO;
        //}

        //set_fs(fs);

        /* remap gtx memory */
        gtxmem = (unsigned char*) ioremap (GTX_PHYSBASE, 0x403000);

        if (!gtxmem) {
                printk (KERN_ERR "%s: %s: failed to remap gtx.\n",
                        __FILE__, __FUNCTION__);
                return -ENOMEM;
        }


        gtxreg = gtxmem + 0x400000;

        rh(RR0) = 0xFFFF;
        rh(RR1) = 0x00FF;
        udelay (500);
        rh(RR0) &= ~( (1 << 13) | (1 << 12) | (1 << 10)
                      | (1 <<  9) | (1 << 6) | 1);   // DRAM, VIDEO, GRAPHICS

        udelay (500);

        //LoaduCode (microcode);
        //vfree(microcode);

        memset (gtxmem, 0xFF, 2 * 1024 * 1024);          // clear ram
        gtx_proc_init ();

        cr = rh (CR0);
        printk (KERN_INFO "%s: %s: gtxID %.2x\n", __FILE__, __FUNCTION__,
                (cr & 0xF000) >> 12);

        cr |= 1 << 11;           // enable graphics
        cr &= ~(1 << 10);        // enable sar
        cr |= 1 << 9;            // disable pcm
        cr &= ~(1 << 5);         // enable dac output
        cr |= 1 << 3;

        cr &= ~(3 << 6);
        cr |= 1 << 6;
        cr &= ~(1 << 2);

        ramsize  = 2 * 1024 * 1024;
        rambeg   = 0;
        rh (CR0) = cr;

        gtx_intialize_interrupts ();
#ifndef MODULE
        gtx_dmx_init ();
        gtxfb_init();
#endif
        // workaround for framebuffer?
        atomic_set (&THIS_MODULE->uc.usecount, 1);
        printk (KERN_NOTICE "%s: %s: loaded AViA GTX core driver\n",
                __FILE__, __FUNCTION__);

        /* buffer disable */
        rw (PCMA) = 1;
        /* set volume for pcm and mpeg */
        rw (PCMN) = 0x80808080;
        rh (PCMC) = 0;
        /* enable PCM frequ. same MPEG */
        rh (PCMC) |= (3 << 14);
        /* 16 bit mode */
        rh (PCMC) |= (1 << 13);
        /* stereo */
        rh (PCMC) |= (1 << 12);
        /* signed samples */
        rh (PCMC) |= (1 << 11);
        /* clock from aclk */
        rh (PCMC) &= ~(0 << 6);   // 0: use external (avia) clock
        /* set adv */
        rh (PCMC) |= 0;
        /* set acd */
        rh (PCMC) |= 2 << 2;
        /* set bcd */
        rh (PCMC) |= 2;

	// enable teletext
	rh(TTCR) |= (1 << 9);

        return 0;
}

/* ---------------------------------------------------------------------- */
int
gtx_allocate_dram (int size, int align)
{
        int newbeg = rambeg;

        newbeg += align - 1;
        newbeg &= ~(align - 1);
        dprintk (KERN_DEBUG "%s: %s: wasting %d bytes.\n", __FILE__,
                 __FUNCTION__, newbeg-rambeg);
        if ((newbeg+size) > ramsize) {
                printk (KERN_ERR "%s: %s: GTX out of memory.\n", __FILE__,
                        __FUNCTION__);
                return -ENOMEM;
        }
        rambeg = newbeg + size;

        return newbeg;
}

/* ---------------------------------------------------------------------- */
static void (*gtx_irq[64]) (int reg, int bit);
static int isr[4] = { gISR0, gISR1, gISR2, gISR3 };
static int imr[4] = { gIMR0, gIMR1, gIMR2, gIMR3 };

/* ---------------------------------------------------------------------- */
static void
gtx_interrupt (int irq, void *dev, struct pt_regs * regs)
{
        int i, j;
        for (i = 0; i < 3; i++) {            // bis das geklärt ist! danke.
                int rn = rhn (isr[i]);

                for (j = 0; j < 16; j++) {
                        if (rn & (1 << j)) {
                                int nr = i * 16 + j;

                                if (gtx_irq[nr])
                                        gtx_irq[nr] (i, j);
                                else {
                                        if (rhn(imr[i]) & (1 << j)) {
                                                dprintk (KERN_DEBUG "%s: %s: "
                                                         "masking unhandled "
                                                         "gtx irq %d/%d\n",
                                                         __FILE__,
                                                         __FUNCTION__, i, j);
                                                // disable IMR bit
                                                rhn (imr[i]) &= ~(1 << j);
                                        }
                                }
                                rhn(isr[i]) |= 1 << j;  // clear ISR bits
                        }
                }
        }
}

/* ---------------------------------------------------------------------- */
int
gtx_allocate_irq (int reg, int bit, void (*isr) (int, int))
{
        int nr = reg * 16 + bit;
        if (gtx_irq[nr]) {
                // nur für debugzwecke, aber da ist das praktischer
                panic (KERN_ERR "%s: %s: FATAL: gtx irq already used.\n",
                       __FILE__, __FUNCTION__);
                return -ENODEV;
        }

        gtx_irq[nr] = isr;
        rhn (imr[reg]) |= 1 << bit;

        return 0;
}

/* ---------------------------------------------------------------------- */
void
gtx_free_irq (int reg, int bit)
{
        rhn (imr[reg]) &= ~(1 << bit);
        gtx_irq[reg * 16 + bit] = 0;
}

/* ---------------------------------------------------------------------- */
static int
gtx_intialize_interrupts (void)
{
        rh (IPR0) = -1;
        rh (IPR1) = -1;
        rh (IPR2) = -1;
        rh (IPR3) = -1;

        rh (IMR0) = 0;
        rh (IMR1) = 0;
        rh (IMR2) = 0;
        rh (IMR3) = 0;

        rh (ISR0) = 0;
        rh (ISR1) = 0;
        rh (ISR2) = 0;
        rh (ISR3) = 0;
        rh (RR0) &= ~(1 << 4);

        rh (IMR0) = 0xFFFF;
        rh (IMR1) = 0xFFFF;

        if (request_8xxirq (GTX_INTERRUPT, gtx_interrupt, 0, "gtx", 0) != 0) {
                printk (KERN_ERR "%s: %s: Could not allocate GTX IRQ!",
                        __FILE__, __FUNCTION__);
                return -ENODEV;
          }

        return 0;
}

/* ---------------------------------------------------------------------- */
static int
gtx_close_interrupts(void)
{
	rh (RR0) |= (1 << 4);

        rh (IMR0) = 0;
        rh (IMR1) = 0;
        rh (IMR2) = 0;
        rh (IMR3) = 0;

        rh (IPR0) = -1;
        rh (IPR1) = -1;
        rh (IPR2) = -1;
        rh (IPR3) = -1;

        rh (ISR0) = 0;
        rh (ISR1) = 0;
        rh (ISR2) = 0;
        rh (ISR3) = 0;

        free_irq (GTX_INTERRUPT, 0);

        return 0;
}

/* ---------------------------------------------------------------------- */
static void
gtx_close (void)
{
	gtx_proc_cleanup ();

#ifndef MODULE
        gtx_dmx_close ();
        gtxfb_close ();
#endif
        gtx_close_interrupts ();

	rh (CR0) = 0x0030;
	rh (CR1) = 0x0000;

	// take modules in reset state
        rh(RR0) = 0xFBFF;
        rh(RR1) = 0x00FF;

	// disable dram module, don't work :-/ why ????
//	rh(RR0) |= (1<<10);

	if (gtxmem)
	{
		iounmap(gtxmem);
		gtxmem = 0;
	}
}

#ifdef CONFIG_PROC_FS
int
gtx_proc_init (void)
{
        struct proc_dir_entry *proc_bus_gtx;
        struct proc_dir_entry *proc_bus_gtx_reg;

        gtx_proc_initialized = 0;

        if (!proc_bus) {
                printk("%s: %s: /proc/bus/ does not exist", __FILE__,
                       __FUNCTION__);
                gtx_proc_cleanup ();
                return -ENOENT;
        }

        /* read and write allowed for anybody */
        proc_bus_gtx = create_proc_entry("gtx", S_IRUGO | S_IWUGO |
                                                S_IFREG, proc_bus);

        if (!proc_bus_gtx) {
                printk("%s: %s: Could not create /proc/bus/gtx", __FILE__,
                       __FUNCTION__);
                gtx_proc_cleanup ();
                return -ENOENT;
        }

        proc_bus_gtx->read_proc   = &read_bus_gtx;
        proc_bus_gtx->write_proc  = &write_bus_gtx;
        proc_bus_gtx->owner       = THIS_MODULE;
        gtx_proc_initialized     += 2;

        /* read allowed for anybody */
        proc_bus_gtx_reg = create_proc_entry("gtx-reg", S_IRUGO |
                                                S_IFREG, proc_bus);

        if (!proc_bus_gtx_reg) {
                printk("%s: %s: Could not create /proc/bus/gtx-reg", __FILE__,
                       __FUNCTION__);
                gtx_proc_cleanup ();
                return -ENOENT;
        }

        proc_bus_gtx_reg->read_proc   = &read_bus_gtx_reg;
        proc_bus_gtx_reg->owner       = THIS_MODULE;
        gtx_proc_initialized     += 2;

        return 0;
}

/****************************************************************************
 * The /proc functions
 ****************************************************************************/
int
read_bus_gtx (char *buf, char **start, off_t offset, int len, int *eof,
              void *private)
{
        if (offset < 0)
                return -EINVAL;
        if (len < 0)
                return -EINVAL;
        if (offset > 2048)
                return -EINVAL;
        if (offset + len > 2048)
                len = 2048 - offset;
        memcpy (buf, gtxreg + 0x1000 + offset, len);

        return len;
}

int
write_bus_gtx (struct file *file, const char *buffer, unsigned long count,
               void *data)
{
	return -EPERM;
}

int
read_bus_gtx_reg (char *buf, char **start, off_t offset, int len, int *eof,
              void *private)
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

int
gtx_proc_cleanup (void)
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



/* ---------------------------------------------------------------------- */
#ifdef MODULE

int
init_module (void)
{
        return init_gtx ();
}

void
cleanup_module (void)
{
        return gtx_close ();
}

MODULE_PARM(debug,"i");
MODULE_PARM(ucode,"s");
#endif

EXPORT_SYMBOL(gtx_get_mem);
EXPORT_SYMBOL(gtx_get_reg);
EXPORT_SYMBOL(gtx_allocate_dram);
EXPORT_SYMBOL(gtx_allocate_irq);
EXPORT_SYMBOL(gtx_free_irq);

MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("Avia GTX driver");

