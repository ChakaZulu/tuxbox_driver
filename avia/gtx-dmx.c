#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/malloc.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/tqueue.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>

#include <ost/demux.h>

#include "dbox/gtx.h"
#include "dbox/gtx-dmx.h"
#include "dbox/avia.h"

// #define DF printk("%s\n", __FUNCTION__);
#define DF

static unsigned char* gtxmem;
static unsigned char* gtxreg;

#ifdef MODULE
MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("GTX DMX Driver");
#endif

static u32 gtx_get_queue_wptr(int queue);
static Pcr_t gtx_read_transport_pcr(void);
static Pcr_t gtx_read_latched_clk(void);
static Pcr_t gtx_read_current_clk(void);
static s32 gtx_calc_diff(Pcr_t clk1, Pcr_t clk2);
static s32 gtx_bound_delta(u32 bound, s32 delta);

static void gtx_task(void *);

struct tq_struct gtx_tasklet=
{
  routine: gtx_task,
  data: 0
};

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

void gtx_set_pid_control_table_section(int entry, int type, int queue, int fork, int cw_offset, int cc, int start_up, int pec, int filt_tab_idx, int no_of_filters)
{
  u8 w[4];
  w[0]=type<<5;
  w[0]|=(queue)&31;
  w[1]=(!!fork)<<7;
  w[1]|=cw_offset<<4;
  w[1]|=cc;
  w[2]=1<<7;
  w[2]|=(!!start_up)<<6;
  w[2]|=(!!pec)<<5;
  w[2]|=filt_tab_idx;
  w[3]=no_of_filters;
  rw(RISC+0x740+entry*4)=*(u32*)w;
}

void gtx_set_filter_definition_table(int entry, int and_or_flag, int filter_param_id)
{
  u8 w[4]={0, 0};
  w[0]=(!!and_or_flag)<<7;
  w[0]|=filter_param_id<<6;
  rh(RISC+0x7C0+entry*2)=*(u16*)w;      // TODO: das stimmt wahrscheinlich nicht.
}

void gtx_set_filter_parameter_table(int entry, u8 mask[8], u8 param[8], int not_flag, int not_flag_ver_id_byte)
{
  u8 w[18];
  int i=0;

  for (; i<8; i++)
  {
    w[i*2]=mask[i];
    w[i*2+1]=param[i];
  }
  w[16]=(!!not_flag)<<4;
  w[16]|=(!!not_flag_ver_id_byte)<<1;
  w[17]=0;
  rh(RISC+0x400+entry*6)=*(u16*)w;
  rh(RISC+0x400+entry*6+2)=*(u16*)(w+2);
  rh(RISC+0x400+entry*6+4)=*(u16*)(w+4);

  rh(RISC+0x500+entry*6)=*(u16*)(w+6);
  rh(RISC+0x500+entry*6+2)=*(u16*)(w+8);
  rh(RISC+0x500+entry*6+4)=*(u16*)(w+10);
  
  rh(RISC+0x600+entry*6)=*(u16*)(w+12);
  rh(RISC+0x600+entry*6+2)=*(u16*)(w+14);
  rh(RISC+0x600+entry*6+4)=*(u16*)(w+16);
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
  u32 wp=-1, oldwp;
  do
  {
    oldwp=wp;
    if (queue>=16)
      rh(CR1)|=0x10;
    else
      rh(CR1)&=~0x10;
    queue &= 0xF;
    wp=rh(QWPnL+4*queue);
    wp|=(rh(QWPnH+4*queue)&63)<<16;
  } while (wp!=oldwp);
  return wp; 
}

#define Q_VIDEO         2               // to check AGAIN and AGAIN...  aber eigentlich sollte das stimmen (versuche zeigen: zumindest Q_VIDEO ist korrekt)
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


__u32 datamask=0;

