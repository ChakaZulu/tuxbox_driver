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

#include "gtx.h"
#include "gtx-dmx.h"

#define NUM_QUEUES      32
#define VIDEO_QUEUE     0
#define AUDIO_QUEUE     1
#define TELETEXT_QUEUE  2
#define USER_QUEUE_START 3
#define LAST_USER_QUEUE 30
#define MESSAGE_QUEUE   31
#define HIGHSPEED_QUEUE 32

#define NUM_PID_FILTER  32

typedef struct gtxqueue_s
{
  int base, end;      // all relative to gtx_mem
  int size;
  int quid;
} gtxqueue_t;

static wait_queue_head_t queuewait;
static gtxqueue_t gtx_queue[NUM_QUEUES];

typedef struct queue_s
{
  int readptr;
  gtxqueue_t *gtxqueue;
} queue_t;

typedef struct user_dmx_filter_s
{
  queue_t queue[32];
  int uqueue[32];               // used/non-used
} user_dmx_filter_t;

void gtx_set_pid_table(int entry, int wait_pusi, int invalid, int pid);

static int gtx_fioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t gtx_fwrite (struct file *file, const char *buf, size_t count, loff_t *offset);
static ssize_t gtx_fread (struct file *file, char *buf, size_t count, loff_t *offset);
static int gtx_fopen (struct inode *inode, struct file *file);
static int gtx_frelease(struct inode *inode, struct file *file);
static u32 gtx_get_queue_wptr(int queue);
static Pcr_t gtx_read_transport_pcr(void);
static Pcr_t gtx_read_latched_clk(void);
static Pcr_t gtx_read_current_clk(void);
static s32 gtx_calc_diff(Pcr_t clk1, Pcr_t clk2);
static s32 gtx_bound_delta(u32 bound, s32 delta);

static struct file_operations gtx_fops = {
        owner:          THIS_MODULE,
        read:           gtx_fread,
        write:          gtx_fwrite,
        ioctl:          gtx_fioctl,
        open:           gtx_fopen,
        release:        gtx_frelease,
};


static int gtx_fioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
  return -EINVAL;
}

static ssize_t gtx_fwrite (struct file *file, const char *buf, size_t count, loff_t *offset)
{
  return -EINVAL;
}                             

static ssize_t gtx_fread (struct file *file, char *buf, size_t count, loff_t *offset)
{
  user_dmx_filter_t *flt=(user_dmx_filter_t*)file->private_data;
  int read=0;
 //  printk("gtx_fread\n");
  while (count)
  {
    DECLARE_WAITQUEUE(wait, current);
    int i, numwc=0, ok=0;

    for (i=0; i<32; i++)
    {
      if (flt->uqueue[i])
      {
//        printk("%d, rptr: %d wptr %d\n", flt->queue[i].gtxqueue->quid, flt->queue[i].readptr, gtx_get_queue_wptr(flt->queue[i].gtxqueue->quid));
        if (flt->queue[i].readptr!=gtx_get_queue_wptr(flt->queue[i].gtxqueue->quid))            // daten da?
        {
          int nrb=gtx_get_queue_wptr(flt->queue[i].gtxqueue->quid)-flt->queue[i].readptr;       // und zwar bis zum wp
//          printk("data inside.\n");
          if (nrb<0)                                                                            // wrap around?
            nrb+=flt->queue[i].gtxqueue->size;                                                  // add size to unwrap
//          printk("%d bytes in queue %d\n", nrb, flt->queue[i].gtxqueue->quid);
          if (nrb>count)
            nrb=count;
          if (flt->queue[i].readptr+nrb>flt->queue[i].gtxqueue->end)
          {
            int wb=flt->queue[i].gtxqueue->end-flt->queue[i].readptr;
//            printk("wrapping: %d, %d\n", wb, nrb-wb);
            copy_to_user(buf, gtxmem+flt->queue[i].readptr, wb);
            copy_to_user(buf+wb, gtxmem+flt->queue[i].gtxqueue->base, nrb-wb);
          } else
          {
            copy_to_user(buf, gtxmem+flt->queue[i].readptr, nrb);
          }
          buf+=nrb;
//          printk("so we're reading %d bytes\n", nrb);
          read+=nrb;
          count-=nrb;
          flt->queue[i].readptr+=nrb;
          if (flt->queue[i].readptr>flt->queue[i].gtxqueue->end)
            flt->queue[i].readptr-=flt->queue[i].gtxqueue->size;
          ok++;
        }
        numwc++;
      }
    }
//    add_wait_queue(&flt->queue[i].gtxqueue->wait, &wait);
    if (!numwc)
      return -EIO;
    add_wait_queue(&queuewait, &wait);
//    printk("no data but %d queues left.\n", numwc);
    if (!ok)
    {
      set_current_state(TASK_INTERRUPTIBLE);
      schedule();
      current->state = TASK_RUNNING;
    }
/*    for (i=0; i<32; i++)
      if (flt->uqueue[i])
        remove_wait_queue(&flt->queue[i].gtxqueue->wait, &wait); */
    remove_wait_queue(&queuewait, &wait);
    if (signal_pending(current))
      return -ERESTARTSYS;
  }
  return read;
}

