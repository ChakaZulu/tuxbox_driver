/*
 *   pcm.c - pcm driver for gtx (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2001 Gillem htoa@gmx.net
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
 *  Supported devices:
 *  /dev/dsp    standard /dev/dsp device, (mostly) OSS compatible
 *  /dev/mixer  standard /dev/mixer device, (mostly) OSS compatible
 *
 *   $Log: pcm.c,v $
 *   Revision 1.14  2001/12/01 06:53:35  gillem
 *   - malloc.h -> slab.h
 *
 *   Revision 1.13  2001/11/26 23:07:30  Terminar
 *   implemented more ioctls - pcm Termina'ted' ;)
 *
 *   Revision 1.12  2001/03/10 22:27:59  gillem
 *   - remove avia settings
 *
 *   Revision 1.11  2001/02/03 19:44:38  tmbinc
 *   Fixed double-buffering.
 *
 *   Revision 1.10  2001/02/02 23:45:33  tmbinc
 *   Modified for strictly halfword-aligned accesses to gtxmem. -tmb
 *
 *   Revision 1.9  2001/02/02 22:08:36  tmbinc
 *   Fixed Buffer, removed DAC-Setting (as it's for the PCR), optimized swab.
 *
 *   still some strange sounds, but mpg123 now works (with -w /dev/dsp).
 *
 *    - tmb
 *
 *   Revision 1.8  2001/02/01 20:02:56  gillem
 *   - tests
 *
 *   Revision 1.7  2001/01/07 20:52:31  gillem
 *   add volume to mixer ioctl
 *
 *   Revision 1.6  2001/01/06 10:24:06  gillem
 *   add some mixer stuff (not work now)
 *
 *   Revision 1.5  2001/01/06 10:07:10  gillem
 *   cvs check
 *
 *   Revision 1.4  2001/01/06 09:55:09  gillem
 *   cvs check
 *
 *
 *   $Revision: 1.14 $
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
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

#include <linux/sound.h>
#include <linux/soundcard.h>
#include "pcm.h"
#include "gtx.h"
#include "avia.h"

#define wDR(a, d) avia_wr(TM_DRAM, a, d)
#define rDR(a) avia_rd(TM_DRAM, a)

#define PCM_INTR_REG        1
#define PCM_PF_INTR_BIT       10
#define PCM_AD_INTR_BIT       12

//just debug stuff
//#define PCM_DEBUG 1
//#define PCM_FORCE_11025 1
//#define PCM_FORCE_MONO 1
//#define PCM_FORCE_8BIT 1

static wait_queue_head_t pcm_wait;

#ifdef MODULE
MODULE_AUTHOR("Gillem <htoa@gmx.net>");
MODULE_DESCRIPTION("GTX-PCM Driver");
#endif


struct pcm_state s;
struct pcm_data pcm;

static unsigned char *gtxmem, *gtxreg;
static int buffer_start, buffer_size, buffer_end, buffer_wptr, buffer_rptr, buffer_playptr;
static int underrun=0;


int pcm_setfmt(int fmt)
{
    printk("setformat %d\n",fmt);

    if (fmt != AFMT_QUERY)
    {
    
        switch(fmt)
        {

            case AFMT_U8: //8 on pcm_write_bits
                printk("set unsigned 8bit\n");
                rh(PCMC) &= ~(1<<11);       //unsigned
                rh(PCMC) &= ~(1<<13);       //8bit
                pcm.sign = 0;
                pcm.bits = 8;
                pcm.fmt = AFMT_U8;
                break;

            default:    
                printk("set default for %d\n",fmt);
#ifndef PCM_FORCE_8BIT
            case AFMT_S16_LE: //16 bit signed little endian, 16 on pcm_write_bits
                printk("set signed 16bit little endian\n");
                rh(PCMC) |= (1<<11);       //signed
                rh(PCMC) |= (1<<13);       //16bit
                pcm.sign = 1;
                pcm.bits = 16;
                pcm.fmt = AFMT_S16_LE;
                break;
#endif
        }
    }
    else
    {
        printk("got afmt_query\n");
    }

    return(pcm.fmt);    
}


void printbin(unsigned long num, int size)
{
	int i;

	for (i = size - 1; i >= 0; i--) {
            if (  ((i+1)  % 4 ) == 0)
            {
                printk(" %u",((1<<i) & num) ? 1 : 0);
            } else
            {
                printk("%u",((1<<i) & num) ? 1 : 0);
            }
	}
        
        printk("\n");
}

void pcm_dump_reg()
{
  printk("pcm dump reg\n");
  printk("PCMA: %08lX\n",rw(PCMA));
  printbin(rw(PCMA),32);
  printk("PCMN: %08lX\n",rw(PCMN));
  printbin(rw(PCMN),32);
  printk("PCMC: %04X\n",rh(PCMC));
  printbin(rh(PCMC),32);
  printk("PCMD: %08lX\n",rw(PCMD));
  printbin(rw(PCMD),32);
}

void avia_dump_reg()
{
  printk("avia dump reg\n");
  printk("0XE0: %08X\n",rDR(0xE0));
  printbin(rDR(0xe0),32);
  printk("0XE8: %08X\n",rDR(0xE8));
  printbin(rDR(0xe8),32);
  printk("0XEC: %08X\n",rDR(0xEC));
  printbin(rDR(0xec),32);
  printk("0XF4: %08X\n",rDR(0xF4));	
  printbin(rDR(0xf4),32);
}

/* reset pcm register on gtx */
void pcm_reset()
{
  int cr;

#ifdef PCM_DEBUG
  printk("pcm-reset-start\n");
  avia_dump_reg();
  pcm_dump_reg();
#endif

  /* enable pcm on gtx */
  cr=rh(CR0);
  cr&=~(1<<9);
  rh(CR0)=cr;

  /* reset aclk  and pcm*/
//  rh(RR0) |= 0x1200;
//  rh(RR0) &= ~0x1200;
  
  /* buffer disable */
  rw(PCMA) |= 1;

  /* set volume for pcm and mpeg */
  rw(PCMN) = 0x40404040;

  rh(PCMC) |= 0;

  /* enable PCM frequ. same MPEG */
  //rh(PCMC) |= (3<<14);
  rh(PCMC) &= ~(3<<14);
  rh(PCMC) |= (1<<14);
  pcm.rate = 11025;

  /* 16 bit mode */
  rh(PCMC) &= ~(1<<13);
  pcm.bits = 8;

  /* stereo */
    rh(PCMC) &= ~(1<<12);
    pcm.channels = 1;

  /* unsigned samples */  
  rh(PCMC) &= ~(1<<11);
  pcm.sign = 0;

//TODO: setfmt
    pcm_setfmt(AFMT_U8);
    
  
  /* !!! ACLK NOT WORK !!! */

  /* clock from aclk */
//  rh(PCMC) &= ~(0<<6);  // 0: use external (avia) clock <-- bug!
    rh(PCMC) |= (1<<6);

  /* set adv */
//  rh(PCMC) |= 0;	<---BUG!?
//    rh(PCMC) &= ~(2<<4); //(a/4, 1x)
    rh(PCMC) |= (2<<4); //(a/4, 1x) <-------------- important? !!! ?

  /* set acd */
  rh(PCMC) |= 2<<2;             // 256 ACLKs per LRCLK

  /* set bcd */
  rh(PCMC) |= 2;

#ifdef PCM_DEBUG
  avia_dump_reg();  
  pcm_dump_reg();
#endif
  
}

