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
#include "../avia/gtx.h"
#include "../avia/avia.h"

#define wDR(a, d) avia_wr(TM_DRAM, a, d)
#define rDR(a) avia_rd(TM_DRAM, a)


#define PCM_INTR_REG				1
#define PCM1_INTR_BIT       10
#define PCM2_INTR_BIT				12

void pcm_reset(void);
void pcm_dump_reg(void);

void avia_audio_init(void);

static wait_queue_head_t pcm_wait;

#ifdef MODULE
MODULE_AUTHOR("Gillem <htoa@gmx.net>");
MODULE_DESCRIPTION("GTX-PCM Driver");
#endif

struct pcm_state {
	/* soundcore stuff */
	int dev_audio;
};

struct pcm_state s;


unsigned char *gtxmem,*gtxreg;

/* reset pcm register on gtx */
void pcm_reset()
{
	int cr;

	/* enable pcm on gtx */
  cr=rh(CR0);
  cr&=~(1<<9);
  rh(CR0)=cr;

	/* enable dac on gtx */
  cr=rh(CR0);
  cr&=~(1<<5);
  rh(CR0)=cr;

	/* reset aclk */
  rh(RR0) |=  (1<<12);
  rh(RR0) &= ~(1<<12);

	/* reset pcm */
  rh(RR0) |=  (1<<9);
  rh(RR0) &= ~(1<<9);

	/* reset dac */
  rh(RR1) |=  (1<<6);
  rh(RR1) &= ~(1<<6);

	rh(RR0) = 0;
	rh(RR1) = 0;

	printk("DPCR: %08X\n",rw(DPCR));
	rw(DPCR) = 0xFFFF001F;
	printk("DPCR: %08X\n",rw(DPCR));

	/* buffer disable */
	rw(PCMA) = 1;

	/* set volume for pcm*/
	rw(PCMN) = 0x40400000;

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
	rh(PCMC) &= ~(1<<6);

	/* set adv */
	rh(PCMC) |= 0;

	/* set acd */
	rh(PCMC) |= 0;

	/* set bcd */
	rh(PCMC) |= 0;
}

void avia_audio_init()
{
	u32 val;

	val = 0;

	// AUDIO_CONFIG 12,11,7,6,5,4 reserved or must be set to 0
	val |= (0<<10);	// 64 DAI BCKs per DAI LRCK
	val |= (0<<9);	// input is I2S
	val |= (0<<8);	// output constan low (no clock)
	val |= (0<<3);	// 0: normal 1:I2S output
	val |= (0<<2);	// 0:off 1:on channels
	val |= (0<<1);  // 0:off 1:on IEC-958
	val |= (0);			// 0:encoded 1:decoded output
  wDR(0xE0, val);

	val = 0;

	// AUDIO_DAC_MODE 0 reserved
  val |= (0<<8);
  val |= (0<<6);
	val |= (0<<4);
	val |= (0<<3);	// 0:high 1:low DA-LRCK polarity
	val |= (0<<2);	// 0:0 as MSB in 24 bit mode 1: sign ext. in 24bit
	val |= (0<<1);	// 0:msb 1:lsb first
  wDR(0xE8, val);

	val = 0;

	// AUDIO_CLOCK_SELECTION
	val |= (0<<2);
	val |= (0<<1);	// 1:256 0:384 x sampling frequ.
	val |= (1);			// master,slave mode

  wDR(0xEC, val);

	val = 0;

	// AUDIO_ATTENUATION
  wDR(0xF4, 0);

	// wait for avia to ready
	val = 0xFF;
	wDR(0x468,val);
	while(rDR(0x468));
	printk("XXXX: %08X\n",rDR(0x468));
}


void pcm_dump_reg()
{
	printk("PCMA: %08X\n",rw(PCMA));
	printk("PCMN: %08X\n",rw(PCMN));
	printk("PCMC: %04X\n",rh(PCMC));
	printk("PCMD: %08X\n",rw(PCMD));
}