static int gtx_fopen (struct inode *inode, struct file *file)
{
// unsigned int minor=MINOR (file->f_dentry->d_inode->i_rdev);
  user_dmx_filter_t *flt;
    
  file->private_data=kmalloc(sizeof(user_dmx_filter_t), GFP_KERNEL);
  if (!file->private_data)
    return -ENOMEM;
    
  flt=(user_dmx_filter_t*)file->private_data;
  
  memset(flt->uqueue, 0, 32*sizeof(int));
  
// hack hack hack
  flt->queue[0].gtxqueue=&gtx_queue[0];
  flt->queue[0].readptr=gtx_get_queue_wptr(0);
  flt->uqueue[0]=0;
  flt->queue[1].gtxqueue=&gtx_queue[1];
  flt->queue[1].readptr=gtx_get_queue_wptr(1);
  flt->uqueue[1]=1;
  return 0;
}

static int gtx_frelease(struct inode *inode, struct file *file)
{
  user_dmx_filter_t *flt=(user_dmx_filter_t*)file->private_data;
/*  for (i=0; i<32; i++)
    if (flt->uqueue[i])
      gtx_free_queue(&flt->uqueue[i]); */
  kfree((void*)flt);
  return 0;
}

void gtx_set_pid_table(int entry, int wait_pusi, int invalid, int pid)
{
  rh(RISC+0x700+entry*2)=((!!wait_pusi)<<15)|((!!invalid)<<14)|pid;
}

void gtx_set_pid_control_table(int entry, int type, int queue, int fork, int cw_offset, int cc, int start_up, int pec)
{
  u8 w[4];

  w[0]=type<<5;
  w[0]|=(queue)&31;
  w[1]=(!!fork)<<7;
  w[1]|=cw_offset<<4;
  w[1]|=cc;
  w[2]=(!!start_up)<<6;
  w[2]|=(!!pec)<<5;
  w[3]=0;
  rw(RISC+0x740+entry*4)=*(u32*)w;
}

void gtx_set_queue(int queue, u32 wp, u8 size)
{
  /* the 32 queue pointers are visible to the
   * host processor in two banks of 16.
   */
  if (queue>=16)
    rh(CR1)|=0x10;
  else
    rh(CR1)&=~0x10;
  queue &= 0xF;
  
  rh(QWPnL+4*queue)=wp&0xFFFF;
  rh(QWPnH+4*queue)=((wp>>16)&63)|(size<<6);
}

u32 gtx_get_queue_wptr(int queue)
{
  u32 wp;
  if (queue>=16)
    rh(CR1)|=0x10;
  else
    rh(CR1)&=~0x10;
  queue &= 0xF;
  wp=rh(QWPnL+4*queue);
  wp|=(rh(QWPnH+4*queue)&63)<<16;
  return wp; 
}

#define Q_VIDEO         2               // to check AGAIN and AGAIN...  aber eigentlich sollte das stimmen
#define Q_AUDIO         0
#define Q_TELETEXT      1

void gtx_set_queue_pointer(int queue, u32 read, u32 write, int size, int halt)
{
  int base=queue*8+0x1E0;
  
  rhn(base)=read&0xFFFF;
  rhn(base+4)=write&0xFFFF;
  rhn(base+6)=((write>>16)&63)|(size<<6);
  rhn(base+2)=((read>>16)&63)|(halt<<15);
}

static void gtx_queue_interrupt(int nr, int bit)
{
//  int queue=(nr-2)*16+bit;
//  printk("data on queue %d\n", queue);
//  wake_up(&gtx_queue[queue].wait);
  wake_up(&queuewait);
}

