/*
 *   gtp-enx.c - AViA eNX core driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2001 Florian Schirmer (jolt@tuxbox.org)
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
 *   $Log: gtp-enx.c,v $
 *   Revision 1.1  2001/04/19 23:33:13  Jolt
 *   Merge Part II
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
 *   $Revision: 1.1 $
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
#include <linux/malloc.h>
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
#include "gtp-core.h"



unsigned char *enx_mem_addr = NULL;
unsigned char *enx_reg_addr = NULL;


#ifdef MODULE
MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("Avia eNX driver");

static int debug = 0;
static char *ucode = 0;

MODULE_PARM(debug, "i");
MODULE_PARM(ucode, "s");

#endif

#define dprintk(fmt,args...) if(debug) printk( fmt,## args)

static Pcr_t enx_read_transport_pcr(void);
static Pcr_t enx_read_latched_clk(void);
static Pcr_t enx_read_current_clk(void);
static s32 enx_calc_diff(Pcr_t clk1, Pcr_t clk2);
static s32 enx_bound_delta(s32 bound, s32 delta);

static void (*enx_irq[64])(int reg, int bit);
static int isr[6]={0x0100,0x0102,0x0104,0x0106,0x0108,0x010A};
static int imr[6]={0x0110,0x0112,0x0114,0x0116,0x0118,0x011A};


void enx_set_pid_table(int entry,int wait_pusi,int invalid,int pid)
{
   enx_reg_h(0x2700+entry*2)=((!!wait_pusi)<<15)|((!!invalid)<<14)|pid;
}

void enx_set_pid_control_table(int entry, int type,int queue,int fork,int cw_offset,int cc,int start_up,int pec)
{
   u8 w[4];
   w[0]=type<<5;
//   if ((enx_reg_h(0x27FE)&0xFF00)>0xA0000)
     w[0]|=(queue)&31;
//   else
//     w[0]|=(queue+1)&31;
   w[1]=(!!fork)<<7;
   w[1]|=cw_offset<<4;
   w[1]|=cc;
   w[2]=(!!start_up)<<6;
   w[2]|=(!!pec)<<5;
   w[3]=0;
   enx_reg_w(0x2740+entry*4)=*(u32*)w;
   printk("W:%x\n",*(u32*)w);
}


static Pcr_t enx_read_transport_pcr(void)
{
    Pcr_t pcr;
    pcr.hi =enx_reg_h(TP_PCR_2)<<16;
    pcr.hi|=enx_reg_h(TP_PCR_1);
    pcr.lo =enx_reg_h(TP_PCR_0)&0x81FF;
    return pcr;
}

static Pcr_t enx_read_latched_clk(void)
{
    Pcr_t pcr;
    pcr.hi =enx_reg_h(LC_STC_2)<<16;
    pcr.hi|=enx_reg_h(LC_STC_1);
    pcr.lo =enx_reg_h(LC_STC_0)&0x81FF;
    return pcr;
}

static Pcr_t enx_read_current_clk(void)
{
    Pcr_t pcr;
    pcr.hi =enx_reg_h(STC_COUNTER_2)<<16;
    pcr.hi|=enx_reg_h(STC_COUNTER_1);
    pcr.lo =enx_reg_h(STC_COUNTER_0)&0x81FF;
    return pcr;
}

static s32 enx_calc_diff(Pcr_t clk1, Pcr_t clk2)
{
    s32 delta;
    delta=(s32)clk1.hi;
    delta-=(s32)clk2.hi;
    delta*=(s32)2;
    delta+=(s32)((clk1.lo>>15)&1);
    delta-=(s32)((clk2.lo>>15)&1);
    delta*=(s32)300;
    delta+=(s32)(clk1.lo & 0x1FF);
    delta-=(s32)(clk2.lo & 0x1FF);
    return delta;
}

static s32 enx_bound_delta(s32 bound, s32 delta)
{
    if (delta>bound)
	delta=bound;
    if (delta<-bound)
	delta=-bound;
    return delta;
}

static void enx_set_pcr(Pcr_t pcr)
{
    enx_reg_h(STC_COUNTER_2)=pcr.hi>>16;
    enx_reg_h(STC_COUNTER_1)=pcr.hi&0xFFFF;
    enx_reg_h(STC_COUNTER_0)=pcr.lo&0x81FF;
}


static int discont=1, large_delta_count, deltaClk_max, deltaClk_min,deltaPCR_AVERAGE;
static Pcr_t oldClk;
extern void avia_set_pcr(u32 hi, u32 lo);

static void enx_pcr_interrupt(int b, int r)
{
  Pcr_t TPpcr;
  Pcr_t latchedClk;
  Pcr_t currentClk;
  s32 delta_PCR_AV;

  s32 deltaClk, elapsedTime;

  TPpcr=enx_read_transport_pcr();
  latchedClk=enx_read_latched_clk();
  currentClk=enx_read_current_clk();

  if (discont)
  {
    oldClk=currentClk;
    discont=0;
    large_delta_count=0;
    printk("gtx_dmx: we have a discont:\n");
    printk("gtx_dmx: new stc: %08x%08x\n", TPpcr.hi, TPpcr.lo);
    deltaPCR_AVERAGE=0;
    enx_reg_h(FC) |= 1 << 8;               // force discontinuity
    enx_set_pcr(TPpcr);
    avia_set_pcr(TPpcr.hi, TPpcr.lo);
    return;
  }

  elapsedTime=latchedClk.hi-oldClk.hi;
  if (elapsedTime > TIME_HI_THRESHOLD)
  {
    printk("gtx_dmx: elapsed time disc.\n");
    goto WE_HAVE_DISCONTINUITY;
  }
  deltaClk=TPpcr.hi-latchedClk.hi;
  if ((deltaClk < -MAX_HI_DELTA) || (deltaClk > MAX_HI_DELTA))
  {
    printk("gtx_dmx: large_delta disc.\n");
    if (large_delta_count++>MAX_DELTA_COUNT)
      goto WE_HAVE_DISCONTINUITY;
    return;
  } else
    large_delta_count=0;

  deltaClk=enx_calc_diff(TPpcr, latchedClk);
  deltaPCR_AVERAGE*=99;
  deltaPCR_AVERAGE+=deltaClk*10;
  deltaPCR_AVERAGE/=100;

  if ((deltaPCR_AVERAGE < -0x20000) || (deltaPCR_AVERAGE > 0x20000))
  {
    printk("gtx_dmx: tmb pcr\n");
    goto WE_HAVE_DISCONTINUITY;
  }

  delta_PCR_AV=deltaClk-deltaPCR_AVERAGE/10;
  if (delta_PCR_AV > deltaClk_max)
    deltaClk_max=delta_PCR_AV;
  if (delta_PCR_AV < deltaClk_min)
    deltaClk_min=delta_PCR_AV;

  elapsedTime=enx_calc_diff(latchedClk, oldClk);
  if (elapsedTime > TIME_THRESHOLD)
    goto WE_HAVE_DISCONTINUITY;

//  printk("elapsed %08x, delta %c%08x\n", elapsedTime, deltaClk<0?'-':'+', deltaClk<0?-deltaClk:deltaClk);
//  printk("%x (%x)\n", deltaClk, enx_bound_delta(MAX_DAC, deltaClk));

/*  deltaClk=gtx_bound_delta(MAX_DAC, deltaClk);

  rw(DPCR)=((-deltaClk)<<16)|0x0009; */

  deltaClk=-enx_bound_delta(MAX_DAC, deltaClk*32);

  enx_reg_h(DAC_PC)=deltaClk;
  enx_reg_h(DAC_CP)=9;
  

  oldClk=latchedClk;
  return;
