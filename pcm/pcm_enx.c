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
 *   $Log: pcm_enx.c,v $
 *   Revision 1.4  2001/11/26 23:20:20  obi
 *   struct pcm_state moved to pcm.h
 *
 *   Revision 1.3  2001/03/29 14:05:01  fnbrd
 *   Siehe vorher, war falsche Datei.
 *
 *   Revision 1.2  2001/03/29 11:55:17  fnbrd
 *   Angepasst an neues enx.h. Geht aber immer noch nicht.
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
 *   $Revision: 1.4 $
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
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

#include <linux/sound.h>
#include <linux/soundcard.h>
#include "pcm.h"
#include "enx.h"
#include "avia.h"

#define wDR(a, d) avia_wr(TM_DRAM, a, d)
#define rDR(a) avia_rd(TM_DRAM, a)

#define PCM_INTR_REG        0
#define PCM_PF_INTR_BIT       3
#define PCM_AD_INTR_BIT       4

#define dprintk(fmt,args...) if(debug) printk( fmt,## args)

void avia_audio_init(void);

static wait_queue_head_t pcm_wait;

#ifdef MODULE
static int debug = 0;

MODULE_PARM(debug, "i");
MODULE_AUTHOR("Gillem <htoa@gmx.net>");
MODULE_DESCRIPTION("eNX-PCM Driver");
#endif

#include "dprintkRegBits.c" // I know, bad style

struct pcm_state s;

static unsigned char *enxmem;
//static unsigned char *gtxmem, *gtxreg;
static int buffer_start, buffer_size, buffer_end, buffer_wptr, buffer_rptr, buffer_playptr;
static int underrun=0;

/* reset pcm register on gtx */
void pcm_reset(void)
{
//  int cr;

  dprintk("pcm_enx: pcm_reset\n");

  dprintk("pcm_enx: CCR: 0x%08x\n", enx_reg_w(CRR));
  dprintk("pcm_enx: RCSC: 0x%08x\n", enx_reg_w(RCSC));

  // PCM aus dem reset holen
  enx_reg_w(RSTR0)&=~(1<<28);

  // PCM aus dem reset (ohne clock) holen
//  enx_reg_w(RSTR0)&=~1;

  enx_reg_w(CFGR0)|=1<<24; // DAC output enable

  // buffer disable
  enx_reg_w(PCMA) = 1;

  enx_reg_h(PCMC) = 0;

  // enable PCM frequ. same MPEG
  enx_reg_h(PCMC) |= (3<<14);

  // 16 bit mode
  enx_reg_h(PCMC) |= (1<<13);

  // stereo
  enx_reg_h(PCMC) |= (1<<12);

  // signed samples
  enx_reg_h(PCMC) |= (1<<11);

  // !!! ACLK NOT WORK !!! fnbrd: auch beim enx? muss noch testen

  // clock from aclk
  enx_reg_h(PCMC) &= ~(1<<6);  // 0: use external clock
//  enx_reg_h(PCMC) |= (1<<6);

  // set adv
//  enx_reg_h(PCMC) |= 0;

  // set acd
  enx_reg_h(PCMC) |= (2<<2);             // 256 ACLKs per LRCLK

  // set bcd
  enx_reg_h(PCMC) |= 2; // 32 clks

  dprintkRegBits("PCMC", enx_reg_h(PCMC), 16);
  dprintkRegBits("CFGR0", enx_reg_w(CFGR0), 32);

#ifdef NIXTUN
  /* enable pcm on enx */
  cr=rh(CR0);
  cr&=~(1<<9);
  rh(CR0)=cr;

  /* reset aclk  and pcm*/
  rh(RR0) |= 0x1200;
  rh(RR0) &= ~0x1200;

  /* buffer disable */
  rw(PCMA) = 1;

  /* set volume for pcm and mpeg */
//  rw(PCMN) = 0x40404040;

  rh(PCMC) = 0;

  /* enable PCM frequ. same MPEG */
  rh(PCMC) |= (3<<14);

  /* 16 bit mode */
  rh(PCMC) |= (1<<13);

  /* stereo */
  rh(PCMC) |= (1<<12);

  /* signed samples */
  rh(PCMC) |= (1<<11);

  /* !!! ACLK NOT WORK !!! */

  /* clock from aclk */
  rh(PCMC) &= ~(0<<6);  // 0: use external (avia) clock

  /* set adv */
  rh(PCMC) |= 0;

  /* set acd */
  rh(PCMC) |= 2<<2;             // 256 ACLKs per LRCLK

  /* set bcd */
  rh(PCMC) |= 2;
#endif
}

