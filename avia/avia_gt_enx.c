/*
 *   enx-core.c - AViA eNX core driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2001 Florian "Jolt" Schirmer (jolt@tuxbox.org)
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
 *   $Log: avia_gt_enx.c,v $
 *   Revision 1.4  2002/04/10 21:59:59  Jolt
 *   Cleanups
 *
 *   Revision 1.3  2001/12/01 06:37:06  gillem
 *   - malloc.h -> slab.h
 *
 *   Revision 1.2  2001/10/23 08:40:58  Jolt
 *   eNX capture and pig driver
 *
 *   Revision 1.1  2001/10/15 20:47:46  tmbinc
 *   re-added enx-core
 *
 *   Revision 1.24  2001/09/19 18:47:00  TripleDES
 *   small init fix
 *
 *   Revision 1.23  2001/09/17 21:21:40  TripleDES
 *   some small changes
 *
 *   Revision 1.22  2001/09/02 01:01:23  TripleDES
 *   -some fixes
 *
 *   Revision 1.21  2001/08/18 18:21:54  TripleDES
 *   moved the ucode-loading to dmx
 *
 *   Revision 1.20  2001/07/17 14:38:17  tmbinc
 *   sdram fixes, but problems still not solved
 *
 *   Revision 1.19  2001/05/15 22:42:03  kwon
 *   make do_firmread() do a printk on error even if not loaded with debug=1
 *
 *   Revision 1.17  2001/04/21 10:40:13  tmbinc
 *   fixes for eNX
 *
 *   Revision 1.16  2001/04/20 01:20:19  Jolt
 *   Final Merge :-)
 *
 *   Revision 1.15  2001/04/17 22:55:05  Jolt
 *   Merged framebuffer
 *
 *   Revision 1.14  2001/04/09 23:26:42  TripleDES
 *   some changes
 *
 *   Revision 1.12  2001/03/29 03:58:24  tmbinc
 *   chaned enx_reg_w to enx_reg_h and enx_reg_d to enx_reg_w.
 *   Improved framebuffer.
 *
 *   Revision 1.11  2001/03/29 02:26:22  tmbinc
 *   fixed defines and CRLFs
 *
 *   Revision 1.10  2001/03/29 02:23:19  fnbrd
 *   IRQ angepasst, load_ram aktiviert.
 *
 *   Revision 1.9  2001/03/29 01:28:23  TripleDES
 *   Some demux testing...still not working
 *
 *   Revision 1.5  2001/03/03 12:00:35  Jolt
 *   Firmware loader
 *
 *   Revision 1.4  2001/03/03 00:47:46  Jolt
 *   Firmware loader
 *
 *   Revision 1.3  2001/03/03 00:11:34  Jolt
 *   Version cleanup
 *
 *   Revision 1.2  2001/03/03 00:09:15  Jolt
 *   Typo fix
 *
 *   Revision 1.1  2001/03/02 23:56:34  gillem
 *   - initial release
 *
 *   $Revision: 1.4 $
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

#include "dbox/enx.h"
#include "dbox/enx-dmx.h"
#include "dbox/avia.h"

unsigned char *enx_mem_addr = NULL;
unsigned char *enx_reg_addr = NULL;

/* ---------------------------------------------------------------------- */

#ifdef MODULE
MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("Avia eNX driver");

static int debug = 0;
static char *ucode = 0;

MODULE_PARM(debug, "i");
MODULE_PARM(ucode, "s");
#endif

#define dprintk(fmt,args...) if(debug) printk( fmt,## args)

static void (*enx_irq[128])(int reg, int bit);
static int isr[6]={0x0100,0x0102,0x0104,0x0106,0x0108,0x010A};
static int imr[6]={0x0110,0x0112,0x0114,0x0116,0x0118,0x011A};

int enx_allocate_irq(int reg, int bit, void (*isro)(int, int))
{

	int nr = reg * 16 + bit;
	
	dprintk("enx_core: allocate_irq reg %d bit %d\n", reg, bit);
	
	if (enx_irq[nr]) {
	
		printk("enx_core: irq already used\n");
		
		return -EBUSY;
		
	}
	
	enx_irq[nr] = isro;
	enx_reg_16n(imr[reg]) = (1 << bit) | 1;
	enx_reg_16n(isr[reg]) = (1 << bit);		// clear isr status
	
	return 0;
	
}

void enx_free_irq(int reg, int bit)
{
	dprintk("enx_core: free_irq reg %d bit %d\n", reg, bit);
	enx_reg_16n(imr[reg])=1<<bit;
	enx_irq[reg*16+bit]=0;
}

unsigned char *enx_get_mem_addr(void)
{
	return enx_mem_addr;
}

unsigned char *enx_get_reg_addr(void)
{
	return enx_reg_addr;
}

void enx_dac_init(void)
{
	enx_reg_w(RSTR0) &= ~(1 << 20);	// Get dac out of reset state
	enx_reg_h(DAC_PC) = 0x0000;
	enx_reg_h(DAC_CP) = 0x0009;
}