WE_HAVE_DISCONTINUITY:
  printk("gtx_dmx: WE_HAVE_DISCONTINUITY\n");
  discont=1;
}

int enx_allocate_irq(int reg, int bit, void (*isr)(int, int))
{
    int nr=reg*16+bit;
    dprintk("enx_core: allocate_irq reg %d bit %d\n", reg, bit);
    if (enx_irq[nr])
    {

      printk("enx_core: irq already used\n");
      return -1;
    }
    enx_irq[nr]=isr;
    enx_reg_h(IMR1) = (1<<bit)|1;
//    enx_reg_h(IMR1) = 1<<bit | 1<<0;
//    enx_reg_h(IMR1) = 0x21;
  dprintk("enx_core: IMR0: 0x%x\n", enx_reg_h(IMR0));
  dprintk("enx_core: IMR1: 0x%x\n", enx_reg_h(IMR1));
  dprintk("enx_core: IMR2: 0x%x\n", enx_reg_h(IMR2));
  dprintk("enx_core: IMR3: 0x%x\n", enx_reg_h(IMR3));
    return 0;

}

void enx_free_irq(int reg, int bit)
{
  dprintk("enx_core: free_irq reg %d bit %d\n", reg, bit);
    enx_reg_w(imr[reg]) = 1<<bit;
    enx_irq[reg*16+bit]=0;
}