void avia_audio_init()
{

  u32 val;

  val = 0;

  // AUDIO_CLOCK_SELECTION
/*
  val |= (1<<4);   //44100
  val |= (1<<3);   //44100
  val |= (1<<2);
  val |= (1<<1);   // 1:256 0:384 x sampling frequ.
  val |= (1);      // master,slave mode

  wDR(0xEC, val);  
*/

/*

  // AUDIO_CONFIG 12,11,7,6,5,4 reserved or must be set to 0
  val |= (0<<10);  // 64 DAI BCKs per DAI LRCK
  val |= (0<<9);  // input is I2S
  val |= (0<<8);  // output constan low (no clock)
  val |= (1<<3);  // 0: normal 1:I2S output
  val |= (1<<2);  // 0:off 1:on channels
  val |= (1<<1);  // 0:off 1:on IEC-958
  val |= (1);      // 0:encoded 1:decoded output
  
  wDR(0xE0, val);

  val = 0;

  // AUDIO_DAC_MODE 0 reserved
  val |= (0<<8);
  val |= (0<<6);
  val |= (0<<4);
  val |= (0<<3);  // 0:high 1:low DA-LRCK polarity
  val |= (0<<2);  // 0:0 as MSB in 24 bit mode 1: sign ext. in 24bit
  val |= (0<<1);  // 0:msb 1:lsb first
  wDR(0xE8, val);

  wDR(0x1B0, 0);                // DEBUG: 0: disabled av_sync 6: enable

  val = 0;

  // AUDIO_CLOCK_SELECTION
  val |= (1<<2);
  val |= (1<<1);   // 1:256 0:384 x sampling frequ.
  val |= (1);      // master,slave mode

  wDR(0xEC, val);

  val = 0;

  // AUDIO_ATTENUATION
  wDR(0xF4, 0);

  // wait for avia to ready
  val = 0xFF;
  wDR(0x468,val);
  while(rDR(0x468));
  printk("XXXX: %08X\n",rDR(0x468));
*/

  buffer_start=0x50000;
  buffer_end=  0x60000;
  buffer_size= 0x10000; //64k
  /* anm.:
  16bit stereo 4 byte/sample
  44100 bits * 4 (2 x 16bit, 2 x stereo)
  */
  
  buffer_playptr=buffer_wptr=buffer_rptr=buffer_start;
  memset(gtxmem+buffer_start, 0, buffer_size); // ...
}  