void avia_audio_init()
{
/*
  u32 val;

  val = 0;

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
  buffer_size= 0x10000;
  buffer_playptr=buffer_wptr=buffer_rptr=buffer_start;
  memset(enxmem+buffer_start, 0, buffer_size); // ...
}


void pcm_dump_reg(void)
{
  dprintk("pcm_enx: PCMA: 0x%08x\n", enx_reg_w(PCMA));
  dprintk("pcm_enx: PCMN: 0x%08x\n", enx_reg_w(PCMN));
  dprintkRegBits("PCMC", enx_reg_h(PCMC), 16);
//  dprintk("pcm_enx: PCMC: 0x%04x\n", enx_reg_h(PCMC));
  dprintk("pcm_enx: PCMD: 0x%08x\n", enx_reg_w(PCMD));
  dprintk("pcm_enx: PCMS: 0x%04x\n", enx_reg_h(PCMS));
}


static int pcm_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
  int val;
  switch(cmd) {

    case SNDCTL_DSP_RESET:
//    	dprintk("pcm_enx: IOCTL RESET\n");
                      pcm_reset();
                      return 0;
                      break;
    case SNDCTL_DSP_SPEED:
//    	dprintk("pcm_enx: IOCTL SPEED\n");
                      if (get_user(val,(int*)arg))
                        return -EFAULT;

                      /* clear flags */
                      enx_reg_h(PCMC) &= ~3;

                      /* set speed */
                      switch(val) {
                        case 44100:
                          enx_reg_h(PCMC) |= (3<<14);
		       	  dprintk("pcm_enx: SPEED 44100\n");
                          break;
                        case 22050:
                          enx_reg_h(PCMC) |= (2<<14);
		       	  dprintk("pcm_enx: SPEED 22050\n");
                          break;
                        case 11025:
                          enx_reg_h(PCMC) |= (1<<14);
		       	  dprintk("pcm_enx: SPEED 11025\n");
                          break;
                        default: printk("pcm_enx: SPEED: %d not support\n",val);return -1;
                      }

                      return 0;
                      break;

    case SNDCTL_DSP_STEREO:
//    	dprintk("pcm_enx: IOCTL STEREO\n");
                      if (get_user(val,(int*)arg))
                        return -EFAULT;

                      if (val)
                        enx_reg_h(PCMC) |= (1<<12);
                      else
                        enx_reg_h(PCMC) &= ~(1<<12);

                      dprintk("pcm_enx: STEREO: %d\n",val);

                      return 0;
                      break;

    case SNDCTL_DSP_SETFMT:
//    	dprintk("pcm_enx: IOCTL SETFMT\n");
                      if (get_user(val,(int*)arg))
                        return -EFAULT;
                      dprintk("pcm_enx: SETFMT: %d\n",val);
                      return 0;
                      break;

    case SNDCTL_DSP_GETFMTS:
    	dprintk("pcm_enx: IOCTL GETFMTS\n");
                      return put_user( AFMT_S16_NE|AFMT_S8, (int*)arg );
                      break;

    case SNDCTL_DSP_GETBLKSIZE:
//    	dprintk("pcm_enx: IOCTL GETBLKSIZE\n");
                      if (get_user(val,(int*)arg))
                        return -EFAULT;

                      dprintk("pcm_enx: GETBLKSIZE: %d\n",val);

                      break;
//    case SNDCTL_SEQ_THRESHOLD: break;
//    case 0x402c7413: break;
//    case SNDCTL_DSP_SAMPLESIZE:break;

//    case SOUND_MIXER_READ_DEVMASK:break;
//    case SOUND_MIXER_WRITE_PCM:break;
//    case SOUND_MIXER_WRITE_VOLUME:break;

    default:   printk("pcm_enx: IOCTL: %04X\n",cmd); break;
  }

  return 0;
}