static void enx_dmx_set_pcr_source(int pid)
{
  enx_reg_h(PCR_PID)=(1<<13)|pid;
//  enx_reg_h(PCR_PID)=0x20FF;
  enx_reg_h(FC) |= 1 << 8;               // force discontinuity
  discont=1;
  enx_free_irq(1, 5);
  enx_allocate_irq(1, 5, enx_pcr_interrupt);       // pcr reception
}


unsigned char *enx_get_mem_addr(void) {

  return enx_mem_addr;

}

unsigned char *enx_get_reg_addr(void) {

  return enx_reg_addr;

}

void enx_dac_init(void) {

  enx_reg_w(RSTR0) &= ~(1 << 20);	// Get dac out of reset state
  enx_reg_h(DAC_PC) = 0x0000;
  enx_reg_h(DAC_CP) = 0x0009;

}

static void enx_irq_handler(int irq, void *dev, struct pt_regs *regs)
{
  int i, j;

  dprintk("enx-core: IRQ\n");
  dprintk("enx_core: ISR0: 0x%x\n", enx_reg_h(ISR0));
  dprintk("enx_core: ISR1: 0x%x\n", enx_reg_h(ISR1));
  dprintk("enx_core: ISR2: 0x%x\n", enx_reg_h(ISR2));
  dprintk("enx_core: ISR3: 0x%x\n", enx_reg_h(ISR3));

  for (i=0; i<3; i++)
  {
    int rn=enx_reg_h(isr[i]);
    for (j=0; j<16; j++)
    {
      if (rn&(1<<j))
      {
        int nr=i*16+j;
        if (enx_irq[nr])
          enx_irq[nr](i, j);
        else
        {
          if (enx_reg_h(imr[i])&(1<<j))
          {
            printk("enx_core: masking unhandled gtx irq %d/%d\n", i, j);
            enx_reg_h(imr[i]) = 1<<j;                 // disable IMR bit
          }
        }
//	printk("enx_core: clear status register:%x , %x \n",isr[i],j);
        enx_reg_h(isr[i]) = 1<<j;            // clear ISR bits
      }
    }
  }
}

void enx_set_queue(int queue, u32 wp, u8 size)
{
    if (queue>=16)
      enx_reg_w(CFGR0)|= 0x10;
    else
      enx_reg_w(CFGR0)&=~0x10;
    queue &= 0xF;

    enx_reg_h(QWPnL+4*queue)=wp&0xFFFF;
    enx_reg_h(QWPnH+4*queue)=((wp>>16)&63)|(size<<6);
}

void enx_set_queue_pointer(int queue, u32 read, u32 write, int size, int halt)
{
    int base=queue*8+0x08E0;

    enx_reg_h(base)=read&0xFFFF;
    enx_reg_h(base+4)=write&0xFFFF;
    enx_reg_h(base+6)=((write>>16)&63)|(size<<6);
    enx_reg_h(base+2)=((read>>16)&63);
}

void enx_set_queue_rptr(int queue, u32 read)
{
    int base=queue*8+0x1E0;
    enx_reg_h(base)=read&0xFFFF;
    enx_reg_h(base+2)=((read>>16)&63)|(enx_reg_h(base+2)&(1<<15));
}    