static int pcm_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
  int val;
  
  switch(cmd) {
    case OSS_GETVERSION:                                                                                  
            return put_user(SOUND_VERSION, (int *)arg);                                                   


    case SNDCTL_DSP_RESET:
                      printk("RESET\n");
                      pcm_reset();
                      return 0;
                      
    case SNDCTL_DSP_SPEED:
                      if (get_user(val,(int*)arg))
                        return -EFAULT;

                      /* clear flags, disable pcm, set to 00 */
			rh(PCMC) &= ~(3<<14);

                      /* set speed */
		      /* hm, the specifications of the kernel-sound stuff...
		         a kernel mod should set values near the supported speed
			 even if something different is to be set... right? */

#ifndef PCM_FORCE_11025
			//nearest to 11025
		        if (val <= 15537)
		        {
		    	    rh(PCMC) |= (1<<14);
			    val = pcm.rate = 11025;
			}
			//nearest to 22050
		        else if ((val > 15538) && (val <= 33075))
		        {
		    	    rh(PCMC) |= (2<<14);
			    val = pcm.rate = 22050;
			}
			//nearest to 44100
		        else if (val > 33076)
		        {
		    	    rh(PCMC) |= (3<<14);
			    val = pcm.rate = 44100;
			}
#else
		    	    rh(PCMC) |= (1<<14);
			    val = pcm.rate = 11025;
                
#endif

                        printk("SET_SPEED: %d\n",val);
			
                      return put_user(val,(int *) arg);

    case SNDCTL_DSP_STEREO:
                      if (get_user(val,(int*)arg))
                        return -EFAULT;

                    //stereo
#ifndef PCM_FORCE_MONO
		      if (val == 1)
		      {
		        rh(PCMC) |= (1<<12);
			val = 1;
                        pcm.channels = 2;
		      } else
#endif
		      {
		        rh(PCMC) &= ~(1<<12);
			val = 0;
                        pcm.channels = 1;
		      }

                      printk("STEREO: %d\n",val);
                      return put_user(val,(int*)arg);

    case SNDCTL_DSP_CHANNELS:
		      if (get_user(val,(int*)arg))
		    	return -EFAULT;
			if (val != 0)
			{
#ifndef PCM_FORCE_MONO
			    if (val >= 2)
			    {
		    		rh(PCMC) |= (1<<12);
				val = pcm.channels = 2;
			    } else
#endif
			    {
		    		rh(PCMC) &= ~(1<<12);
				val = pcm.channels = 1;
			    }
                            
			    return put_user(val,(int*)arg);
			    
			}
			

    case SNDCTL_DSP_GETFMTS:
                        printk("GETFMTS\n");
#ifdef PCM_FORCE_8BIT
			return put_user( AFMT_U8 , (int*)arg );
#else
			return put_user( AFMT_U8 | AFMT_S16_LE, (int*)arg );
#endif

    case SNDCTL_DSP_SETFMT:
                      if (get_user(val,(int*)arg))
                        return -EFAULT;
                      printk("SETFMT: %d\n",val);
                      pcm_setfmt(val);
		      val = pcm.fmt;
                      return put_user(pcm.fmt,(int*) arg);


    case SNDCTL_DSP_GETBLKSIZE:
                      if (get_user(val,(int*)arg))
                        return -EFAULT;

		      val = buffer_size;
                      printk("GETBLKSIZE: %d\n",val);
			return put_user(val,(int *)arg);

    case SOUND_PCM_READ_RATE:
		    printk("READ_RATE: %d\n",pcm.rate);
		    return put_user(pcm.rate,(int *)arg);
		    
    case SOUND_PCM_READ_CHANNELS:
		    printk("READ_CHANNELS: %d\n",pcm.channels);
		    return put_user(pcm.channels,(int *)arg);
		    
    case SOUND_PCM_READ_BITS:
		    printk("READ_BITS: %d\n",pcm.bits);
		    return put_user(pcm.bits,(int *)arg);
	
    case SNDCTL_DSP_SYNC:
                    printk("SYNC\n");
		    return 0;

    case SNDCTL_DSP_NONBLOCK:
		    printk("NONBLOCK\n");                    
		    file->f_flags |= O_NONBLOCK;                                                          
                    return 0;                       
                    
    case SNDCTL_DSP_GETCAPS:
                    printk("GETCAPS\n");
                    return put_user(0,(int *) arg);
    
    case SOUND_PCM_WRITE_FILTER:                                                                          
                    printk("PCM_WRITE_FILTER\n");
    case SNDCTL_DSP_SETSYNCRO:                                                                            
                    printk("PCM_SETSYNCRO\n");
    case SOUND_PCM_READ_FILTER:                                                                           
                    printk("PCM_READ_FILTER\n");
    case SNDCTL_DSP_POST:
                    printk("POST\n");
    case SNDCTL_DSP_SUBDIVIDE:
                    printk("SUBDIVIDE\n");
    case SNDCTL_DSP_SETFRAGMENT:
                    printk("SETFRAGMENT\n");
    case SNDCTL_DSP_GETOSPACE:
                    printk("GETOSPACE\n");
    case SNDCTL_DSP_GETISPACE:
                    printk("GETISPACE\n");
		    
    case SNDCTL_DSP_GETTRIGGER:
                    printk("GETTRIGGER\n");
    case SNDCTL_DSP_SETTRIGGER:
                    printk("SETTRIGGER\n");
    case SNDCTL_DSP_GETIPTR:
                    printk("GETIPTR\n");
    case SNDCTL_DSP_GETOPTR:
                    printk("GETOPTR\n");
    case SNDCTL_DSP_MAPINBUF:
                    printk("MAPINBUF\n");
    case SNDCTL_DSP_MAPOUTBUF:
                    printk("MAPOUTBUF\n");

		        return -EINVAL;                                                                               
							    

    

//    case SNDCTL_DSP_SAMPLESIZE:
//    case SOUND_MIXER_READ_DEVMASK:break;
//    case SOUND_MIXER_WRITE_PCM:break;
//    case SOUND_MIXER_WRITE_VOLUME:break;

    default:   
        printk("IOCTL: %04X\n",cmd); 
        return -EINVAL;                                                                               
  }

  return 0;
}