static unsigned char swapbuffer[2048];

static void startplay(int start)
{
  int byps=1;
  dprintk("pcm_enx: startplay\n");

    // 16 bit ?
  if ( enx_reg_h(PCMC) & (1<<13) )
    byps<<=1;

  // stereo ?
  if ( enx_reg_h(PCMC) & (1<<12) )
    byps<<=1;

  dprintk("pcm_enx: PCMA: 0x%08x\n", enx_reg_w(PCMA));
  if(!(enx_reg_w(PCMA)&1))
    printk("pcm_enx: not ready for write!");
  enx_reg_h(PCMS)=512;
  enx_reg_w(PCMA)=buffer_rptr;
  dprintk("pcm_enx: startplay buffer_rptr: %x\n", buffer_rptr);
  buffer_rptr+=512*byps;
  if (buffer_rptr>=buffer_end)
    buffer_rptr-=buffer_size;
  if(enx_reg_w(PCMA)&1) {
    enx_reg_h(PCMS)=512;
    enx_reg_w(PCMA)=buffer_rptr;
    dprintk("pcm_enx: startplay buffer_rptr: %x\n", buffer_rptr);
    buffer_rptr+=512*byps;
    if (buffer_rptr>=buffer_end)
      buffer_rptr-=buffer_size;
  }
  pcm_dump_reg();
}

static void stopplay(void)
{
  dprintk("pcm_enx: stopplay\n");
  enx_reg_w(PCMA)=1;
}

static ssize_t pcm_write (struct file *file, const char *buf, size_t count, loff_t *offset)
{
  int written=0;

  int byps=1;     // bytes per sample
  int bit16=0;
  int i;

  dprintk("pcm_enx: pcm_write\n");
  dprintk("pcm_enx: buffer_end: %x\n", buffer_end);

  // 16 bit ?
  if ( enx_reg_h(PCMC) & (1<<13) )
  {
    byps<<=1;
    bit16=1;
  }

  // stereo ?
  if ( enx_reg_h(PCMC) & (1<<12) )
    byps<<=1;

  if (count<=0)
    return -EFAULT;

  while (count)
  {
    int tocopy=count;

    int avail;

    dprintk("pcm_enx: count: %x\n", count);
    avail=buffer_playptr-buffer_wptr;

    if (avail<0)
      avail=buffer_end-buffer_wptr;

    dprintk("pcm_enx: rptr: %x wptr: %x (play %x) (avail: %x)\n", buffer_rptr, buffer_wptr, buffer_playptr, avail);

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
  dprintk("pcm_enx: ISR0: 0x%x\n", enx_reg_h(ISR0));
  dprintk("pcm_enx: ISR1: 0x%x\n", enx_reg_h(ISR1));
  dprintk("pcm_enx: ISR2: 0x%x\n", enx_reg_h(ISR2));
  dprintk("pcm_enx: ISR3: 0x%x\n", enx_reg_h(ISR3));
  dprintk("pcm_enx: IMR0: 0x%x\n", enx_reg_h(IMR0));
  dprintk("pcm_enx: IMR1: 0x%x\n", enx_reg_h(IMR1));
  dprintk("pcm_enx: IMR2: 0x%x\n", enx_reg_h(IMR2));
  dprintk("pcm_enx: IMR3: 0x%x\n", enx_reg_h(IMR3));
  pcm_dump_reg();
      continue;
    }

    if (tocopy>avail)
      tocopy=avail;

    if (tocopy>2048)
      tocopy=2048;

    tocopy&=~3;

    if (!tocopy)
      return -EIO;

    dprintk("pcm_enx: tocopy: %x\n", tocopy);

    if (bit16)          // swap bytes
    {
      u16 *d=(u16*)(enxmem+buffer_wptr);
      u8 *s=swapbuffer;
      if (copy_from_user(swapbuffer, buf, tocopy))
        return -EFAULT;

      for (i=0; i<tocopy; i+=2)
      {
        *d++=(s[1]<<8)|s[0];
        s+=2;
      }
    } else
      if (copy_from_user(enxmem+buffer_wptr, buf, tocopy))
        return -EFAULT;

    buffer_wptr+=tocopy;
    if (buffer_wptr>=buffer_end)
      buffer_wptr-=buffer_size;

    written+=tocopy;
    count-=tocopy;
    buf+=tocopy;
    dprintk("pcm_enx: written: %x count: %x\n", written, count);
  }

  underrun=0;

  if (enx_reg_w(PCMA)&1)
  {
    dprintk("pcm_enx: starting playback: %x .. %x\n", buffer_rptr, buffer_wptr);
    startplay(buffer_rptr);
  }

  return written;
}

