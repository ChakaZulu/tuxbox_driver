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

#define NUM_QUEUES      32
#define VIDEO_QUEUE     0
#define AUDIO_QUEUE     1
#define TELETEXT_QUEUE  2
#define USER_QUEUE_START 3
#define LAST_USER_QUEUE 30
#define MESSAGE_QUEUE   31
#define HIGHSPEED_QUEUE 32

#define NUM_PID_FILTER  32

void gtx_set_pid_table(int entry, int wait_pusi, int invalid, int pid)
{
  rh(RISC+0x700+entry*2)=((!!wait_pusi)<<15)|((!!invalid)<<14)|pid;
}

void gtx_set_pid_control_table(int entry, int type, int queue, int fork, int cw_offset, int cc, int start_up, int pec)
{
  u8 w[4];
  
  w[0]=type<<5;
  w[0]|=(queue+1)&31;
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

#define Q_VIDEO         2
#define Q_AUDIO         0
#define Q_TELETEXT      1

/*
   audio    1e0
   teletext 1e6
   video    1f0
*/

#define g0 0
void gtx_set_queue_pointer(int queue, u32 read, u32 write, int size, int halt)
{
  int base=queue*8+0x1E0;
  
  rh(0+base)=read&0xFFFF;
  rh(0+base+4)=write&0xFFFF;
  rh(0+base+6)=((write>>16)&63)|(size<<6);
  rh(0+base+2)=((read>>16)&63)|(halt<<15);
}

static void gtx_queue_interrupt(int nr, int bit)
{
  printk("queue %d has new data!\n", (nr-2)*16+bit);
}

#define d udelay(1000*1000);

#define PID_V   0x12
#define PID_A   0x14
   
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

  rh(IMR0)=0;
  rh(IMR1)=0;
  rh(IMR2)=0xFFFF;
  rh(IMR3)=0xFFFF;

  rh(FCR)=0x9147;               // byte wide input
  rh(SYNCH)=0x21;

  for (i=0; i<NUM_PID_FILTER; i++)      // disable all pid filters
  {
    gtx_set_pid_table(i, 0, 1, 0);
    gtx_set_queue(i, 0x20000*i, 11);
  }
  
  gtx_allocate_irq(2, 0, gtx_queue_interrupt);
  gtx_allocate_irq(2, 1, gtx_queue_interrupt);

  gtx_set_queue_pointer(Q_VIDEO, 0, 0, 11, 0);
  gtx_set_queue_pointer(Q_AUDIO, 0, 0, 8, 0);

  gtx_set_queue(0, 0, 11);		// video
  gtx_set_queue(1, 0x20000, 8);		// audio

  gtx_set_pid_table(0, 0, 0, PID_V);     // video
  gtx_set_pid_table(1, 0, 0, PID_A);     // audio

  //we want a dual PES stream!
  gtx_set_pid_control_table(0, 2, VIDEO_QUEUE, 1, 0, 0, 1, 0); 
  gtx_set_pid_control_table(1, 2, AUDIO_QUEUE, 1, 0, 0, 1, 0);

  memset(gtxmem, 0, 2*1024*1024);
}

void gtx_dmx_close(void)
{
  int i, j;
  for (j=0; j<2; j++)
    for (i=0; i<16; i++)
      gtx_free_irq(j+2, i);
}