static int pcm_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int val;
	switch(cmd) {

		case SNDCTL_DSP_RESET:
                      printk("RESET\n");
											pcm_reset();
											return 0;
											break;
		case SNDCTL_DSP_SPEED:
											if (get_user(val,(int*)arg))
												return -EFAULT;

											/* clear flags */
											rh(PCMC) &= ~3;

											/* set speed */
											switch(val) {
												case 44100: rh(PCMC) |= (3<<14);break;
												case 22050: rh(PCMC) |= (2<<14);break;
												case 11025: rh(PCMC) |= (1<<14);break;
												default: printk("SPEED: %d not support\n",val);return -1;
											}

											return 0;
											break;

		case SNDCTL_DSP_STEREO:
											if (get_user(val,(int*)arg))
												return -EFAULT;

											if (val)
												rh(PCMC) |= (1<<12);
											else
												rh(PCMC) &= ~(1<<12);

                      printk("STEREO: %d\n",val);

											return 0;
											break;

		case SNDCTL_DSP_SETFMT:
											if (get_user(val,(int*)arg))
												return -EFAULT;
                      printk("SETFMT: %d\n",val);
											return 0;
											break;

		case SNDCTL_DSP_GETFMTS:
											return put_user( AFMT_S16_NE|AFMT_S8, (int*)arg );
											break;

		case SNDCTL_DSP_GETBLKSIZE:
											if (get_user(val,(int*)arg))
												return -EFAULT;

                      printk("GETBLKSIZE: %d\n",val);

											break;
//		case SNDCTL_DSP_SAMPLESIZE:break;

//		case SOUND_MIXER_READ_DEVMASK:break;
//		case SOUND_MIXER_WRITE_PCM:break;
//		case SOUND_MIXER_WRITE_VOLUME:break;

		default: 	printk("IOCTL: %04X\n",cmd); break;
	}

	return 0;
}

static ssize_t pcm_write (struct file *file, const char *buf, size_t count, loff_t *offset)
{
	int i,s,z,iret;
	unsigned char c;

	int samples;

	samples = 0;

	// 16 bit ?
	if ( rh(PCMC) & (1<<13) )
		samples += 2;
	// stereo ?
	if ( rh(PCMC) & (1<<12) )
		samples += 2;
	// none
	if (samples==0)
		samples++;

//	printk("COUNT: %d\n",count);

	if (count<=0)
    return -EFAULT;

  if (copy_from_user(gtxmem+1, buf, count))
    return -EFAULT;

  // swab
	for (i=0;i<(count/4);i+=4)
	{
		c = gtxmem[i];
		gtxmem[i] = gtxmem[i+2];
		gtxmem[i+2] = c;
		c = gtxmem[i+1];
		gtxmem[i+1] = gtxmem[i+3];
		gtxmem[i+3] = c;
	}

	// 8Bit
//	for (i=0;i<(count/2);i+=2){c = gtxmem[i];gtxmem[i] = gtxmem[i+1];gtxmem[i+1] = c;}

	s = count;

//	printk("SAMPLES: %d\n",samples);

	for(i=0;i<s;)
	{
		if ((s-i)>(0x3ff*samples))
			z=0x3ff;
		else
			z=s-i;

		rw(PCMA) = 1 | i<<1;
		rw(PCMA) |= z<<22;
		rw(PCMA) &= ~1;

		iret = interruptible_sleep_on_timeout(&pcm_wait,1000*100);

		printk("IRET: %08X\n",iret);

		i+=((z+1)*samples);
	}

  return count;
}

static ssize_t pcm_read (struct file *file, char *buf, size_t count, loff_t *offset)
{
	return 0;
}

static int pcm_open (struct inode *inode, struct file *file)
{
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
  /* Get 'me going again. */
  wake_up_interruptible( &pcm_wait );
}

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

	if ( gtx_allocate_irq( PCM_INTR_REG, PCM1_INTR_BIT, pcm_interrupt ) < 0 )
	{
    printk("pcm.o: unable to get interrupt\n");
    return -EIO;
	}

	if ( gtx_allocate_irq( PCM_INTR_REG, PCM2_INTR_BIT, pcm_interrupt ) < 0 )
	{
    printk("pcm.o: unable to get interrupt\n");
    return -EIO;
	}

	pcm_reset();

	init_waitqueue_head(&pcm_wait);

	avia_audio_init();

  return 0;
}

static int __init init_pcm(void)
{
	printk(KERN_INFO "pcm: version v0.1 time " __TIME__ " " __DATE__ "\n");

	s.dev_audio = register_sound_dsp(&pcm_fops, -1); //) < 0)

  return init_audio();
}

static void __exit cleanup_pcm(void)
{
	printk(KERN_INFO "pcm: unloading\n");

  unregister_sound_dsp(s.dev_audio);

	gtx_free_irq( PCM_INTR_REG, PCM1_INTR_BIT );
	gtx_free_irq( PCM_INTR_REG, PCM2_INTR_BIT );
}

module_init(init_pcm);
module_exit(cleanup_pcm);