static ssize_t pcm_read (struct file *file, char *buf, size_t count, loff_t *offset)
{
  dprintk("pcm_enx: pcm_read\n");
  return 0;
}

static int pcm_open (struct inode *inode, struct file *file)
{
  dprintk("pcm_enx: pcm_open\n");
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

  dprintk("pcm_enx: IRQ reg %d bit %d\n", reg, bit);
  dprintk("pcm_enx: ISR0: 0x%x\n", enx_reg_h(ISR0));
  dprintk("pcm_enx: IMR0: 0x%x\n", enx_reg_h(IMR0));
  if ( (bit != PCM_PF_INTR_BIT) || (reg != PCM_INTR_REG))
  {
//    printk("ign %d %d\n", bit, reg);
    return;
  }

  enx_reg_h(PCMC)&=~(1<<10); // Stop loeschen

  if ( enx_reg_h(PCMC) & (1<<13) )
    byps<<=1;
  if ( enx_reg_h(PCMC) & (1<<12) )
    byps<<=1;

  dprintk("pcm_enx: another block played (%x:%x) -> %x (%x) -> %x.\n", reg, bit, enx_reg_w(PCMD), enx_reg_w(PCMA), enx_reg_h(PCMC));

  wake_up_interruptible( &pcm_wait );

  buffer_playptr=enx_reg_w(PCMD);
  if (buffer_playptr>=buffer_end)
    buffer_playptr-=buffer_size;

  buffer_playptr&=~3; // ?

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
    memset(enxmem+buffer_wptr, 0, (buffer_rptr+512*byps)-buffer_wptr);        // silence. WE ARE SIGNED. :)
    buffer_wptr=buffer_rptr+512*byps;
    if (buffer_wptr>=buffer_end)
      buffer_wptr-=buffer_size;
    underrun=1;
  }

  if(!(enx_reg_w(PCMA)&1))
    printk("pcm_enx: not ready for write!");
  enx_reg_h(PCMS)=512;
  enx_reg_w(PCMA)=buffer_rptr;
  dprintk("pcm_enx: startplay (irq) buffer_rptr: %x\n", buffer_rptr);

  buffer_rptr+=512*byps;               // next block
  if (buffer_rptr>=buffer_end)
    buffer_rptr-=buffer_size;

  if (enx_reg_h(PCMC)&(1<<9))
  {
    printk("pcm_enx: OVERFLOW.\n");
    enx_reg_h(PCMC)&=~(1<<9);
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
  dprintk("pcm_enx: mixer_ioctl\n");
  if (_SIOC_DIR(cmd) & _SIOC_WRITE)
  {
    switch(cmd) {
      case SOUND_MIXER_VOLUME:
        enx_reg_w(PCMN) = (enx_reg_w(PCMN) & 0xFFFF) | (*(int*)arg << 16);
        break;
    default:
        return -EINVAL;
    }
  }
  else
  {
    switch(cmd) {
      case SOUND_MIXER_VOLUME:
        *(int*)arg = (enx_reg_w(PCMN)>>16)&0xffff;
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

  enxmem = enx_get_mem_addr();
//fnbrd  gtxmem = gtx_get_mem();
//fnbrd  gtxreg = gtx_get_reg();

  if (!enxmem)
  {
    printk("pcm_enx: enxmem not remap.\n");
    return -1;
  }

  init_waitqueue_head(&pcm_wait);
  dprintk("pcm_enx: ISR0: 0x%x\n", enx_reg_h(ISR0));
  dprintk("pcm_enx: ISR1: 0x%x\n", enx_reg_h(ISR1));
  dprintk("pcm_enx: ISR2: 0x%x\n", enx_reg_h(ISR2));
  dprintk("pcm_enx: ISR3: 0x%x\n", enx_reg_h(ISR3));
  dprintk("pcm_enx: IMR0: 0x%x\n", enx_reg_h(IMR0));
  dprintk("pcm_enx: IMR1: 0x%x\n", enx_reg_h(IMR1));
  dprintk("pcm_enx: IMR2: 0x%x\n", enx_reg_h(IMR2));
  dprintk("pcm_enx: IMR3: 0x%x\n", enx_reg_h(IMR3));
  if ( enx_allocate_irq( PCM_INTR_REG, PCM_AD_INTR_BIT, pcm_interrupt ) < 0 )
  {
    printk("pcm_enx: unable to get interrupt\n");
    return -EIO;
  }

  if ( enx_allocate_irq( PCM_INTR_REG, PCM_PF_INTR_BIT, pcm_interrupt ) < 0 )
  {
    printk("pcm_enx: unable to get interrupt\n");
    return -EIO;
  }
  dprintk("pcm_enx: ISR0: 0x%x\n", enx_reg_h(ISR0));
  dprintk("pcm_enx: ISR1: 0x%x\n", enx_reg_h(ISR1));
  dprintk("pcm_enx: ISR2: 0x%x\n", enx_reg_h(ISR2));
  dprintk("pcm_enx: ISR3: 0x%x\n", enx_reg_h(ISR3));
  dprintk("pcm_enx: IMR0: 0x%x\n", enx_reg_h(IMR0));
  dprintk("pcm_enx: IMR1: 0x%x\n", enx_reg_h(IMR1));
  dprintk("pcm_enx: IMR2: 0x%x\n", enx_reg_h(IMR2));
  dprintk("pcm_enx: IMR3: 0x%x\n", enx_reg_h(IMR3));

  pcm_reset();


  avia_audio_init();

  return 0;
}

static int __init init_pcm(void)
{
  printk(KERN_INFO "pcm_enx: version v0.1 time " __TIME__ " " __DATE__ "\n");

  // Todo: error handling ...
  s.dev_audio = register_sound_dsp(&pcm_fops, -1); //) < 0)

  s.dev_mixer = register_sound_mixer(&pcm_mixer_fops, -1); //) < 0)

  return init_audio();
}

static void __exit cleanup_pcm(void)
{
  printk(KERN_INFO "pcm_enx: unloading\n");

  enx_free_irq( PCM_INTR_REG, PCM_AD_INTR_BIT );
  enx_free_irq( PCM_INTR_REG, PCM_PF_INTR_BIT );

  // disable PCM
  enx_reg_h(PCMC) &= ~(3<<14);

  // PCM in den reset stellen
  enx_reg_w(RSTR0)|=1<<28;

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
}

__setup("pcm=", pcm_setup);

#endif /* MODULE */