static unsigned char swapbuffer[2048];

static void startplay(int start)
{
  int byps=1;
  
    // 16 bit ?
#ifndef PCM_FORCE_8BIT
  if ( pcm.bits == 16 )
    byps<<=1;
#endif

  // stereo ?
#ifndef PCM_FORCE_MONO
  if ( pcm.channels == 2 )
    byps<<=1;
#endif
 
  rw(PCMA)=(512<<22)|buffer_rptr;
  buffer_rptr+=512*byps;
  if (buffer_rptr>=buffer_end)
    buffer_rptr-=buffer_size;

  rw(PCMA)=(512<<22)|buffer_rptr;

  buffer_rptr+=512*byps;
  if (buffer_rptr>=buffer_end)
    buffer_rptr-=buffer_size;
}

static void stopplay(void)
{
  rw(PCMA)=1;
}

static ssize_t pcm_write (struct file *file, const char *buf, size_t count, loff_t *offset)
{
  int written=0;

  int byps=1;     // bytes per sample
  int bit16=0;
  int i;
  
  // 16 bit ?
#ifndef PCM_FORCE_8BIT
  if ( pcm.bits == 16 )
  {
    byps<<=1;
//    bit16=1;
  }
#endif

  // stereo ?
#ifndef PCM_FORCE_MONO
  if ( pcm.channels == 2 )
    byps<<=1;
#endif

  if (count<=0)
    return -EFAULT;

  while (count)
  {
    int tocopy=count;
    
    int avail;
    
    avail=buffer_playptr-buffer_wptr;

    if (avail<0)
      avail=buffer_end-buffer_wptr;

//    printk("rptr: %x wptr: %x (play %x) (avail: %x)\n", buffer_rptr, buffer_wptr, buffer_playptr, avail);
    if (!avail)
    {
      if (file->f_flags&O_NONBLOCK)
      {
        if (written)
          return written;
        else
          return -EWOULDBLOCK;
      }
      
      interruptible_sleep_on_timeout(&pcm_wait, 1000);
      if (signal_pending(current))
        return -ERESTARTSYS;
      continue;
    }
    
    if (tocopy>avail)
      tocopy=avail;

    if (tocopy>2048)
      tocopy=2048;

    tocopy&=~3;

    if (!tocopy)
      return -EIO;

#ifndef PCM_FORCE_8BIT
    if (pcm.bits == 16 )          // swap bytes
    {
      u16 *d=(u16*)(gtxmem+buffer_wptr);
      u8 *s=swapbuffer;
      if (copy_from_user(swapbuffer, buf, tocopy))
        return -EFAULT;

      for (i=0; i<tocopy; i+=2)
      {
        *d++=(s[1]<<8)|s[0];
        s+=2;
      }
    } else
#endif
      if (copy_from_user(gtxmem+buffer_wptr, buf, tocopy))
        return -EFAULT;

    buffer_wptr+=tocopy;
    if (buffer_wptr>=buffer_end)
      buffer_wptr-=buffer_size;

    written+=tocopy;
    count-=tocopy;
    buf+=tocopy;
  }
  
  underrun=0;

  if (rw(PCMA)&1)
  {
//    printk("starting playback: %x .. %x\n", buffer_rptr, buffer_wptr);
    startplay(buffer_rptr);
  }

  return written;
}