static void enx_irq_handler(int irq, void *dev, struct pt_regs *regs)
{

  int i, j;
  
  for (i=0; i<6; i++) {
  
    int rn=enx_reg_16n(isr[i]);
    
    for (j=1; j<16; j++) {
    
      if (rn&(1<<j)) {
      
        int nr=i*16+j;
//				printk("enx interrupt %d:%d\n", i, j);
        if (enx_irq[nr]) {
	
	    enx_irq[nr](i, j);
	    
        } else {
	
          if (enx_reg_16n(imr[i]) & (1 << j)) {
	  
            printk("enx_core: masking unhandled enx irq %d/%d\n", i, j);
            enx_reg_16n(imr[i]) = 1 << j;                 // disable IMR bit
	    
          }
	  
        }
	
//	printk("enx_core: clear status register:%x , %x \n",isr[i],j);
        enx_reg_16n(isr[i]) = 1 << j;            // clear ISR bits
	
      }
    }
  }
}

int enx_irq_enable(void)
{
  enx_reg_w(EHIDR) = 0;					// IRQs an Hostprozessor weiterreichen
  enx_reg_w(IPR4) = 0x55555555; // alles auf HIRQ0
  enx_reg_w(IPR5) = 0x55555555; // das auch noch

  enx_reg_h(ISR0) = 0xFFFE;		// Clear all irq states
  enx_reg_h(ISR1) = 0xFFFE;		// Clear all irq states
  enx_reg_h(ISR2) = 0xFFFE;		// Clear all irq states
  enx_reg_h(ISR3) = 0xFFFE;		// Clear all irq states
  enx_reg_h(ISR4) = 0xFFFE;		// Clear all irq states
  enx_reg_h(ISR5) = 0xFFFE;		// Clear all irq states

  enx_reg_h(IMR0) = 0x0001;		// mask all IRQ's (=disable them)
  enx_reg_h(IMR1) = 0x0001;
  enx_reg_h(IMR2) = 0x0001;
  enx_reg_h(IMR3) = 0x0001;
  enx_reg_h(IMR4) = 0x0001;
  enx_reg_h(IMR5) = 0x0001;
  enx_reg_w(IDR) = 0;

  if (request_8xxirq(ENX_INTERRUPT, enx_irq_handler, 0, "enx", 0) != 0) {
    printk(KERN_ERR "enx-core: Could not allocate IRQ!\n");

    return -1;
  }

  return 0;

}

void enx_irq_disable(void) {

  enx_reg_h(IMR0) = 0xFFFE;		// Mask all IRQ's
  enx_reg_h(IMR1) = 0xFFFE;		// Mask all IRQ's
  enx_reg_h(IMR2) = 0xFFFE;		// Mask all IRQ's
  enx_reg_h(IMR3) = 0xFFFE;		// Mask all IRQ's
  enx_reg_h(IMR4) = 0xFFFE;		// Mask all IRQ's
  enx_reg_h(IMR5) = 0xFFFE;		// Mask all IRQ's

  enx_reg_h(IMR0) = 0x0001;		// Mask all IRQ's
  enx_reg_h(IMR1) = 0x0001;		// Mask all IRQ's
  enx_reg_h(IMR2) = 0x0001;		// Mask all IRQ's
  enx_reg_h(IMR3) = 0x0001;		// Mask all IRQ's
  enx_reg_h(IMR4) = 0x0001;		// Mask all IRQ's
  enx_reg_h(IMR5) = 0x0001;		// Mask all IRQ's

  free_irq(ENX_INTERRUPT, 0);
  
}

void enx_reset(void) {

  enx_reg_w(RSTR0) = 0xFCF6BEFF;	// Reset all modules
  
}

void enx_sdram_ctrl_init(void) {

  enx_reg_w(SCSC) = 0x00000000;		// Set sd-ram start address
  enx_reg_w(RSTR0) &= ~(1 << 12);	// Get sd-ram controller out of reset state
  enx_reg_w(MC) = 0x00001011;		// Write memory configuration
  enx_reg_32n(0x88) |= 0x3E << 4;
  
}

void enx_tdp_start(void) 
{

  enx_reg_w(RSTR0) &= ~(1 << 22);	// Clear TDP reset bit
  enx_reg_w(EC) = 0x00;			// Start TDP
  
}

void enx_tdp_stop(void) 
{

  enx_reg_w(EC) = 0x02;			// Stop TDP
  
}