u32 enx_get_queue_wptr(int queue)
{
    u32 wp=-1, oldwp;
    do
    {
      oldwp=wp;
      if (queue>=16)
         enx_reg_w(CFGR0)|= 0x10;
	else
	 enx_reg_w(CFGR0)&=~0x10;
      queue &=0xF;
      wp=enx_reg_h(QWPnL+4*queue);
      wp|=(enx_reg_h(QWPnH+4*queue)&63)<<16;
    } while (wp!=oldwp);
    return wp;
}  	 
      

void enx_reset_queue(int queue)
{
    int rqueue;
    switch (queue)
    {
    case 0:
	    rqueue=2;
	    break;
    case 1:
	    rqueue=0;
	    break;
    case 2:
	    rqueue=1;
	    break;
    default:
	    return;
    }
    enx_set_queue_rptr(rqueue,enx_get_queue_wptr(queue));
}


int enx_irq_enable(void) {

  enx_reg_h(ISR0) = 0xFFFE;		// Clear all irq states
  enx_reg_h(ISR1) = 0xFFFE;		// Clear all irq states
  enx_reg_h(ISR2) = 0xFFFE;		// Clear all irq states
  enx_reg_h(ISR3) = 0xFFFE;		// Clear all irq states
  enx_reg_h(ISR4) = 0xFFFE;		// Clear all irq states
  enx_reg_h(ISR5) = 0xFFFE;		// Clear all irq states

  // Ale IRQs maskieren
  enx_reg_h(IMR0) = 0xFFFE;
  enx_reg_h(IMR1) = 0xFFFE;
  enx_reg_h(IMR2) = 0xFFFE;
  enx_reg_h(IMR3) = 0xFFFE;
  enx_reg_h(IMR4) = 0xFFFE;
  enx_reg_h(IMR5) = 0xFFFE;

  if (request_8xxirq(ENX_INTERRUPT, enx_irq_handler, 0, "enx", 0) != 0) {

    printk(KERN_ERR "enx-core: Could not allocate IRQ!\n");

    return -1;

  }

  // IRQs an Hostprozessor weiterreichen
  enx_reg_w(EHIDR)=0;
  // Und den entsprechenden Pin (HIRQ0) waehlen
  // Man sollte das evtl. doch per Hex-Wert machen :-)
  enx_reg_w(IPR4)=(1<<2)|(1<<4)|(1<<6)|(1<<8)|(1<<10)|(1<<12)|(1<<14)|(1<<16)|(1<<18)|(1<<20)|(1<<22)|(1<<24)|(1<<26);

/*
  enx_reg_h(ISR0) = 0x0000;		// Clear all irq states
  enx_reg_h(ISR1) = 0x0000;		// Clear all irq states
  enx_reg_h(ISR2) = 0x0000;		// Clear all irq states
  enx_reg_h(ISR3) = 0x0000;		// Clear all irq states
  enx_reg_h(ISR4) = 0x0000;		// Clear all irq states
  enx_reg_h(ISR5) = 0x0000;		// Clear all irq states

  enx_reg_h(IMR0) = 0x00FF;		// mask all IRQ's
  enx_reg_h(IMR1) = 0xFF79;
  enx_reg_h(IMR2) = 0xFCFF;
  enx_reg_h(IMR3) = 0xFFFF;
  enx_reg_h(IMR4) = 0xFFFF;
  enx_reg_h(IMR5) = 0x80DF;


  enx_reg_w(EHIDR)=0;
  enx_reg_w(IPR4)=(1<<2);
*/
  enx_reg_w(IDR)=0;
  return 0;

}

void enx_irq_disable(void) {

  enx_reg_h(IMR0) = 0xFFFE;		// Mask all IRQ's
  enx_reg_h(IMR1) = 0xFFFE;		// Mask all IRQ's
  enx_reg_h(IMR2) = 0xFFFE;		// Mask all IRQ's
  enx_reg_h(IMR3) = 0xFFFE;		// Mask all IRQ's
  enx_reg_h(IMR4) = 0xFFFE;		// Mask all IRQ's
  enx_reg_h(IMR5) = 0xFFFE;		// Mask all IRQ's
/*
  enx_reg_h(IMR0) = 0x0000;		// Mask all IRQ's
  enx_reg_h(IMR1) = 0x0000;		// Mask all IRQ's
  enx_reg_h(IMR2) = 0x0000;		// Mask all IRQ's
  enx_reg_h(IMR3) = 0x0000;		// Mask all IRQ's
  enx_reg_h(IMR4) = 0x0000;		// Mask all IRQ's
  enx_reg_h(IMR5) = 0x0000;		// Mask all IRQ's
*/
  free_irq(ENX_INTERRUPT, 0);

}