static ssize_t pcm_read (struct file *file, char *buf, size_t count, loff_t *offset)
{
  return 0;
}

static int pcm_open (struct inode *inode, struct file *file)
{
  buffer_rptr=buffer_wptr=buffer_start;
  buffer_playptr=buffer_end-4;
  return 0;
}

static struct file_operations pcm_fops = {
        owner:          THIS_MODULE,
        read:           pcm_read,
        write:          pcm_write,
        ioctl:          pcm_ioctl,
        open:           pcm_open,
};

static void pcm_interrupt( int reg, int bit )
{
  int byps=1;
  
  if ( (bit != PCM_PF_INTR_BIT) || (reg != PCM_INTR_REG))
  {
//    printk("ign %d %d\n", bit, reg);
    return;
  }
  
  rh(PCMC)&=~1<<10;
#ifndef PCM_FORCE_8BITS
  if ( pcm.bits == 16 )
    byps<<=1;
#endif

#ifndef PCM_FORCE_MONO
  if ( pcm.channels == 2 )
    byps<<=1;
#endif

 
//  printk("another block played (%x:%x) -> %x (%x) -> %x.\n", reg, bit, rw(PCMD), rw(PCMA), rh(PCMC));

  wake_up_interruptible( &pcm_wait );

  buffer_playptr=rw(PCMD);
  if (buffer_playptr>=buffer_end)
    buffer_playptr-=buffer_size;
  
  buffer_playptr&=~3;
  
//    printk("%x, %x\n", buffer_rptr, rw(PCMA));
//    rw(PCMA)=buffer_rptr;                 // play next block

  if (underrun)
  {
//    printk("underrun -> stop.\n");
    stopplay();
    return;
  }

  if ((buffer_rptr <= buffer_wptr) && ((buffer_rptr+512*byps) > buffer_wptr))   // THIS block includes WPTR
  {
//    printk("silencing from %x to %x (wptr is %x)\n", buffer_wptr, buffer_rptr+512*byps, buffer_wptr);
                // das hier ist nicht SOOO schön gelöst, aber buffer-underruns sollten ja auch nicht soo häufig sein.
    memset(gtxmem+buffer_wptr, 0, (buffer_rptr+512*byps)-buffer_wptr);        // silence. WE ARE SIGNED. :)
    buffer_wptr=buffer_rptr+512*byps;
    if (buffer_wptr>=buffer_end)
      buffer_wptr-=buffer_size;
    underrun=1;
  }

  rw(PCMA)=(512<<22)|buffer_rptr;

  buffer_rptr+=512*byps;               // next block
  if (buffer_rptr>=buffer_end)
    buffer_rptr-=buffer_size;
    
  if (rh(PCMC)&(1<<9))
  {
    printk("OVERFLOW.\n");
    rh(PCMC)&=~(1<<9);
  }

}