static Pcr_t gtx_read_transport_pcr(void)
{
  Pcr_t pcr;
  pcr.hi =rh(PCR2)<<16;
  pcr.hi|=rh(PCR1);
  pcr.lo =rh(PCR0)&0x81FF;
  return pcr;
}

static Pcr_t gtx_read_latched_clk(void)
{
  Pcr_t pcr;
  pcr.hi =rh(LSTC2)<<16;
  pcr.hi|=rh(LSTC1);
  pcr.lo =rh(LSTC0)&0x81FF;
  return pcr;
}

static Pcr_t gtx_read_current_clk(void)
{
  Pcr_t pcr;
  pcr.hi =rh(STC2)<<16;
  pcr.hi|=rh(STC1);
  pcr.lo =rh(STC0)&0x81FF;
  return pcr;
}

static void gtx_set_pcr(Pcr_t pcr)
{
  rh(STC2)=pcr.hi>>16;
  rh(STC1)=pcr.hi&0xFFFF;
  rh(STC0)=pcr.lo&0x81FF;
}

static s32 gtx_calc_diff(Pcr_t clk1, Pcr_t clk2)
{
  s32 delta;
  delta = (s32) clk1.hi;
  delta -= (s32) clk2.hi;
  delta *= (s32) 2;
  delta += (s32)((clk1.lo >> 15) & 1);
  delta -= (s32)((clk2.lo >> 15) & 1);
  delta *= (s32) 300;
  delta += (s32)(clk1.lo & 0x1FF);
  delta -= (s32)(clk2.lo & 0x1FF);
  return delta;
}

static s32 gtx_bound_delta(u32 bound, s32 delta)
{
  if (delta>bound)
    delta=bound;
  if (delta<-bound)
    delta=-bound;
  return delta;
}

static int discont=1, large_delta_count, deltaClk_max, deltaClk_min, deltaPCR_AVERAGE;

static Pcr_t oldClk;

extern void avia_set_cr(u32 hi, u32 lo);

static void gtx_pcr_interrupt(int b, int r)
{
  Pcr_t TPpcr;
  Pcr_t latchedClk;
  Pcr_t currentClk;
  s32 delta_PCR_AV;
  
  s32 deltaClk, elapsedTime;
  
  TPpcr=gtx_read_transport_pcr();
  latchedClk=gtx_read_latched_clk();
  currentClk=gtx_read_current_clk();

/*  printk("tp pcr %08x%08x, ", TPpcr.hi, TPpcr.lo);
  printk("lat pcr %08x%08x, ", latchedClk.hi, latchedClk.lo);
  printk("cur pcr %08x%08x\n", currentClk.hi, currentClk.lo); */

  if (discont)
  {
    oldClk=currentClk;
    discont=0;
    large_delta_count=0;
    printk("we have a discont:\n");
    printk("write stc: %08x%08x\n", TPpcr.hi, TPpcr.lo);      // wieso LATCHED CLOCK?
    gtx_set_pcr(TPpcr);
    avia_set_pcr(TPpcr.hi, TPpcr.lo);
    return;
  }
  elapsedTime=latchedClk.hi-oldClk.hi;
  if (elapsedTime > TIME_HI_THRESHOLD)
  {
    printk("elapsed time disc.\n");
    goto WE_HAVE_DISCONTINUITY;
  }
  deltaClk=TPpcr.hi-latchedClk.hi;
  if ((deltaClk < -MAX_HI_DELTA) || (deltaClk > MAX_HI_DELTA))
  {
    printk("large_delta disc.\n");
    if (large_delta_count++>MAX_DELTA_COUNT)
      goto WE_HAVE_DISCONTINUITY; 
    return;
  } else
    large_delta_count=0;
  
  deltaClk=gtx_calc_diff(TPpcr, latchedClk);
  deltaPCR_AVERAGE*=99;
  deltaPCR_AVERAGE+=deltaClk*10;
  deltaPCR_AVERAGE/=100;
  
  delta_PCR_AV=deltaClk-deltaPCR_AVERAGE/10;
  if (delta_PCR_AV > deltaClk_max)
    deltaClk_max=delta_PCR_AV;
  if (delta_PCR_AV < deltaClk_min)
    deltaClk_min=delta_PCR_AV;

  elapsedTime=gtx_calc_diff(latchedClk, oldClk);
  if (elapsedTime > TIME_THRESHOLD)
    goto WE_HAVE_DISCONTINUITY;

  deltaClk=gtx_bound_delta(MAX_DAC, deltaClk);
  rh(DPCR)=deltaClk;
  
  oldClk=latchedClk;
  return;
WE_HAVE_DISCONTINUITY:
  printk("WE_HAVE_DISCONTINUITY\n");
  discont=1;
  
}

