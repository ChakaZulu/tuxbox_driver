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
  wait_queue_head_t wait;
} gtxqueue_t;

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

static int gtx_fioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t gtx_fwrite (struct file *file, const char *buf, size_t count, loff_t *offset);
static ssize_t gtx_fread (struct file *file, char *buf, size_t count, loff_t *offset);
static int gtx_fopen (struct inode *inode, struct file *file);
static int gtx_frelease(struct inode *inode, struct file *file);
static u32 gtx_get_queue_wptr(int queue);

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
  while (read<count)
  {
    DECLARE_WAITQUEUE(wait, current);
    int i, numwc=0, ok=0;

    for (i=0; i<32; i++)
      if (flt->uqueue[i])
      {
//        printk("%d, rptr: %d wptr %d\n", flt->queue[i].gtxqueue->quid, flt->queue[i].readptr, gtx_get_queue_wptr(flt->queue[i].gtxqueue->quid));
        if (flt->queue[i].readptr!=gtx_get_queue_wptr(flt->queue[i].gtxqueue->quid))
        {
          int nrb=gtx_get_queue_wptr(flt->queue[i].gtxqueue->quid)-flt->queue[i].readptr;
//          printk("data inside.\n");
          if (nrb<0)
            nrb+=flt->queue[i].gtxqueue->size;
//          printk("%d bytes in queue %d\n", nrb, flt->queue[i].gtxqueue->quid);
          if (nrb>count)
            nrb=count;
          if (flt->queue[i].readptr+nrb>flt->queue[i].gtxqueue->end)
          {
            int wb=flt->queue[i].gtxqueue->end-flt->queue[i].readptr;
            printk("wrapping: %d, %d\n", wb, nrb-wb);
            copy_to_user(buf, gtxmem+flt->queue[i].readptr, wb);
            copy_to_user(buf+wb, gtxmem+flt->queue[i].gtxqueue->base, nrb-wb);
          } else
          {
            copy_to_user(buf, gtxmem+flt->queue[i].readptr, nrb);
          }
          buf+=nrb;
//          printk("so we're reading %d bytes\n", nrb);
          read+=nrb;
          flt->queue[i].readptr+=nrb;
          if (flt->queue[i].readptr>flt->queue[i].gtxqueue->end)
            flt->queue[i].readptr-=flt->queue[i].gtxqueue->size;
          ok++;
        }
        add_wait_queue(&flt->queue[i].gtxqueue->wait, &wait);
        numwc++;
      }

//    printk("no data but %d queues left.\n", numwc);
    if (!numwc)
      return -EIO;
    if (!ok)
    {
      set_current_state(TASK_INTERRUPTIBLE);
      schedule();
      current->state = TASK_RUNNING;
    }
    for (i=0; i<32; i++)
      if (flt->uqueue[i])
        remove_wait_queue(&flt->queue[i].gtxqueue->wait, &wait);
    if (signal_pending(current))
      return -ERESTARTSYS;
    continue;
  }
  return read;
}

static int gtx_fopen (struct inode *inode, struct file *file)
{
// unsigned int minor=MINOR (file->f_dentry->d_inode->i_rdev);
  user_dmx_filter_t *flt;
  printk("gtx_fopen\n");
    
  file->private_data=kmalloc(sizeof(user_dmx_filter_t), GFP_KERNEL);
  if (!file->private_data)
    return -ENOMEM;
  flt=(user_dmx_filter_t*)file->private_data;
  memset(flt->uqueue, 0, sizeof(int)*32);
  
// hack hack hack
  flt->queue[0].gtxqueue=&gtx_queue[1];
  flt->queue[0].readptr=gtx_get_queue_wptr(1);
  flt->uqueue[0]=1;
  printk("done.\n");
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
  w[2]|=(!!start_up)<<6;
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

#define Q_VIDEO         2               // to check AGAIN and AGAIN...
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
  int queue=(nr-2)*16+bit;
//  printk("data on queue %d\n", queue);
  wake_up(&gtx_queue[queue].wait);
}

#define d udelay(1000*1000);

#define PID_A   0x00FF 
#define PID_V   0x0100

void gtx_dmx_init(void)
{
  int i;
  printk("initializing gtx_dmx_init\n");
//  rh(RR1)&=~0x1C;               // take framer, ci, avi module out of reset
  rh(RR0)=0;               // take framer, ci, avi module out of reset
  rh(RR1)=0;               // take framer, ci, avi module out of reset
  udelay(1000*1000);
//  rh(RISCCON)&=0xFFFD;
  rh(RISCCON)=0;

  rh(FCR)=0x9147;               // byte wide input
  rh(SYNCH)=0x21;
  
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

  // we want a dual PES stream!
  gtx_set_pid_control_table(0, 2, VIDEO_QUEUE, 1, 0, 0, 1, 0); 
  gtx_set_pid_control_table(1, 2, AUDIO_QUEUE, 1, 0, 0, 1, 0);
  
  for (i=0; i<NUM_QUEUES; i++)
  {
    gtx_queue[i].quid=i;
    gtx_queue[i].size=64*1024; // fix this somehow...
//    gtx_queue[i].base=gtx_allocate_dram(gtx_queue[i].size, gtx_queue[i].size);
    gtx_queue[i].base=i*0x10000;
    gtx_queue[i].end=gtx_queue[i].base+gtx_queue[i].size;

    gtx_set_queue(i, gtx_queue[i].base, 10);
    init_waitqueue_head(&gtx_queue[i].wait);
  }
  
  gtx_set_queue_pointer(Q_VIDEO, gtx_queue[VIDEO_QUEUE].base, gtx_queue[VIDEO_QUEUE].base, 10, 0);        // set system queues
  gtx_set_queue_pointer(Q_AUDIO, gtx_queue[AUDIO_QUEUE].base, gtx_queue[AUDIO_QUEUE].base, 10, 0);

  memset(gtxmem, 0, 2*1024*1024);

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
}