/* --------------------------------------------------------------------- */

static const struct {
  unsigned volidx:4;
  unsigned left:4;
  unsigned right:4;
  unsigned stereo:1;
  unsigned recmask:13;
  unsigned avail:1;
} mixtable[SOUND_MIXER_NRDEVICES] = {
  [SOUND_MIXER_VOLUME] = { 0, 0x0, 0x1, 1, 0x0000, 1 },   /* master */
  [SOUND_MIXER_PCM]    = { 1, 0x2, 0x3, 1, 0x0400, 1 },   /* voice */
  [SOUND_MIXER_SYNTH]  = { 2, 0x4, 0x5, 1, 0x0060, 1 },   /* FM */
  [SOUND_MIXER_CD]     = { 3, 0x6, 0x7, 1, 0x0006, 1 },   /* CD */
  [SOUND_MIXER_LINE]   = { 4, 0x8, 0x9, 1, 0x0018, 1 },   /* Line */
  [SOUND_MIXER_LINE1]  = { 5, 0xa, 0xb, 1, 0x1800, 1 },   /* AUX */
  [SOUND_MIXER_LINE2]  = { 6, 0xc, 0x0, 0, 0x0100, 1 },   /* Mono1 */
  [SOUND_MIXER_LINE3]  = { 7, 0xd, 0x0, 0, 0x0200, 1 },   /* Mono2 */
  [SOUND_MIXER_MIC]    = { 8, 0xe, 0x0, 0, 0x0001, 1 },   /* Mic */
  [SOUND_MIXER_OGAIN]  = { 9, 0xf, 0x0, 0, 0x0000, 1 }    /* mono out */
};