void enx_tdp_init(u8 *microcode)
{

  unsigned short *instr_ram = (unsigned short*)enx_reg_o(TDP_INSTR_RAM);
  unsigned short *src = (unsigned short*)microcode;
  int     words=0x800/2;

  while (words--) {
  
    udelay(100);
    *instr_ram++ = *src++;
    
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

        l = lseek (fd, 0L, 2);
        if (l <= 0) {
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
static int enx_init(void)
{
  enx_reg_addr = (unsigned char*)ioremap(ENX_REG_BASE, ENX_REG_SIZE);

  if (!enx_reg_addr) {
    printk(KERN_ERR "enx-core: Failed to remap memory.\n");
    return -1;
  }

  enx_mem_addr = (unsigned char*)ioremap(ENX_MEM_BASE, ENX_MEM_SIZE);

  if (!enx_mem_addr) {
    iounmap(enx_reg_addr);
    printk(KERN_ERR "enx-core: Failed to remap memory.\n");
    return -1;
  }


  //printk(KERN_NOTICE "enx-core: MEM: 0x%X->0x%X REG: 0x%X->0x%X\n", ENX_MEM_BASE, (unsigned int)enx_mem_addr, ENX_REG_BASE, (unsigned int)enx_reg_addr);
  //printk(KERN_NOTICE "enx-core: ARCH_ID: 0x%X API_VERSION: 0x%X\n", enx_reg_s(CRR)->ARCH_ID, enx_reg_s(CRR)->API_VERSION);
  //printk(KERN_NOTICE "enx-core: VERSION: 0x%X REVISION: 0x%X\n", enx_reg_s(CRR)->VERSION, enx_reg_s(CRR)->REVISION);
  //printk(KERN_NOTICE "enx-core: BRD_ID: 0x%X BIU_SEL: 0x%X\n", enx_reg_s(CFGR0)->BRD_ID, enx_reg_s(CFGR0)->BIU_SEL);
  printk(KERN_NOTICE "enx-core: Loaded AViA eNX core driver\n");

  enx_reset();
  enx_sdram_ctrl_init();
  enx_dac_init();

  memset(enx_irq, 0, sizeof(enx_irq));

  if (enx_irq_enable()) {
  
    iounmap(enx_mem_addr);
    iounmap(enx_reg_addr);

    return -1;
    
  }
  
  memset(enx_mem_addr, 0xF, 1024*1024 /*ENX_MEM_SIZE*/);

  //bring out of reset state
  enx_reg_w(RSTR0) &= ~(1 << 28);  // PCM Audio Clock
  enx_reg_w(RSTR0) &= ~(1 << 27);  // AV - Decoder
  enx_reg_w(RSTR0) &= ~(1 << 21);  // Teletext engine
  enx_reg_w(RSTR0) &= ~(1 << 17);  // PIG2
  enx_reg_w(RSTR0) &= ~(1 << 13);  // Queue Manager
  enx_reg_w(RSTR0) &= ~(1 << 11);  // Graphics
  enx_reg_w(RSTR0) &= ~(1 << 10);  // Video Capture
  enx_reg_w(RSTR0) &= ~(1 << 9);   // Video Module
  enx_reg_w(RSTR0) &= ~(1 << 7);   // PIG1
  enx_reg_w(RSTR0) &= ~(1 << 6);   // Blitter / Color expander

  enx_reg_w(CFGR0) &= ~(1 << 3);   // disable clip mode teletext
  enx_reg_w(CFGR0) &= ~(1 << 1);   // disable clip mode audio
  enx_reg_w(CFGR0) &= ~(1 << 0);   // disable clip mode video

		// initialize sound
	enx_reg_w(PCMN) = 0x80808080;
	enx_reg_h(PCMC) = 0;
	enx_reg_h(PCMC) |= (3<<14);
	enx_reg_h(PCMC) |= (1<<13);
	enx_reg_h(PCMC) |= (1<<12);
	enx_reg_h(PCMC) |= (1<<11);
	enx_reg_h(PCMC) &= ~(0<<6);   // 0: use external (avia) clock
	enx_reg_h(PCMC) |= 0;
	enx_reg_h(PCMC) |= 2<<2;
	enx_reg_h(PCMC) |= 2;

  return 0;
}

static void enx_close(void)
{
	if (enx_reg_addr)
	{
		enx_irq_disable();
		enx_tdp_stop();
		iounmap(enx_mem_addr);
		iounmap(enx_reg_addr);
		enx_reg_addr = NULL;
		enx_mem_addr = NULL;
	}
}

/* ---------------------------------------------------------------------- */

EXPORT_SYMBOL(enx_get_mem_addr);
EXPORT_SYMBOL(enx_get_reg_addr);
EXPORT_SYMBOL(enx_allocate_irq);
EXPORT_SYMBOL(enx_free_irq);


#ifdef MODULE

int init_module(void) {

  return enx_init();

}

void cleanup_module(void) {

  enx_close();

}

#endif

#if 0
	enx_reg_w(RSTR0)|=/*(1<<31)|*/(1<<23)|(1<<22);
		
	enx_reg_w(RSTR0)&=~(1<<23);
	enx_reg_h(FC)=0x9147;
	enx_reg_h(SYNC_HYST)=0x21;
	enx_reg_h(BQ)=0x00BC;
	//enx_reg_w(RSTR0)&=~(1<<31);


	enx_reg_w(RSTR0)&=~(1<<22);
	enx_reg_w(EC)=0;
#endif