void gtx_dmx_set_time_source(int pid)
{
  rh(PCRPID)=(1<<13)|pid;
  rh(FCR)|=0x100;               // force discontinuity
  gtx_allocate_irq(0, 8, gtx_pcr_interrupt);       // pcr reception
}

#define PID_V   0x0FF
#define PID_A   0x100

void gtx_dmx_init(void)
{
  int i;
  printk("initializing gtx_dmx_init\n");
//  rh(RR1)&=~0x1C;               // take framer, ci, avi module out of reset
  rh(RR0)=0;            // autsch, das muss so. kann das mal wer überprüfen?
  rh(RR1)=0;
  rh(RISCCON)=0;

  rh(FCR)=0x9147;               // byte wide input
  rh(SYNCH)=0x21;

  rh(AVI)=0x71F;
  rh(AVI+2)=0xF;
 

  printk("AVI: %04x %04x\n", rh(AVI), rh(AVI+2));

  for (i=0; i<NUM_PID_FILTER; i++)      // disable all pid filters
  {
    gtx_set_pid_table(i, 0, 1, 0);
    gtx_set_queue(i, 0x20000*i, 11);
  }
  
  for (i=0; i<15; i++)
    rh(QI0+i*2)=0x8000;
  
  gtx_allocate_irq(2, 0, gtx_queue_interrupt);
  gtx_allocate_irq(2, 1, gtx_queue_interrupt);

  gtx_set_queue_pointer(Q_VIDEO, 0, 0, 11, 0);
  gtx_set_queue_pointer(Q_AUDIO, 0, 0, 8, 0);

//  gtx_set_queue(0, 0, 11);		// video
//  gtx_set_queue(1, 0x20000, 8);		// audio

  gtx_set_pid_table(0, 0, 0, PID_V);     // video
  gtx_set_pid_table(1, 0, 0, PID_A);     // audio

  // we want a dual PES stream! - das PASST, wurde mir erzählt
  gtx_set_pid_control_table(0, 3, VIDEO_QUEUE, 1, 0, 0, 1, 0); 
  gtx_set_pid_control_table(1, 3, AUDIO_QUEUE, 1, 0, 0, 1, 0);

  for (i=0; i<NUM_QUEUES; i++)
  {
    gtx_queue[i].quid=i;
    gtx_queue[i].size=64*1024; // fix this somehow...
//    gtx_queue[i].base=gtx_allocate_dram(gtx_queue[i].size, gtx_queue[i].size);
    gtx_queue[i].base=i*0x10000;
    gtx_queue[i].end=gtx_queue[i].base+gtx_queue[i].size;

    gtx_set_queue(i, gtx_queue[i].base, 10);
//    init_waitqueue_head(&gtx_queue[i].wait);
  }

  init_waitqueue_head(&queuewait);
  gtx_set_queue_pointer(Q_VIDEO, gtx_queue[VIDEO_QUEUE].base, gtx_queue[VIDEO_QUEUE].base, 10, 0);        // set system queues
  gtx_set_queue_pointer(Q_AUDIO, gtx_queue[AUDIO_QUEUE].base, gtx_queue[AUDIO_QUEUE].base, 10, 0);
  gtx_set_queue_pointer(Q_TELETEXT, gtx_queue[AUDIO_QUEUE].base, gtx_queue[AUDIO_QUEUE].base, 10, 0);

  gtx_dmx_set_time_source(PID_V);
  printk("%08x\n", rw(RISC+0x740));
  if (register_chrdev(GTXDMX_MAJOR, "gtx-dmx", &gtx_fops))
  {
    printk("gtx-dmx: unable to get major %d\n", GTXDMX_MAJOR);
    return;
  }
}

void gtx_dmx_close(void)
{
  int i, j;
  if (unregister_chrdev(GTXDMX_MAJOR, "gtx-dmx"))
  {
    printk("gtx-dmx: unable to release major %d\n", GTXDMX_MAJOR);
    return;
  }

  for (j=0; j<2; j++)
    for (i=0; i<16; i++)
      gtx_free_irq(j+2, i);
  gtx_free_irq(0, 8);           // PCR
}