static int mixer_ioctl(struct pcm_state *s, unsigned int cmd, unsigned long arg)
{
  if (_SIOC_DIR(cmd) & _SIOC_WRITE)
  {
    switch(cmd) {
      case SOUND_MIXER_VOLUME:
        rw(PCMN) = (rw(PCMN) & 0xFFFF) | (*(int*)arg << 16);
        break;
    default:
        return -EINVAL;
    }
  }
  else
  {
    switch(cmd) {
      case SOUND_MIXER_VOLUME:
        *(int*)arg = (rw(PCMN)>>16)&0xffff;
        break;
    default:
        return -EINVAL;
    }
  }

  return 0;
}

static loff_t pcm_llseek(struct file *file, loff_t offset, int origin)
{
  return -ESPIPE;
}

static int pcm_open_mixdev(struct inode *inode, struct file *file)
{
  return 0;
}

static int pcm_release_mixdev(struct inode *inode, struct file *file)
{
  return 0;
}

static int pcm_ioctl_mixdev(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
  return mixer_ioctl((struct pcm_state *)file->private_data, cmd, arg);
}

static /*const*/ struct file_operations pcm_mixer_fops = {
  owner:    THIS_MODULE,
  llseek:    pcm_llseek,
  ioctl:    pcm_ioctl_mixdev,
  open:    pcm_open_mixdev,
  release:  pcm_release_mixdev,
};

/* --------------------------------------------------------------------- */

static int init_audio(void)
{
  printk("DBox-II PCM driver v0.1\n");

  gtxmem = gtx_get_mem();
  gtxreg = gtx_get_reg();

  if (!gtxmem)
  {
    printk("gtxmem not remap.\n");
    return -1;
  }

  init_waitqueue_head(&pcm_wait);

  if ( gtx_allocate_irq( PCM_INTR_REG, PCM_AD_INTR_BIT, pcm_interrupt ) < 0 )
  {
    printk("pcm.o: unable to get interrupt\n");
    return -EIO;
  }

  if ( gtx_allocate_irq( PCM_INTR_REG, PCM_PF_INTR_BIT, pcm_interrupt ) < 0 )
  {
    printk("pcm.o: unable to get interrupt\n");
    return -EIO;
  }

  pcm_reset();


  avia_audio_init();

  return 0;
}

static int __init init_pcm(void)
{
  printk(KERN_INFO "pcm: version v0.1 time " __TIME__ " " __DATE__ "\n");

  // Todo: error handling ...
  s.dev_audio = register_sound_dsp(&pcm_fops, -1); //) < 0)

  s.dev_mixer = register_sound_mixer(&pcm_mixer_fops, -1); //) < 0)

  return init_audio();
}

static void __exit cleanup_pcm(void)
{
  printk(KERN_INFO "pcm: unloading\n");

  gtx_free_irq( PCM_INTR_REG, PCM_AD_INTR_BIT );
  gtx_free_irq( PCM_INTR_REG, PCM_PF_INTR_BIT );

  unregister_sound_dsp(s.dev_audio);
  unregister_sound_mixer(s.dev_mixer);
}

module_init(init_pcm);
module_exit(cleanup_pcm);

/* --------------------------------------------------------------------- */

// Todo: add kernel support
#ifndef MODULE

static int __init pcm_setup(char *str)
{
/*
  static unsigned __initdata nr_dev = 0;

  if (nr_dev >= NR_DEVICE)
    return 0;

  (void)
  (   (get_option(&str,&joystick[nr_dev]) == 2)
   && (get_option(&str,&lineout [nr_dev]) == 2)
   &&  get_option(&str,&micbias [nr_dev])
  );

  nr_dev++;
  return 1;
*/

	/* io, irq, dma, dma2 mpuio, mpuirq*/
/*	int ints[7];
	
	str = get_options(str, ARRAY_SIZE(ints), ints);
	
	io	= ints[1];
	irq	= ints[2];
	dma	= ints[3];
	dma2	= ints[4];
	mpuio	= ints[5];
	mpuirq	= ints[6];
*/

    return 1;
}

__setup("pcm=", pcm_setup);

#endif /* MODULE */