static void gtx_queue_interrupt(int nr, int bit)
{
  int queue=(nr-2)*16+bit;
  datamask|=1<<queue;
  if (gtx_tasklet.data)
    schedule_task(&gtx_tasklet);
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

extern void avia_set_pcr(u32 hi, u32 lo);

static void gtx_pcr_interrupt(int b, int r)
{
/*      ------> das muss noch rewritten werden. */
  Pcr_t TPpcr;
  Pcr_t latchedClk;
  Pcr_t currentClk;
  s32 delta_PCR_AV;
  
  s32 deltaClk, elapsedTime;
  
  TPpcr=gtx_read_transport_pcr();
  latchedClk=gtx_read_latched_clk();
  currentClk=gtx_read_current_clk();

  if (discont)
  {
    oldClk=currentClk;
    discont=0;
    large_delta_count=0;
    printk("we have a discont:\n");
    printk("write stc: %08x%08x\n", TPpcr.hi, TPpcr.lo);      // wieso LATCHED CLOCK?
    gtx_set_pcr(TPpcr);
    avia_set_pcr(TPpcr.hi-100, TPpcr.lo);
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
  discont=1;
  gtx_free_irq(0, 8);
  gtx_allocate_irq(0, 8, gtx_pcr_interrupt);       // pcr reception
}

int gtx_dmx_init(void)
{
  printk("gtx_dmx: ");
  gtxmem=gtx_get_mem();
  gtxreg=gtx_get_reg();

//  rh(RR1)&=~0x1C;               // take framer, ci, avi module out of reset
  rh(RR0)=0;            // autsch, das muss so. kann das mal wer überprüfen?
  rh(RR1)=0;
  rh(RISCCON)=0;

  rh(FCR)=0x9147;               // byte wide input
  rh(SYNCH)=0x21;

  rh(AVI)=0x71F;
  rh(AVI+2)=0xF;
 
  printk("AVI: %04x %04x\n", rh(AVI), rh(AVI+2));

  return 0;
}

void gtx_dmx_close(void)
{
  int i, j;

  for (j=0; j<2; j++)
    for (i=0; i<16; i++)
      gtx_free_irq(j+2, i);
  gtx_free_irq(0, 8);           // PCR
}
                // nokia api

static void gtx_task(void *data)
{
  gtx_demux_t *gtx=(gtx_demux_t*)data;
  int queue;
  for (queue=0; queue<32; queue++)
    if (datamask&(1<<queue))
    {
      if (gtx->feed[queue].state!=DMX_STATE_GO)
        printk("DEBUG: interrupt on non-GO feed\n!");
      else
      {
        if (gtx->feed[queue].output&TS_PACKET)
        {
          int wptr=gtx_get_queue_wptr(queue);
          int rptr=gtx->feed[queue].readptr;
        
          void *b1, *b2;
          size_t b1l, b2l;
       
          if (wptr>rptr)  // normal case
          {
            b1=gtxmem+rptr;
            b1l=wptr-rptr;
            b2=0;
            b2l=0;
          } else
          {
            b1=gtxmem+rptr;
            b1l=gtx->feed[queue].end-rptr;
            b2=gtxmem+gtx->feed[queue].base;
            b2l=wptr-gtx->feed[queue].base;
          }
        
          switch (gtx->feed[queue].type)
          {
          case DMX_TYPE_TS:
            gtx->feed[queue].cb.ts(b1, b1l, b2, b2l, &gtx->feed[queue].feed.ts, 0); break;
          case DMX_TYPE_SEC:
  //          gtx->feed[queue].cb.sec(b1, b1l, b2, b2l, &gtx->feed[queue].feed.sec, 0); break;
          case DMX_TYPE_PES:
  //          gtx->feed[queue].cb.pes(b1, b1l, b2, b2l, &gtx->feed[queue].feed.pes, 0); break;
          }
          gtx->feed[queue].readptr=wptr;
        }
      }
      datamask&=~(1<<queue);
    }
}

static gtx_demux_filter_t *GtxDmxFilterAlloc(gtx_demux_t *gtx)
{
  int i;
  DF
  for (i=0; i<32; i++)
    if (gtx->filter[i].state==DMX_STATE_FREE)
      break;
  if (i==32)
    return 0;
  gtx->filter[i].state=DMX_STATE_ALLOCATED;
  return &gtx->filter[i];
}

static gtx_demux_feed_t *GtxDmxFeedAlloc(gtx_demux_t *gtx, int type)
{
  int i;
  DF
  
  switch (type)
  {
  case DMX_TS_PES_VIDEO:
    i=VIDEO_QUEUE;
    break;
  case DMX_TS_PES_AUDIO:
    i=AUDIO_QUEUE;
    break;
  case DMX_TS_PES_TELETEXT:
    i=TELETEXT_QUEUE;
    break;
  case DMX_TS_PES_PCR:
  case DMX_TS_PES_SUBTITLE:
    return 0;
  case DMX_TS_PES_OTHER:
    for (i=USER_QUEUE_START; i<LAST_USER_QUEUE; i++)
      if (gtx->feed[i].state==DMX_STATE_FREE)
        break;
                // evtl. system-queue nehmen wenn nix anderes mehr da ist.. 
    if (i==LAST_USER_QUEUE)
      return 0;
    break;
  }

  if (gtx->feed[i].state!=DMX_STATE_FREE)
    return 0;
  gtx->feed[i].state=DMX_STATE_ALLOCATED;
  printk("gtx-dmx: using queue %d for %d\n", i, type);
  return &gtx->feed[i];
}

static int dmx_open(struct dmx_demux_s* demux)
{
  gtx_demux_t *gtx=(gtx_demux_t*)demux;
  DF
  gtx->users++;
  return 0;
}

static int dmx_close (struct dmx_demux_s* demux)
{
  gtx_demux_t *gtx=(gtx_demux_t*)demux;
  DF
  if (!gtx->users)
    return -ENODEV;
  gtx->users--;
  printk("dmx close.\n");
  if (!gtx->users)
  {
    int i;
    // clear resources
    gtx_tasklet.data=0;
    for (i=0; i<32; i++)
      if (gtx->feed[i].state!=DMX_STATE_FREE)
        printk("gtx-dmx.o: LEAK: queue %d used but it shouldn't.\n", i);
  }
  return 0;
}

static int dmx_write (struct dmx_demux_s* demux, const char* buf, size_t count)
{
  DF
  printk("fatal: dmx_write not yet implemented!\n");
  return 0;
}

static void dmx_set_filter(gtx_demux_filter_t *filter)
{
  DF
  printk("SETTING invalid %d, pid %x -> %d (output: %d)\n", filter->invalid, filter->pid, filter->queue, filter->output);
  gtx_set_pid_table(filter->index, filter->wait_pusi, filter->invalid, filter->pid);
  if (filter->type==GTX_FILTER_PID)
    gtx_set_pid_control_table(filter->index, filter->output, filter->queue, filter->fork, filter->cw_offset, filter->cc, filter->start_up, filter->pec);
  else
    printk("not yet supported: sections\n");
///    gtx_set_pid_control_table_section(filter->index, filter->output, filter->queue, filter->fork, filter->cw_offset, filter->cc, filter->start_up, filter->pec, filter->filt_tab_idx, filter->no_of_filters);
}

static int dmx_ts_feed_set(struct dmx_ts_feed_s* feed, __u16 pid, size_t callback_length, size_t circular_buffer_size, int descramble, struct timespec timeout)
{
  gtx_demux_feed_t *gtxfeed=(gtx_demux_feed_t*)feed;
  gtx_demux_filter_t *filter=gtxfeed->filter;
  
  DF
  
  if (pid>0x1FFF)
    return -EINVAL;
    
  gtxfeed->pid=pid;

  filter->pid=pid;
  printk("filtering pid %d\n", pid);
  filter->wait_pusi=0;  // right?

  printk("feed output: %x\n", gtxfeed->output);
  if (gtxfeed->output&TS_PCR)
  {
    printk("setting PCR-pid to %x\n", pid);
    gtx_dmx_set_time_source(pid);
  }
  if (gtxfeed->pes_type==DMX_TS_PES_VIDEO)
  {
    printk("assuming PCR_PID == VPID == %04x\n", pid);
    gtx_dmx_set_time_source(pid);
  }

  filter->type=GTX_FILTER_PID;

  if (gtxfeed->output&TS_DECODER)
    gtxfeed->output|=TS_PAYLOAD_ONLY;   // weil: wir haben dual-pes

  if (gtxfeed->output&TS_PAYLOAD_ONLY)
    filter->output=GTX_OUTPUT_PESPAYLOAD;
  else
    filter->output=GTX_OUTPUT_TS;
  
  filter->queue=gtxfeed->index;

  filter->invalid=1;
  filter->fork=0;
  filter->cw_offset=0;
  filter->cc=0;
  filter->start_up=0;
  filter->pec=0;
  
  dmx_set_filter(gtxfeed->filter);
  
  gtxfeed->state=DMX_STATE_READY;
  
  return 0;
}

static int dmx_ts_feed_start_filtering(struct dmx_ts_feed_s* feed)
{
  gtx_demux_feed_t *gtxfeed=(gtx_demux_feed_t*)feed;
  gtx_demux_filter_t *filter=gtxfeed->filter;
  
  DF
  
  if (gtxfeed->state!=DMX_STATE_READY)
  {
    printk("feed not DMX_STATE_READY\n");
    return -EINVAL;
  }
  if (gtxfeed->type!=DMX_TYPE_TS)
  {
    printk("feed not DMX_TYPE_TS\n");
    return -EINVAL;
  }

  filter->start_up=1;
  filter->invalid=0;
  dmx_set_filter(gtxfeed->filter);
  feed->is_filtering=1;
  
  gtxfeed->readptr=gtx_get_queue_wptr(gtxfeed->index);
  
  printk("STARTING filtering, pid %d\n", gtxfeed->pid);

  gtx_allocate_irq(2+!!(gtxfeed->index&16), gtxfeed->index&15, gtx_queue_interrupt);
  gtxfeed->state=DMX_STATE_GO;
  return 0;
}

static int dmx_ts_feed_set_type(struct dmx_ts_feed_s* feed, int type, dmx_ts_pes_t pes_type)
{
  gtx_demux_feed_t *gtxfeed=(gtx_demux_feed_t*)feed;
  printk("dmx_ts_feed_set_type(%d, %d)\n", type, pes_type);
  return 0;
}

static int dmx_ts_feed_stop_filtering(struct dmx_ts_feed_s* feed)
{
  gtx_demux_feed_t *gtxfeed=(gtx_demux_feed_t*)feed;
  gtx_demux_filter_t *filter=gtxfeed->filter;
  DF
  filter->invalid=1;
  dmx_set_filter(gtxfeed->filter);
  
  feed->is_filtering=0;
  gtx_free_irq(2+!!(gtxfeed->index&16), gtxfeed->index&15);
  gtxfeed->state=DMX_STATE_ALLOCATED;
  return 0;  
}

static int dmx_allocate_ts_feed (struct dmx_demux_s* demux, dmx_ts_feed_t** feed, dmx_ts_cb callback, int type, dmx_ts_pes_t pes_type)
{
  gtx_demux_t *gtx=(gtx_demux_t*)demux;
  gtx_demux_feed_t *gtxfeed;
  DF
  if (!(gtxfeed=GtxDmxFeedAlloc(gtx, pes_type)))
  {
    printk("couldn't get gtx feed\n");
    return -EBUSY;
  }

  gtxfeed->type=DMX_TYPE_TS;
  gtxfeed->cb.ts=callback;
  gtxfeed->demux=gtx;
  gtxfeed->pid=0xFFFF;
  // peslen
  
  *feed=&gtxfeed->feed.ts;
  (*feed)->is_filtering=0;
  (*feed)->parent=demux;
  (*feed)->priv=0;
  (*feed)->set=dmx_ts_feed_set;
  (*feed)->start_filtering=dmx_ts_feed_start_filtering;
  (*feed)->stop_filtering=dmx_ts_feed_stop_filtering;
  (*feed)->set_type=dmx_ts_feed_set_type;

  gtxfeed->pes_type=pes_type;
  gtxfeed->output=type;
  
  if (!(gtxfeed->filter=GtxDmxFilterAlloc(gtx)))
  {
    printk("couldn't get gtx filter\n");
    gtxfeed->state=DMX_STATE_FREE;
    return -EBUSY;
  }
  
  gtxfeed->filter->type=DMX_TYPE_TS;
  gtxfeed->filter->feed=gtxfeed;
  gtxfeed->filter->state=DMX_STATE_READY;
  return 0;
}

static int dmx_release_ts_feed (struct dmx_demux_s* demux, dmx_ts_feed_t* feed)
{
  gtx_demux_feed_t *gtxfeed=(gtx_demux_feed_t*)feed;
  DF
  if (gtxfeed->state==DMX_STATE_FREE)
    return -EINVAL;
  // buffer.. ne, eher nicht.
  gtxfeed->state=DMX_STATE_FREE;
  gtxfeed->filter->state=DMX_STATE_FREE;
  // pid austragen
  gtxfeed->pid=0xFFFF;
  return 0;
}

static int dmx_allocate_pes_feed (struct dmx_demux_s* demux, dmx_pes_feed_t** feed, dmx_pes_cb callback)
{
  return -EINVAL;
}

static int dmx_release_pes_feed (struct dmx_demux_s* demux, dmx_pes_feed_t* feed)
{
  return -EINVAL;
}
#if 0
static int dmx_section_feed_allocate_filter (struct dmx_section_feed_s* feed, dmx_section_filter** filter)
{
  gtx_demux_feed_t *gtxfeed=(gtx_demux_feed_t*)feed;
  gtx_demux_t *gtx=gtxfeed->demux;
  gtx_demux_filter_t *gtxfilter;

  return 0;
}

static int dmx_section_feed_set(struct dmx_section_feed_s* feed,
                     __u16 pid, size_t circular_buffer_size,
                     int descramble, int check_crc)
{
  gtx_demux_feed_t *gtxfeed=(gtx_demux_feed_t*)feed;
  gtx_demux_filter_t *filter=gtxfeed->filter;
  
  DF
  
  if (pid>0x1FFF)
    return -EINVAL;
    
  gtxfeed->pid=pid;

  filter->pid=pid;
  printk("SEC: filtering pid %d\n", pid);
  
  if (!gtxfeed->filter)
  {
    printk("no filter set.\n");
  }
  dmx_set_filter(gtxfeed->filter);
  if (gtxfeed->filter->
  gtxfeed->state=DMX_STATE_READY;
  
  return 0;
}
#endif

static int dmx_allocate_section_feed (struct dmx_demux_s* demux, dmx_section_feed_t** feed, dmx_section_cb callback)
{
#if 0
  gtx_demux_t *gtx=(gtx_demux_t*)demux;
  gtx_demux_feed_t *gtxfeed;
  DF
  if (!(gtxfeed=GtxDmxFeedAlloc(gtx, ...)))
  {
    printk("couldn't get gtx feed (for section_feed)\n");
    return -EBUSY;
  }

  gtxfeed->type=DMX_TYPE_SEC;
  gtxfeed->cb.sec=callback;
  gtxfeed->demux=gtx;
  gtxfeed->pid=0xFFFF;
  gtxfeed->secfilter=0;
  
  *feed=&gtxfeed->feed.sec;
  (*feed)->is_filtering=0;
  (*feed)->parent=demux;
  (*feed)->priv=0;
  (*feed)->set=dmx_section_feed_set;
  (*feed)->allocate_filter=dmx_section_feed_start_filtering;
  (*feed)->release_filter=dmx_section_feed_start_filtering;
  (*feed)->start_filtering=dmx_section_feed_start_filtering;
  (*feed)->stop_filtering=dmx_section_feed_stop_filtering;
  
  if (!(gtxfeed->filter=GtxDmxFilterAlloc(gtx)))
  {
    printk("couldn't get gtx filter\n");
    gtxfeed->state=DMX_STATE_FREE;
    return -EBUSY;
  }
  
  gtxfeed->filter->type=DMX_TYPE_TS;
  gtxfeed->filter->feed=gtxfeed;
  gtxfeed->filter->state=DMX_STATE_READY;
  return 0;
#endif  
  return -EINVAL;
}

static int dmx_release_section_feed (struct dmx_demux_s* demux,  dmx_section_feed_t* feed)
{
  return -EINVAL;
}

static int dmx_add_frontend (struct dmx_demux_s* demux, dmx_frontend_t* frontend)
{
  gtx_demux_t *gtx=(gtx_demux_t*)demux;
  struct list_head *pos, *head=&gtx->frontend_list;
  DF
  if (!(frontend->id && frontend->vendor && frontend->model))
    return -EINVAL;
  list_for_each(pos, head)
  {
    if (!strcmp(DMX_FE_ENTRY(pos)->id, frontend->id))
      return -EEXIST;
  }
  list_add(&(frontend->connectivity_list), head);
  return 0;
}

static int dmx_remove_frontend (struct dmx_demux_s* demux,  dmx_frontend_t* frontend)
{
  gtx_demux_t *gtx=(gtx_demux_t*)demux;
  struct list_head *pos, *head=&gtx->frontend_list;
  DF
  list_for_each(pos, head)
  {
    if (DMX_FE_ENTRY(pos)==frontend)
    {
      list_del(pos);
      return 0;
    }
  }
  return -ENODEV;
}

static struct list_head* dmx_get_frontends (struct dmx_demux_s* demux)
{
  gtx_demux_t *gtx=(gtx_demux_t*)demux;
  DF
  if (list_empty(&gtx->frontend_list))
    return 0;
  return &gtx->frontend_list;
}

static int dmx_connect_frontend (struct dmx_demux_s* demux, dmx_frontend_t* frontend)
{
  DF
  if (demux->frontend)
    return -EINVAL;
  demux->frontend=frontend;
  return -EINVAL;       // was soll das denn? :)
}

static int dmx_disconnect_frontend (struct dmx_demux_s* demux)
{
  DF
  demux->frontend=0;
  return -EINVAL;
}

int GtxDmxInit(gtx_demux_t *gtxdemux, void *priv, char *id, char *vendor, char *model)
{
  dmx_demux_t *dmx=&gtxdemux->dmx;
  int i;
  DF
  gtxdemux->users=0;
  
  gtxdemux->frontend_list.next=
    gtxdemux->frontend_list.prev=
      &gtxdemux->frontend_list;

  for (i=0; i<NUM_PID_FILTER; i++)      // disable all pid filters
  {
    gtx_set_pid_table(i, 0, 1, 0);
    gtx_set_queue(i, 0x20000*i, 11);
  }
  
  for (i=0; i<NUM_QUEUES; i++)
    rh(QI0+i*2)=0;            // das geht irgendwie nicht :(

  gtx_dmx_set_time_source(0xFF);                // DEBUG
  
  for (i=0; i<NUM_QUEUES; i++)
  {
    gtxdemux->feed[i].size=64*1024;             // geht atm nicht anders
    gtxdemux->feed[i].base=i*0x10000;
    gtxdemux->feed[i].end=gtxdemux->feed[i].base+gtxdemux->feed[i].size;
    //    gtx_queue[i].base=gtx_allocate_dram(gtx_queue[i].size, gtx_queue[i].size);
    gtx_set_queue(i, gtxdemux->feed[i].base, 10);

    gtxdemux->feed[i].index=i;
    gtxdemux->feed[i].state=DMX_STATE_FREE;
  }

  for (i=0; i<32; i++)
  {
    gtxdemux->filter[i].index=i;
    gtxdemux->filter[i].state=DMX_STATE_FREE;
  }

  gtx_set_queue_pointer(Q_VIDEO, gtxdemux->feed[VIDEO_QUEUE].base, gtxdemux->feed[VIDEO_QUEUE].base, 10, 0);        // set system queues
  gtx_set_queue_pointer(Q_AUDIO, gtxdemux->feed[AUDIO_QUEUE].base, gtxdemux->feed[AUDIO_QUEUE].base, 10, 0);
  gtx_set_queue_pointer(Q_TELETEXT, gtxdemux->feed[TELETEXT_QUEUE].base, gtxdemux->feed[TELETEXT_QUEUE].base, 10, 0);

  dmx->id=id;
  dmx->vendor=vendor;
  dmx->model=model;
  dmx->frontend=0;
  dmx->reg_list.next=dmx->reg_list.prev=&dmx->reg_list;
  dmx->priv=(void *) gtxdemux;
  dmx->open=dmx_open;
  dmx->close=dmx_close;
  dmx->write=dmx_write;
  dmx->allocate_ts_feed=dmx_allocate_ts_feed;
  dmx->release_ts_feed=dmx_release_ts_feed;
  dmx->allocate_pes_feed=dmx_allocate_pes_feed;
  dmx->release_pes_feed=dmx_release_pes_feed;
  dmx->allocate_section_feed=dmx_allocate_section_feed;
  dmx->release_section_feed=dmx_release_section_feed;
  
  dmx->descramble_mac_address=0;
  dmx->descramble_section_payload=0;
  
  dmx->add_frontend=dmx_add_frontend;
  dmx->remove_frontend=dmx_remove_frontend;
  dmx->get_frontends=dmx_get_frontends;
  dmx->connect_frontend=dmx_connect_frontend;
  dmx->disconnect_frontend=dmx_disconnect_frontend;
  
  gtx_tasklet.data=gtxdemux;

  if (dmx_register_demux(dmx)<0)
    return -1;
  if (dmx->open(dmx)<0)
    return -1;
  return 0;
}

#ifdef MODULE

int init_module(void)
{
  return gtx_dmx_init();
}

void cleanup_module(void)
{ 
  printk("dmx: close\n");
  gtx_dmx_close();
}

EXPORT_SYMBOL(cleanup_module);
EXPORT_SYMBOL(GtxDmxInit);

#endif