void enx_reset(void) {

  enx_reg_w(RSTR0) = 0xFCF6BEFF;	// Reset all modules

}

void enx_sdram_ctrl_init(void) {

  enx_reg_w(SCSC) = 0x00000000;		// Set sd-ram start address
  enx_reg_w(RSTR0) &= ~(1 << 12);	// Get sd-ram controller out of reset state
  enx_reg_w(MC) = 0x00001015;		// Write memory configuration

}

void enx_tdp_start(void) {

  enx_reg_w(RSTR0) &= ~(1 << 22);	// Clear TDP reset bit
  enx_reg_w(EC) = 0x00;			// Start TDP

}

void enx_tdp_stop(void) {

  enx_reg_w(EC) = 0x02;			// Stop TDP

}


void enx_tdp_init(u8 *microcode)
//void enx_tdp_init(void)
{

  unsigned short *instr_ram = (unsigned short*)enx_reg_o(TDP_INSTR_RAM);
  unsigned short *src = (unsigned short*)microcode;
//  unsigned short *src = (unsigned short*)riscram;
  int     words=0x800/2;

  printk("INSTR-RAM:%x\n",*instr_ram);


  while (words--) {

    udelay(100);

    *instr_ram++ = *src++;

  }

}

/* ---------------------------------------------------------------------- */

static int errno;

static int do_firmread(const char *fn, char **fp)
{

  /* shameless stolen from sound_firmware.c */

  int fd;
  long l;
  char *dp;

  fd = open(fn, 0, 0);

  if (fd == -1)	{

    printk("Unable to load '%s'.\n", ucode);

    return 0;

  }

  l = lseek(fd, 0L, 2);

  if (l<=0) {

    printk("Firmware wrong size '%s'.\n", ucode);
    sys_close(fd);

    return 0;

  }

  lseek(fd, 0L, 0);

  dp = vmalloc(l);

  if (dp == NULL) {

    printk("Out of memory loading '%s'.\n", ucode);
    sys_close(fd);

    return 0;

  }

  if (read(fd, dp, l) != l) {

    printk("Failed to read '%s'. (%d)\n", ucode, errno);
    vfree(dp);
    sys_close(fd);

    return 0;

  }

  close(fd);

  *fp = dp;

  return (int)l;

}

/* ---------------------------------------------------------------------- */

#define VCR_SET_HP(X)    enx_reg_h(VCR) = ((enx_reg_h(VCR)&(~(3<<10))) | ((X&3)<<10))
#define VCR_SET_FP(X)    enx_reg_h(VCR) = ((enx_reg_h(VCR)&(~(3<<8 ))) | ((X&3)<<8 ))
#define GVP_SET_SPP(X)   enx_reg_w(GVP1) = ((enx_reg_w(GVP1)&(~(0x01F<<27))) | ((X&0x1F)<<27))
#define GVP_SET_X(X)     enx_reg_w(GVP1) = ((enx_reg_w(GVP1)&(~(0x3FF<<16))) | ((X&0x3FF)<<16))
#define GVP_SET_Y(X)     enx_reg_w(GVP1) = ((enx_reg_w(GVP1)&(~0x3FF))|(X&0x3FF))
#define GVP_SET_COORD(X,Y) GVP_SET_X(X); GVP_SET_Y(Y)

#define GVS_SET_XSZ(X)   enx_reg_w(GVSZ1) = ((enx_reg_w(GVSZ1)&(~(0x3FF<<16))) | ((X&0x3FF)<<16))
#define GVS_SET_YSZ(X)   enx_reg_w(GVSZ1) = ((enx_reg_w(GVSZ1)&(~0x3FF))|(X&0x3FF))

