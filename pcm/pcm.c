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

#define PCM_INTR_REG				1
#define PCM1_INTR_BIT       10
#define PCM2_INTR_BIT				12

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

static int pcm_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int val;
	switch(cmd) {

		case SNDCTL_DSP_RESET:
                      printk("RESET\n");
											return 0;
											break;
		case SNDCTL_DSP_SPEED:
											if (get_user(val,(int*)arg))
												return -EFAULT;
                      printk("SPEED: %d\n",val);
											return 0;
											break;
		case SNDCTL_DSP_STEREO:
											if (get_user(val,(int*)arg))
												return -EFAULT;
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
											return put_user( AFMT_S16_LE|AFMT_U8, (int*)arg );
											break;

		case SNDCTL_DSP_GETBLKSIZE:
											if (get_user(val,(int*)arg))
												return -EFAULT;
                      printk("GETBLKSIZE: %d\n",val);
											return 0;
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
	int i,s,z;
	char c;

//	printk("COUNT: %d\n",count);

	if (count<=0)
    return -EFAULT;

  if (copy_from_user(gtxmem, buf, count))
    return -EFAULT;

  // swap
/*
	for (i=0;i<(count/2);i+=2)
	{
		c = gtxmem[i];
		gtxmem[i] = gtxmem[i+1];
		gtxmem[i+1] = c;
	}
*/
	// mono 16 bit
	s = count;

//	printk("SAMPLE: %d\n",s);

	for(i=0;i<s;)
	{
		if (((s-i))>(0x3FF*2))
			z=0x3FF;
		else
			z=s-i;

		rw(PCMA) = 1 | i<<1;
		rw(PCMA) |= z<<22;
		rw(PCMA) &= ~1;

		interruptible_sleep_on_timeout(&pcm_wait,10000*1000);

		i+=((z+1)*2);
//		printk("COUNT: %d %d %d\n",count,i,z);
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
//	printk("[PCM]: INTR %d %d\n",reg,bit);
/*
	printk("PCMA: %08X ",rw(PCMA));
	printk("PCMN: %08X ",rw(PCMN));
	printk("PCMC: %04X ",rh(PCMC));
	printk("PCMD: %08X\n",rw(PCMD));
	printk("PCMC: %04X ",rh(PCMC));
*/
  /* Get 'me going again. */
  wake_up_interruptible( &pcm_wait );
}

static int init_audio(void)
{
  int cr,i;
	int a,b,c;

  printk("DBox-II PCM driver v0.1\n");

	gtxmem = gtx_get_mem();
	gtxreg = gtx_get_reg();

  if (!gtxmem)
  {
    printk("gtxmem not remap.\n");
    return -1;
  }
/*
  if (register_chrdev(PCM_MAJOR, "pcm", &pcm_fops))
  {
    printk("pcm.o: unable to get major %d\n", PCM_MAJOR);
    return -EIO;
  }
*/
	if ( gtx_allocate_irq( PCM_INTR_REG, PCM1_INTR_BIT, pcm_interrupt /*void (*isr)(int, int)*/ ) < 0 )
	{
    printk("pcm.o: unable to get interrupt\n");
    return -EIO;
	}

	if ( gtx_allocate_irq( PCM_INTR_REG, PCM2_INTR_BIT, pcm_interrupt /*void (*isr)(int, int)*/ ) < 0 )
	{
    printk("pcm.o: unable to get interrupt\n");
    return -EIO;
	}

  cr=rh(CR0);
  cr&=~(1<<9);       // enable pcm
  rh(CR0)=cr;

	/* reset pcm */
  rh(RR0) |=  (1<<9);
  rh(RR0) &= ~(1<<9);
	
	printk("PCMA: %08X\n",rw(PCMA));
	printk("PCMN: %08X\n",rw(PCMN));
	printk("PCMC: %04X\n",rh(PCMC));
	printk("PCMD: %08X\n",rw(PCMD));

	for(i=0;i<0xFF;i++)
	{
		gtxmem[i]=i;
	}

	rw(PCMN) = 0x80808080;
	rh(PCMC) = 0;
	rh(PCMC) |= (3<<14);		// enable PCM frequ. same MPEG
	rh(PCMC) &= ~(1<<13);		// 16 bit
	rh(PCMC) &= ~(1<<12);		// mono
	rh(PCMC) |= (1<<11);		// signed sample

	rh(PCMC) &= ~(1<<6);		// clock

	rh(PCMC) |= 3<<4;     // adv
	rh(PCMC) |= 3<<2;     // acd
	rh(PCMC) |= 3;     // bcd

//	rh(PCMC) &= ~(1<<10);
//	rw(PCMA) |= 0x3FF<<22;

	printk("PCMA: %08X\n",rw(PCMA));

	init_waitqueue_head(&pcm_wait);

	rw(PCMA) &= ~1;			// buffer desc. is valid
/*
	printk("PCMA: %08X\n",rw(PCMA));
	printk("PCMN: %08X\n",rw(PCMN));
	printk("PCMC: %04X\n",rh(PCMC));
	printk("PCMD: %08X\n",rw(PCMD));
*/
  return 0;
}


#ifdef MODULE
/*
int init_module(void)
{
  return init_audio();
}
*/
/*
void cleanup_module(void)
{
  int res;
  if ((res=unregister_chrdev(PCM_MAJOR, "pcm")))
  {
    printk("pcm.o: unable to release major %d\n", PCM_MAJOR);
  }

	gtx_free_irq( PCM_INTR_REG, PCM1_INTR_BIT );
	gtx_free_irq( PCM_INTR_REG, PCM2_INTR_BIT );
}
*/
#endif

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