void gtp_fb_set_param(unsigned int pal, unsigned int bpp, unsigned int lowres, unsigned int interlaced, unsigned int xres, unsigned int yres, unsigned int stride) {

  int val;
  
	enx_reg_w(VBR)=0;
	
  enx_reg_h(VCR)=0x040|(1<<13);
  enx_reg_h(BALP)=0x7F7F;
  enx_reg_h(VHT)=(pal?857:851)|0x5000;
  enx_reg_h(VLT)=pal?(623|(21<<11)):(523|(18<<11));
  enx_reg_h(VAS)=pal?63:58;

	val=0;
	if (lowres)
		val|=1<<31;

	if (!pal)
		val|=1<<30;                         // NTSC square filter. TODO: do we need this?

	if (!interlaced)
		val|=1<<29;

	val|=1<<28;

  val|=1<<26;                           // chroma filter. evtl. average oder decimate, bei text
  		// TCR noch setzen!
  switch (bpp)
  {
	case 4:
		val|=2<<20; break;
	case 8:
		val|=6<<20; break;
	case 16:
		val|=3<<20; break;
	case 32:
		val|=7<<20; break;
  }
	
  val|=stride;

	enx_reg_h(P1VPSA)=0;
	enx_reg_h(P2VPSA)=0;
	enx_reg_h(P1VPSO)=0;
	enx_reg_h(P2VPSO)=0;
	enx_reg_h(VMCR)=0;
	enx_reg_h(G1CFR)=1;
	enx_reg_h(G2CFR)=1;
		
	enx_reg_w(GMR1)=val;
	enx_reg_w(GMR2)=0;
	printk("GMR1: %08x\n", val);
  enx_reg_h(GBLEV1)=0x0000;
  enx_reg_h(GBLEV2)=0;
//JOLT  enx_reg_h(CCR)=0x7FFF;                  // white cursor
	enx_reg_w(GVSA1)= ENX_FB_OFFSET; 	// dram start address
	enx_reg_h(GVP1)=0;

  GVP_SET_COORD(70,43);                 // TODO: NTSC?
  for (val=0; val<576; val++)
  {
  	int x;
	  for (x=0; x<720; x++)
	  {
	  	((__u16*)enx_mem_addr)[val*720+x]=0x7FFF;
	  }
  }

                                        // DEBUG: TODO: das ist nen kleiner hack hier.
/*  if (lowres)
    GVS_SET_XSZ(xres);
  else
    GVS_SET_XSZ(xres*2);*/

  GVS_SET_XSZ(720);
  GVS_SET_YSZ(576);

/*  if (interlaced)
    GVS_SET_YSZ(yres);
  else
    GVS_SET_YSZ(yres*2);*/
    
}

void *gtp_fb_mem_lin(void) {

  return (void*)(enx_get_mem_addr() + ENX_FB_OFFSET);

}

void *gtp_fb_mem_phys(void) {

  return (void*)(ENX_MEM_BASE + ENX_FB_OFFSET);

}

void gtp_fb_getcolreg(unsigned int regno, unsigned int *red, unsigned int *green, unsigned int *blue, unsigned int *transp) {

  unsigned int val;

  enx_reg_h(CLUTA)=regno;
  mb();
  val=enx_reg_h(CLUTD);

  if (transp)    
    *transp = ((val & 0xFF000000) >> 24);

  if (red)
    *red = ((val & 0x00FF0000) >> 16);
    
  if (green)    
    *green = ((val & 0x0000FF00) >> 8);
    
  if (blue)    
    *blue = (val & 0x000000FF);
    
}

void gtp_fb_setcolreg(unsigned int regno, unsigned int red, unsigned int green, unsigned int blue, unsigned int transp) {

  enx_reg_h(CLUTA) = regno;
  mb();
  enx_reg_w(CLUTD) = ((transp << 24) | (red << 16) | (green << 8) | (blue));

}

sGtpDev gtp_dev_enx;
int gtp_dev_nr;

static int enx_init(void) {

  int bla3;
  u8 *microcode;
  mm_segment_t fs;
  u8 w[4];
  int type = 1;
  int queue = 0;
  int fork = 0;
  int cw_offset = 0;
  int cc = 0;
  int start_up = 1;
  int pec = 0;
  int wait_pusi = 0;
  int pid = 0xFF;
  int invalid=0;
  int i;
  int x;
//  u8 size;
  int val;


  printk(KERN_NOTICE "enx-core: Loading AViA eNX core driver\n");

  fs = get_fs();

  set_fs(get_ds());

  /* read firmware */
  if (do_firmread(ucode, (char**)&microcode) == 0) {

    set_fs(fs);

    return -EIO;

  }


  set_fs(fs);

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


  printk(KERN_NOTICE "enx-core: MEM: 0x%X->0x%X REG: 0x%X->0x%X\n", ENX_MEM_BASE, (unsigned int)enx_mem_addr, ENX_REG_BASE, (unsigned int)enx_reg_addr);
  printk(KERN_NOTICE "enx-core: ARCH_ID: 0x%X API_VERSION: 0x%X\n", enx_reg_s(CRR)->F_ARCH_ID, enx_reg_s(CRR)->F_API_VERSION);
  printk(KERN_NOTICE "enx-core: VERSION: 0x%X REVISION: 0x%X\n", enx_reg_s(CRR)->F_VERSION, enx_reg_s(CRR)->F_REVISION);
  printk(KERN_NOTICE "enx-core: BRD_ID: 0x%X BIU_SEL: 0x%X\n", enx_reg_s(CFGR0)->F_BRD_ID, enx_reg_s(CFGR0)->F_BIU_SEL);
  printk(KERN_NOTICE "enx-core: Loaded AViA eNX core driver\n");

  printk("enx_reset..\n");
  enx_reset();
  printk("enx_sdram_ctrl_init.\n");
  enx_sdram_ctrl_init();
  printk("enx_dac_init\n");
  enx_dac_init();
  printk("tdp_init\n");
  enx_tdp_init(microcode);
  printk("bla.\n");
	
	printk("vfree.\n");
	vfree(microcode);
	
	printk("tdp start.\n");

  enx_tdp_start();

  memset(enx_irq, 0, sizeof(enx_irq));

  if (enx_irq_enable()) {

    iounmap(enx_mem_addr);
    iounmap(enx_reg_addr);

    return -1;

  }
  
  memset(enx_mem_addr, 0xF, ENX_MEM_SIZE);

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

//#ifdef 0
  enx_reg_w(CFGR0) &= ~(1 << 3);   // disable clip mode teletext
  enx_reg_w(CFGR0) &= ~(1 << 1);   // disable clip mode audio
  enx_reg_w(CFGR0) &= ~(1 << 0);   // disable clip mode video
  

   //TDP-Testing - up to now in the module
   enx_reg_w(RSTR0) &= ~(1 << 23);  //take Paketframer out of reset <-??? see docu
		//some settings see docu -> channel interface
   enx_reg_w(RSTR0) &= ~(1 << 31);  //take TS-framer out of reset

   enx_reg_h(FC) = 0x9147;          //start paketframer
   enx_reg_h(SYNC_HYST) = 0xE2;
   enx_reg_h(BQ) = 0x00BC;
   
   enx_reg_w(CFGR0) |= 1 << 24;    //enable dac output
//   enx_dmx_set_pcr_source(0x00FF);    //set pcr source
   
   enx_reg_h(AVI_0) = 0x6CF;          
   enx_reg_h(AVI_1) = 0xA;
   

   
// Framebuffer
   enx_reg_h(VCR)=0x40;
//   enx_reg_h(VCR)|=1<<7;
//   enx_reg_h(VCR)|=1<<6;
//   enx_reg_h(VCR)|=1<<14;
  
   enx_reg_h(VCR)|=1<<13;
//   enx_reg_h(VCR)|=3<<8;
   
   enx_reg_h(VHT)=857|3<<10|0x5000;
   enx_reg_h(VLT)=623|21<<11;
   
//   enx_reg_w(VBR)=(1<<24)|0x808080;

   enx_reg_h(BALP)=0x0;
   enx_reg_h(VMCR)=0x0;
   enx_reg_h(G1CFR)=0x1;
   enx_reg_h(G2CFR)=0x1;
   
   
   enx_reg_h(VAS)=63<<1;

   udelay(1000*1000);

/*   enx_reg_w(GMR1)=0x043005A0;
//   enx_reg_w(GMR1)=0x0;
   enx_reg_h(GBLEV1)=0x0;
   enx_reg_h(GBLEV2)=0x7F7F;
   enx_reg_w(GVSA1)=0x0;
   enx_reg_h(GVP1)=0x0;
   enx_reg_w(GVP1)=((enx_reg_w(GVP1)&(~(0x3FF<<16))) | ((70&0x1F)<<27));
   enx_reg_w(GVP1)=((enx_reg_w(GVP1)&(~0x3FF))|(43&0x3FF));
   enx_reg_w(GVSZ1)=((enx_reg_w(GVSZ1)&(~(0x3FF<<16))) | ((720&0x3FF)<<16));
   enx_reg_w(GVSZ1)=((enx_reg_w(GVSZ1)&(~0x3FF))|(576&0x3FF));
*/
   //Sections
   
   enx_reg_h(SPPCR1)=0;
   enx_reg_h(SPPCR2)=0;
   enx_reg_h(SPPCR3)=0;
   enx_reg_h(SPPCR4)=0;
   
   //PID settings
   for (i=0;i<32;i++){
      enx_set_pid_table(i,0,1,0);
//      enx_set_pid_control_table(0,0,0,0,0,0,0,0);
      enx_set_queue(i,0x20000*i,11);
   }

   for (i=0;i<32;i++){
      enx_set_queue(i,0x10000*i,10);
   }
/*   
   enx_set_queue_pointer(2,0x00000,0x00000,10,0);   //Video
   enx_set_queue_pointer(0,0x10000,0x10000,10,0);   //Audio
   enx_set_queue_pointer(1,0x20000,0x20000,10,0);   //Teletext

   enx_reset_queue(0);
   enx_reset_queue(1);
   enx_reset_queue(2);

   enx_set_pid_table(0,0,1,0x00FF);             //(filter,pusi,invalid,pid)
   enx_set_pid_table(1,0,1,0x100);             //(filter,pusi,invalid,pid)
   
   enx_set_pid_control_table(0,3,0,0,0,0,0,0); 
   enx_set_pid_control_table(1,3,1,0,0,0,0,0); 

   enx_set_pid_table(0,0,0,0x00FF);             //(filter,pusi,invalid,pid)
   enx_set_pid_table(1,0,0,0x100);             //(filter,pusi,invalid,pid)
*/
//   enx_set_pid_control_table(0,3,0,0,0,0,1,0); 
//   enx_set_pid_control_table(1,3,1,0,0,0,1,0); 

    
/*
   avia_wait(avia_command(Reset));   
   avia_wait(avia_command(SetStreamType,0xB));
   avia_command(SelectStream,0,0xFF);
   avia_command(Play,0,0,0);
*/

/*   for(i=0x1FF;i<0xFFF;i++){
     enx_dmx_set_pcr_source(i);    //set pcr source
     printk("PID:%x\n",i);
     udelay(100*1000);
   }
 */
   
  gtp_dev_enx.name = "AViA eNX GTP driver";
  gtp_dev_enx.fb_mem_lin = enx_get_mem_addr() + ENX_FB_OFFSET;
  gtp_dev_enx.fb_mem_phys = (void*)(ENX_MEM_BASE + ENX_FB_OFFSET);
  gtp_dev_enx.fb_clut_get = gtp_fb_getcolreg;
  gtp_dev_enx.fb_clut_set = gtp_fb_setcolreg;
  gtp_dev_enx.fb_param_set = gtp_fb_set_param;
  
  gtp_dev_nr = gtp_dev_register(&gtp_dev_enx);

  return 0;
  
}

static void enx_close(void) {

  if (enx_reg_addr) {
  
    gtp_dev_release(gtp_dev_nr);
  
    enx_irq_disable();
    enx_tdp_stop();
  
    iounmap(enx_mem_addr);
    iounmap(enx_reg_addr);
    
    enx_reg_addr = NULL;
    enx_mem_addr = NULL;
    
  }
  
}



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
