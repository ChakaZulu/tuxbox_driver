/*
 *	 avia_gt_napi.c - AViA GTX demux driver (dbox-II-project)
 *
 *	 Homepage: http://dbox2.elxsi.de
 *
 *	 Copyright (C) 2000-2001 Felix "tmbinc" Domke (tmbinc@gmx.net)
 *
 *	 This program is free software; you can redistribute it and/or modify
 *	 it under the terms of the GNU General Public License as published by
 *	 the Free Software Foundation; either version 2 of the License, or
 *	 (at your option) any later version.
 *
 *	 This program is distributed in the hope that it will be useful,
 *	 but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 *	 GNU General Public License for more details.
 *
 *	 You should have received a copy of the GNU General Public License
 *	 along with this program; if not, write to the Free Software
 *	 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *   $Revision: 1.96 $
 *   $Log: avia_gt_napi.c,v $
 *   Revision 1.96  2002/09/02 20:56:06  Jolt
 *   - HW section fix (GTX)
 *   - DMX/NAPI cleanups
 *
 *   Revision 1.95  2002/09/02 19:25:37  Jolt
 *   - DMX/NAPI cleanup
 *   - Compile fix
 *
 *   Revision 1.94  2002/09/01 17:50:51  wjoost
 *   I don't like #ifdef :-(
 *
 *   Revision 1.93  2002/08/25 09:38:26  wjoost
 *   Hardware Section Filtering
 *
 *   Revision 1.92  2002/08/24 09:36:07  Jolt
 *   Merge
 *
 *   Revision 1.91  2002/08/24 09:28:20  Jolt
 *   Compile fix
 *
 *   Revision 1.90  2002/08/22 13:39:33  Jolt
 *   - GCC warning fixes
 *   - screen flicker fixes
 *   Thanks a lot to Massa
 *
 *   Revision 1.89  2002/06/11 20:35:43  Jolt
 *   Sections cleanup
 *
 *   Revision 1.88  2002/05/07 16:59:19  Jolt
 *   Misc stuff and cleanups
 *
 *   Revision 1.87  2002/05/06 12:58:37  Jolt
 *   obi[TM] fix 8-)
 *
 *   Revision 1.86  2002/05/06 02:18:18  obi
 *   cleanup for new kernel
 *
 *   Revision 1.85  2002/05/05 19:58:13  Jolt
 *   Doh 8-(
 *
 *   Revision 1.84  2002/05/04 17:05:53  Jolt
 *   PCR PID workaround
 *
 *   Revision 1.83  2002/05/03 17:06:44  obi
 *   replaced r*() by gtx_reg_()
 *
 *   Revision 1.82  2002/05/02 12:37:35  Jolt
 *   Merge
 *
 *   Revision 1.81  2002/05/02 04:56:47  Jolt
 *   Merge
 *
 *   Revision 1.80  2002/05/01 21:51:35  Jolt
 *   Merge
 *
 *   Revision 1.79  2002/04/22 17:40:01  Jolt
 *   Major cleanup
 *
 *   Revision 1.78  2002/04/19 11:31:53  Jolt
 *   Added missing module init stuff
 *
 *   Revision 1.77  2002/04/19 11:28:26  Jolt
 *   Final DMX merge
 *
 *   Revision 1.76  2002/04/19 11:02:43  obi
 *   build fix
 *
 *   Revision 1.75  2002/04/19 10:07:27  Jolt
 *   DMX merge
 *
 *   Revision 1.74  2002/04/18 18:17:37  happydude
 *   deactivate pcr pid failsafe
 *
 *   Revision 1.73  2002/04/14 18:06:19  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.72  2002/04/13 23:19:05  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.71  2002/04/12 23:20:25  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.70  2002/04/12 14:28:13  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.69  2002/03/19 18:32:25  happydude
 *   allow seperate setting of pcr pid
 *
 *   Revision 1.68  2002/02/24 15:29:23  woglinde
 *   test new tuner-api
 *
 *   Revision 1.66.2.1  2002/02/09 20:44:01  TripleDES
 *   fixes
 *
 *   CV: ----------------------------------------------------------------------
 *
 *   Revision 1.66  2002/01/18 14:48:52  tmbinc
 *   small fix for multiple pid streaming
 *
 *   Revision 1.65  2002/01/02 19:45:50  tmbinc
 *   added support for streaming a pid to multiple clients.
 *
 *   Revision 1.64  2002/01/02 04:40:08  McClean
 *   make OUT OF SYNC dprintf
 *
 *   Revision 1.63  2001/12/19 13:19:17  derget
 *   debugoutput entfernt
 *   CHCH [DEMUX] START
 *   CHCH [DEMUX] STOP
 *   wer braucht das schon ..
 *
 *   Revision 1.62  2001/12/17 19:29:51  gillem
 *   - sync with includes
 *
 *   Revision 1.61  2001/12/01 06:37:06  gillem
 *   - malloc.h -> slab.h
 *
 *   Revision 1.60  2001/11/14 17:59:22  wjoost
 *   Section-Empfang geaendert (Pruefung auf maximale Groesse, zusammenhaengende TS-Pakete)
 *
 *	 Revision 1.55	2001/09/02 01:28:34	TripleDES
 *	 -small fix (dac)
 *
 *	 Revision 1.54	2001/09/02 01:16:42	TripleDES
 *	 -more fixes (corrects my wrong commit)
 *
 *	 Revision 1.52	2001/08/18 18:59:40	tmbinc
 *	 fixed init
 *
 *	 Revision 1.51	2001/08/18 18:20:21	TripleDES
 *	 moved the ucode loading to dmx
 *
 *	 Revision 1.50	2001/08/15 14:57:44	tmbinc
 *	 fixed queue-reset
 *
 *	 Revision 1.49	2001/07/15 17:08:45	Toerli
 *	 Flimmern bei Sagem beseitigt
 *
 *	 Revision 1.48	2001/06/25 22:27:22	gillem
 *	 - start autodetect
 *
 *	 Revision 1.47	2001/06/22 00:03:42	tmbinc
 *	 fixed aligning of queues, system queues
 *
 *	 Revision 1.46	2001/06/18 20:30:59	tmbinc
 *	 decent buffersizes. change for your needs.
 *
 *	 Revision 1.45	2001/06/18 20:14:08	tmbinc
 *	 fixed state sets in sections, now it works as expected (multiple filter on one pid)
 *
 *	 Revision 1.44	2001/06/15 00:17:26	TripleDES
 *	 fixed queue-reset problem - solves zap problem with enx
 *
 *	 Revision 1.43	2001/04/28 22:43:13	fnbrd
 *	 Added fix from tmbinc. ;)
 *
 *	 Revision 1.42	2001/04/24 17:54:14	tmbinc
 *	 fixed 188bytes-on-callback bug
 *
 *	 Revision 1.41	2001/04/22 13:56:35	tmbinc
 *	 other philips- (and maybe sagem?) fixes
 *
 *	 Revision 1.40	2001/04/21 13:08:57	tmbinc
 *	 eNX now works on philips.
 *
 *	 Revision 1.39	2001/04/21 10:57:41	tmbinc
 *	 small fix.
 *
 *	 Revision 1.38	2001/04/21 10:40:13	tmbinc
 *	 fixes for eNX
 *
 *	 Revision 1.37	2001/04/19 02:14:43	tmbinc
 *	 renamed gtx-dmx.c to gen-dmx.c
 *
 *
 *	 old log: gtx-dmx.c,v
 *	 Revision 1.36	2001/04/10 03:07:29	Hunz
 *	 1st nokia/sat fix - supported by Wodka Gorbatschow *oerks* ;)
 *
 *	 Revision 1.35	2001/04/09 23:26:42	TripleDES
 *	 some changes
 *
 *	 Revision 1.34	2001/04/08 22:22:29	TripleDES
 *	 added eNX support
 *	 -every register/ucode access is temporarily duplicated for eNX testing - will be cleared soon ;)
 *	 -up to now there is no section support for eNX
 *	 -need to rewrite the register-defines gReg for gtx, eReg for enx (perhaps/tmb?)
 *	 -queue-interrupts are not correct for eNX
 *	 -uncomment the "#define enx_dmx" for testing eNX
 *
 *	 Revision 1.33	2001/04/08 02:05:40	tmbinc
 *	 made it more modular, this time the demux. dvb.c not anymore dependant on
 *	 the gtx.
 *
 *	 Revision 1.32	2001/03/30 01:19:55	tmbinc
 *	 Fixed multiple-section bug.
 *
 *	 Revision 1.31	2001/03/27 14:41:49	tmbinc
 *	 CRC check now optional.
 *
 *	 Revision 1.30	2001/03/21 15:30:25	tmbinc
 *	 Added SYNC-delay for avia, resulting in faster zap-time.
 *
 *	 Revision 1.29	2001/03/19 17:48:32	tmbinc
 *	 re-fixed a fixed fix by gillem.
 *
 *	 Revision 1.28	2001/03/19 16:24:32	tmbinc
 *	 fixed section parsing bugs.
 *
 *	 Revision 1.27	2001/03/16 19:50:28	gillem
 *	 - fix section parser
 *
 *	 Revision 1.26	2001/03/16 17:49:56	gillem
 *	 - fix section parser
 *
 *	 Revision 1.25	2001/03/15 15:56:26	gillem
 *	 - fix dprintk output
 *
 *	 Revision 1.24	2001/03/14 21:42:48	gillem
 *	 - fix bugs in section parsing
 *	 - add crc32 check in section parsing
 *
 *	 Revision 1.23	2001/03/12 22:32:01	gillem
 *	 - test only ... sections not work
 *
 *	 Revision 1.22	2001/03/11 22:58:09	gillem
 *	 - fix ts parser
 *
 *	 Revision 1.21	2001/03/11 21:34:29	gillem
 *	 - fix af parser
 *
 *	 Revision 1.20	2001/03/10 02:46:14	tmbinc
 *	 Fixed section support.
 *
 *	 Revision 1.19	2001/03/10 00:41:21	tmbinc
 *	 Fixed section handling.
 *
 *	 Revision 1.18	2001/03/09 22:10:20	tmbinc
 *	 Completed first table support (untested)
 *
 *	 Revision 1.17	2001/03/09 20:48:31	gillem
 *	 - add debug option
 *
 *	 Revision 1.16	2001/03/07 22:25:14	tmbinc
 *	 Tried to fix PCR.
 *
 *	 Revision 1.15	2001/03/04 14:15:42	tmbinc
 *	 fixed ucode-version autodetection.
 *
 *	 Revision 1.14	2001/03/04 13:03:17	tmbinc
 *	 Removed %188 bytes check (for PES)
 *
 *	 Revision 1.13	2001/02/27 14:15:22	tmbinc
 *	 added sections.
 *
 *	 Revision 1.12	2001/02/17 01:19:19	tmbinc
 *	 fixed DPCR
 *
 *	 Revision 1.11	2001/02/11 16:01:06	tmbinc
 *	 *** empty log message ***
 *
 *	 Revision 1.10	2001/02/11 15:53:25	tmbinc
 *	 section filtering (not yet working)
 *
 *	 Revision 1.9	2001/02/10 14:31:52	gillem
 *	 add GtxDmxCleanup function
 *
 *	 Revision 1.8	2001/01/31 17:17:46	tmbinc
 *	 Cleaned up avia drivers. - tmb
 *
 *	 last (old) Revision: 1.36
 *
 */

/*
		This driver implements the Nokia-DVB-Api (Kernel level Demux driver),
		but it isn't yet complete.

		It does not support descrambling (and some minor features as well).

		Writing isn't supported, either.
 */
#define __KERNEL_SYSCALLS__

#include <linux/string.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <linux/fcntl.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
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

#include <dbox/avia_gt.h>
#include <dbox/avia_gt_dmx.h>
#include <dbox/avia_gt_napi.h>
#include "crc32.c"

static sAviaGtInfo *gt_info = (sAviaGtInfo *)NULL;
static unsigned char auto_pcr_pid = 0;

#define GTX_SECTIONS
//#undef dprintk
//#define dprintk printk


#ifdef MODULE
MODULE_PARM(auto_pcr_pid, "i");
MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("Avia eNX/GTX demux driver");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#endif

static gtx_demux_t gtx;

static void gtx_task(void *);
static int GtxDmxInit(gtx_demux_t *gtxdemux);
static int GtxDmxCleanup(gtx_demux_t *gtxdemux);
static void dmx_set_filter(gtx_demux_filter_t *filter);

struct tq_struct gtx_tasklet=
{
	routine: gtx_task,
	data: 0
};

const int buffersize[32]=		// sizes are 1<<x*64bytes. BEWARE OF THE ALIGNING!
														// DO NOT CHANGE UNLESS YOU KNOW WHAT YOU'RE DOING!
	{10,	// video
	9,	// audio
	9,	// teletext
	10, 10, 10, 10, 10,		// user 3..7
	8, 8, 8, 8, 8, 8, 8, 8,	// user 8..15
	8, 8, 8, 8, 7, 7, 7, 7,	// user 16..23
	7, 7, 7, 7, 7, 7, 7, 7	// user 24..31
	};		// 512kb total + (a little bit more ;)

#if 0
static unsigned short rb[4096], rrb[32];
#endif

#if 0
		/* hehe da kapiert ja eh niemand was das soll, ausser willid und TripleDES vielleicht, also kanns hier auch bleiben :) */
		/* man beachte: hier spionieren wir geheimste C-cube infos aus, indem wir auf undokumentierte register zugreifen!      */
void enx_tdp_trace(void)
{
	int i, oldpc=-1;
	unsigned short *r=(unsigned short*)enx_reg_o(TDP_INSTR_RAM);
	enx_reg_16(EC) = 0x03;			//start tdp
	memcpy(rb, r, 4096*2);
	memset(rrb, 0x76, 32*2);
	for (i=0; i<1000; i++)
	{
		int j;
		int pc=enx_reg_16(EPC);
		for (j=0; j<4096; j++)
			if (rb[j]!=r[j])
			{
				printk("(%04x: %04x -> %04x)\n", j, rb[j], r[j]);
				rb[j]=r[j];
			}
		if (pc!=oldpc)
		{
			int a;

			for (a=0; a<32; a++)
			{
				int tr=enx_reg_16(EPC-32+a*2);
				if (rrb[a]!=tr)
					printk("%04x ", enx_reg_16(EPC-32+a*2));
				else
					printk("     ");
				rrb[a]=tr;
			}
			printk("\n");
			printk("%03x (%04x) %x\n", pc, r[pc], (r[pc]>>3)&0xFF);
		}
		oldpc=pc;
		enx_reg_16(EC) = 0x03;			// und wieder nen stueck...
	}
	enx_reg_16(EC)=0;
}
#endif

#define Q_VIDEO				 2
#define Q_AUDIO				 0
#define Q_TELETEXT			1

void gtx_reset_queue(gtx_demux_feed_t *feed)
{

	feed->readptr = avia_gt_dmx_get_queue_write_pointer(feed->index);

	avia_gt_dmx_set_queue_write_pointer(feed->index, feed->readptr);
	//avia_gt_dmx_set_queue_read_pointer(feed->index, feed->readptr);

}

static volatile __u32 datamask=0;

static void gtx_queue_interrupt(unsigned short irq)
{

    unsigned char nr = AVIA_GT_IRQ_REG(irq);
    unsigned char bit = AVIA_GT_IRQ_BIT(irq);
    int queue = (int)0;

    if (avia_gt_chip(ENX)) {

	if (nr==3)
		queue=bit+16;
	else if (nr==4)
		queue=bit+1;
	else if (nr==5)
		queue=bit-6;
	else
	{
		printk("unexpected enx queue interrupt %d:%d", nr, bit);
		return;
	}

    } else if (avia_gt_chip(GTX)) {

	queue=(nr-2)*16+bit;

    }

	set_bit(queue,&datamask);

	if (gtx_tasklet.data)
		schedule_task(&gtx_tasklet);
		
}

extern int register_demux(struct dmx_demux_s *demux);
extern int unregister_demux(struct dmx_demux_s *demux);

void gtx_dmx_close(void)
{

	u8 queue_nr;

	for (queue_nr = 0; queue_nr < 32; queue_nr++)
		avia_gt_free_irq(avia_gt_dmx_get_queue_irq(queue_nr));

	if (avia_gt_chip(ENX))
		avia_gt_free_irq(ENX_IRQ_PCR);
	else if (avia_gt_chip(GTX))
		avia_gt_free_irq(GTX_IRQ_PCR);					

	unregister_demux(&gtx.dmx);
	GtxDmxCleanup(&gtx);

}

#if 0
static void dump(unsigned char *b1, unsigned b1l, unsigned char *b2, unsigned b2l)
{
	unsigned i = 0;

	while (b1l + b2l > 0)
	{
		if (b1l)
		{
			printk("%02X ",*b1 & 0xFF);
			b1l--;
			b1++;
		}
		else
		{
			printk("%02X ",*b2 & 0xFF);
			b2l--;
			b2++;
		}
		i++;
		if ( (i & 0x0F) == 0)
		{
			printk("\n");
		}
	}
	printk("\n");
}
#endif

								// nokia api

static int gtx_handle_section(gtx_demux_feed_t *gtxfeed)
{
	gtx_demux_secfilter_t *secfilter = (gtx_demux_secfilter_t *)NULL;
	int ok,i;

#ifdef GTX_SECTIONS
	int recover = 0;
#endif

	if (gtxfeed->sec_recv != gtxfeed->sec_len)
	{
		dprintk("gtx_dmx: have: %d, want %d\n", gtxfeed->sec_recv, gtxfeed->sec_len);
	}

	if (!gtxfeed->sec_recv)
	{
		gtxfeed->sec_len=gtxfeed->sec_recv=0;
		return 0;
	}

	/*
	 * linux_dvb_api.pdf, allocate_filter():
	 * [..] Note that on most demux hardware it is not possible to filter on
	 * the section length field of the section header thus this field is
	 * ignored, even though it is included in filter value and filter mask
	 * fields.
	 */

	for (secfilter=gtxfeed->secfilter; secfilter; secfilter=secfilter->next)
	{
		ok = 1;
		i = 0;
		while ( (i < DMX_MAX_FILTER_SIZE) && ok)
		{
			if ( i == 1 )
			{
				i = 3;
			}
			if ( ((gtxfeed->sec_buffer[i]^secfilter->filter.filter_value[i])&secfilter->filter.filter_mask[i]) )
			{
				ok=0;
			}
			else
			{
				i++;
			}
		}

		if (ok)
		{
#ifdef GTX_SECTIONS
			recover++;
#endif
			if ( (!gtxfeed->check_crc) || (crc32(gtxfeed->sec_buffer, gtxfeed->sec_len) == 0) )
				gtxfeed->cb.sec(gtxfeed->sec_buffer, gtxfeed->sec_len, 0, 0, &secfilter->filter, 0);
			else
				dprintk("gtx_dmx: CRC Problem !!!\n");
		}
#ifdef GTX_SECTIONS
		else if ( i >= 10)
		{
			recover++;
		}
#endif
	}

	gtxfeed->sec_len=gtxfeed->sec_recv=0;

#ifdef GTX_SECTIONS
	return recover;
#else
	return 0;
#endif

}

static void gtx_task(void *data)
{
	gtx_demux_t *gtx=(gtx_demux_t*)data;
	int queue = (int)0;
	int ccn = (int)0;
#ifdef GTX_SECTIONS
	static char sync_lost = 0;
#endif

	if (datamask == 0)
		return;

	for (queue=31; datamask && queue>= 0; queue--)	// msg queue must have priority
		if (test_bit(queue,&datamask))
		{
			gtx_demux_feed_t *gtxfeed=gtx->feed+queue;

			if (gtxfeed->state!=DMX_STATE_GO)
			{
				dprintk("gtx_dmx: DEBUG: interrupt on non-GO feed, queue %d\n!",queue);
			}
			else
			{
//				if (gtxfeed->output&TS_PACKET)
				{
					int wptr = (int)0;
					int rptr = (int)0;
					int i = (int)0;

					__u8 *b1 = (__u8 *)NULL, *b2 = (__u8 *)NULL;
					size_t b1l = (size_t)0, b2l = (size_t)0;

					wptr = avia_gt_dmx_get_queue_write_pointer(queue);
					rptr = gtxfeed->readptr;

					// can happen if a queue has been reset but an interrupt is pending
					if (wptr == rptr)
					{
						clear_bit(queue,&datamask);
						continue;
					}

					if (wptr < gtxfeed->base)
					{
						printk("avia_gt_napi: wptr < base (is: %x, base is %x, queue %d)!\n", wptr, gtxfeed->base, queue);
						break;
					}
					if (wptr >= (gtxfeed->base+gtxfeed->size))
					{
						printk("avia_gt_napi: wptr out of bounds! (is %x)\n", wptr);
						break;
					}

					b1=gt_info->mem_addr+rptr;

					if (wptr>rptr)	// normal case
					{
						b1l=wptr-rptr;
						b2=0;
						b2l=0;
					} else
					{
						b1l=gtx->feed[queue].end-rptr;
						b2=gt_info->mem_addr+gtx->feed[queue].base;
						b2l=wptr-gtx->feed[queue].base;
					}

					if (!(gtxfeed->output&TS_PAYLOAD_ONLY))		// nur bei TS auf sync achten
					{
						int rlen=b1l+b2l;
						if (*b1 != 0x47)
						{
							dprintk("OUT OF SYNC. -> %x\n", *(__u32*)b1);
							gtx->feed[queue].readptr=wptr;
							continue;
						}
						rlen-=rlen%188;
						if (!rlen)
						{
							dprintk("this SHOULDN'T happen (tm) (incomplete packet)\n");
							continue;
						}
						if (rlen<=b1l)
						{
							b1l=rlen;
							b2l=0;
							gtxfeed->readptr=rptr+b1l;
							if (gtxfeed->readptr==gtxfeed->end)
								gtxfeed->readptr=gtxfeed->base;
						} else
						{
							b2l=rlen-b1l;
							gtxfeed->readptr=gtxfeed->base+b2l;
						}
					} else
						gtx->feed[queue].readptr=wptr;

					switch (gtxfeed->type)
					{
						// handle TS
						case DMX_TYPE_TS:
							for (i=USER_QUEUE_START; i<LAST_USER_QUEUE; i++)
								if ((gtx->feed[i].state==DMX_STATE_GO) &&
										(gtx->feed[i].pid==gtxfeed->pid) &&
										(gtx->feed[i].output&TS_PACKET) &&
										(!((gtxfeed->output^gtx->feed[i].output)&TS_PAYLOAD_ONLY)))
									gtx->feed[i].cb.ts(b1, b1l, b2, b2l, &gtx->feed[i].feed.ts, 0);
							break;


#ifdef GTX_SECTIONS
						// handle message from dmx
						case DMX_TYPE_MESSAGE:
						{
							sCC_ERROR_MESSAGE msg;
							sSECTION_COMPLETED_MESSAGE comp_msg;
							sPRIVATE_ADAPTION_MESSAGE adaption;
							u8 type;
							unsigned len,len2;
							unsigned i;
							__u32 blocked = 0;
							while (b1l + b2l > 0)
							{
								if (b1l)
								{
									type = *(b1++);
									b1l--;
								}
								else
								{
									type = *(b2++);
									b2l--;
								}
								if (type == DMX_MESSAGE_CC_ERROR)
								{
									sync_lost = 0;
									msg.type = type;
									if (b1l + b2l < sizeof(msg) - 1)
									{
										printk("avia_gt_napi: short CC-error-message received.\n");
										break;
									}
									len = (b1l > sizeof(msg) - 1) ? sizeof(msg) - 1 : b1l;
									if (len)
									{
										memcpy(((char *) &msg) + 1,b1,len);
										b1 += len;
										b1l -= len;
									}
									if (len < sizeof(msg) - 1)
									{
										len2 = sizeof(msg) - 1 - len;
										memcpy(((char *) &msg) + 1 + len,b2,len2);
										b2 += len2;
										b2l -= len2;
									}
									for (i = USER_QUEUE_START; i < LAST_USER_QUEUE; i++)
									{
										if ( (gtx->feed[i].state == DMX_STATE_GO) &&
										     (gtx->feed[i].type == DMX_TYPE_HW_SEC) &&
											 (gtx->feed[i].filter->pid == (msg.pid & 0x1FFF)) )
										{
											dprintk("avia_gt_napi: cc discontinuity on feed %d\n",i);
											gtx->feed[i].filter->invalid = 1;
											dmx_set_filter(gtx->feed[i].filter);
											gtx->feed[i].filter->invalid = 0;
											gtx_reset_queue(gtx->feed+i);
											gtx->feed[i].sec_len = 0;
											gtx->feed[i].sec_recv = 0;
											clear_bit(i,&datamask);
											blocked |= 1 << i;
											dmx_set_filter(gtx->feed[i].filter);
											break;
										}
									}
								}
								else if (type == DMX_MESSAGE_ADAPTION)
								{
									sync_lost = 0;
									adaption.type = type;
									if (b1l + b2l < sizeof(adaption) - 1)
									{
										printk("avia_gt_napi: short private adaption field message.\n");
										break;
									}
									len = (b1l > sizeof(adaption) - 1) ? sizeof(adaption) - 1 : b1l;
									if (len)
									{
										memcpy(((char *) &adaption) + 1,b1,len);
										b1 += len;
										b1l -= len;
									}
									if (len < sizeof(adaption) - 1)
									{
										len2 = sizeof(adaption) - 1 - len;
										memcpy(((char *) &adaption) + 1 + len,b2,len2);
										b2 += len2;
										b2l -= len2;
									}
									if (b1l + b2l < adaption.length)
									{
										printk("avia_gt_napi: short private adaption field message.\n");
										break;
									}
									len = adaption.length;
									if (len > b1l)
									{
										len -= b1l;
										b1 += b1l;
										b1l = 0;
										b2l -= len;
										b2 += len;
									}
									else
									{
										b1l -= len;
										b1 += len;
									}
								}
								else if (type == DMX_MESSAGE_SYNC_LOSS)
								{
									if (!sync_lost)
									{
										sync_lost = 1;
										printk("avia_gt_napi: lost sync\n");
										for (i = USER_QUEUE_START; i < LAST_USER_QUEUE; i++)
										{
											if ( (gtx->feed[i].state == DMX_STATE_GO) &&
											     (gtx->feed[i].type == DMX_TYPE_HW_SEC) &&
												 (gtx->feed[i].filter->pid == (msg.pid & 0x1FFF)) )
											{
												gtx->feed[i].filter->invalid = 1;
												dmx_set_filter(gtx->feed[i].filter);
												gtx->feed[i].filter->invalid = 0;
												gtx_reset_queue(gtx->feed+i);
												gtx->feed[i].sec_len = 0;
												gtx->feed[i].sec_recv = 0;
												clear_bit(i,&datamask);
												blocked |= 1 << i;
												dmx_set_filter(gtx->feed[i].filter);
												break;
											}
										}
									}
								}
								else if (type == DMX_MESSAGE_SECTION_COMPLETED)
								{
									comp_msg.type = type;
									if (b1l + b2l < sizeof(comp_msg) - 1)
									{
										printk("avia_gt_napi: short section completed message.\n");
										break;
									}
									len = (b1l > sizeof(comp_msg) - 1) ? sizeof(comp_msg) - 1 : b1l;
									if (len)
									{
										memcpy(((char *) &comp_msg) + 1,b1,len);
										b1 += len;
										b1l -= len;
									}
									if (len < sizeof(comp_msg) - 1)
									{
										len2 = sizeof(comp_msg) - 1 - len;
										memcpy(((char *) &comp_msg) + 1 + len,b2,len2);
										b2 += len2;
										b2l -= len2;
									}
									if ( !(blocked & (1<<comp_msg.filter_index)) )
									{
//										printk("got section completed %d\n",comp_msg.filter_index);
										sync_lost = 0;
										set_bit(comp_msg.filter_index,&datamask);
									}
								}
								else
								{
									printk("bad message, type-value %02X, len = %d\n",type,b1l+b2l);
//									dump(b1,b1l,b2,b2l);
									break;
								}
							}
						}
						break;
						// handle prefiltered section
						case DMX_TYPE_HW_SEC:
						{
							unsigned len;
							unsigned needed;
//							dump(b1,b1l,b2,b2l);
							while (b1l + b2l >= 3)
							{

								if (gtxfeed->sec_len == 0)
								{
									while ( (b1l > 0) && (*b1 == 0xFF) )
									{
										b1l--;
										b1++;
									}
									if (b1l == 0)
									{
										while ( (b2l > 0) && (*b2 == 0xFF) )
										{
											b2l--;
											b2++;
										}
									}
									if (b1l + b2l < 3)
									{
										break;
									}
									if (b1l >= 2)
										gtxfeed->sec_len = (b1[1] & 0x0F) << 8;
									else
										gtxfeed->sec_len = (b2[1-b1l] & 0x0F) << 8;
									if (b1l >= 3)
										gtxfeed->sec_len += b1[2] + 3;
									else
										gtxfeed->sec_len += b2[2-b1l] + 3;
								}

								if (gtxfeed->sec_len > 4096)
								{
									printk(KERN_ERR "section length %d > 4096!\n",gtxfeed->sec_len);
									b1l = 0;
									b2l = 0;
									gtxfeed->filter->invalid = 1;
									dmx_set_filter(gtxfeed->filter);
									gtxfeed->filter->invalid = 0;
									gtx_reset_queue(gtxfeed);
									dmx_set_filter(gtxfeed->filter);
									break;
								}
								needed = gtxfeed->sec_len - gtxfeed->sec_recv;
								if (b1l > 0)
								{
									len = (b1l > needed) ? needed : b1l;
									memcpy(gtxfeed->sec_buffer + gtxfeed->sec_recv,b1,len);
									b1l -= len;
									b1 += len;
									needed -= len;
									gtxfeed->sec_recv += len;
								}
								if ( (b2l > 0) && (needed > 0) )
								{
									len = (b2l > needed) ? needed : b2l;
									memcpy(gtxfeed->sec_buffer + gtxfeed->sec_recv,b2,len);
									b2l -= len;
									b2 += len;
									needed -= len;
									gtxfeed->sec_recv += len;
								}
								if (needed == 0)
								{
									if (gtx_handle_section(gtxfeed) == 0)
									{
										b1l = 0;
										b2l = 0;
										gtxfeed->filter->invalid = 1;
										dmx_set_filter(gtxfeed->filter);
										gtxfeed->filter->invalid = 0;
										gtx_reset_queue(gtxfeed);
										dmx_set_filter(gtxfeed->filter);
										break;
									}
								}
							}
							if (b1l + b2l > 0)
							{
								if ( ((b1l == 0) || (b1[0] == 0xFF)) &&
								     ((b1l < 2 ) || (b1[1] == 0xFF)) &&
									 ((b2l == 0) || (b2[0] == 0xFF)) &&
									 ((b2l < 2 ) || (b2[1] == 0xFF)) )
								{
									break;
								}
								printk(KERN_CRIT "not enough data to extract section length, this is unhandled!\n");
								gtxfeed->filter->invalid = 1;
								dmx_set_filter(gtxfeed->filter);
								gtxfeed->filter->invalid = 0;
								gtx_reset_queue(gtxfeed);
								dmx_set_filter(gtxfeed->filter);
							}
						}
						break;
#endif
						// handle section
						case DMX_TYPE_SEC:
						{
							static __u8 tsbuf[188];

							// let's rock
							while (b1l || b2l)
							{
								int tr=b1l, r=0, p=4;

								if (tr>188)
									tr=188;

								// check header & sync
								if (((b1l+b2l)%188) || (((char*)b1)[0]!=0x47))
								{
									dprintk("gtx_dmx: there's a BIG out of sync problem\n");
									break;
								}

								memcpy(tsbuf, b1, tr);

								r+=tr;
								b1l-=tr;
								b1+=tr;

								// TODO: handle CC

								tr=b2l;

								if (tr>(188-r))
									tr=(188-r);

								memcpy(tsbuf+r, b2, tr);

								b2l-=tr;
								b2+=tr;

								tr+=r;

								// no payload
								if (!(tsbuf[3]&0x10))							// adaption control field
								{
									dprintk("a packet with no payload. sachen gibt's.\n");
									continue;
								}

								ccn=tsbuf[3]&0x0f;								// continuity counter

								if (ccn == gtxfeed->sec_ccn)			// doppelt hält besser, hmm?
									continue;

								if ( (((ccn > 0)	&& (gtxfeed->sec_ccn + 1 != ccn)) ||
											((ccn == 0) && (gtxfeed->sec_ccn != 15))) &&
											(gtxfeed->sec_recv > 0) )
								{
									gtxfeed->sec_recv = 0;
									gtxfeed->sec_len = 0;
									dprintk("we lost a packet :-(\n");
								}

								gtxfeed->sec_ccn = ccn;

								tr-=4;														// 4 Byte fixe Headerlänge


								// af + pl
								if (tsbuf[3]&0x20)								// adaption field
								{
									dprintk("nen adaption field! HIER! (%d bytes aber immerhin) (report to tmb plz)\n", tsbuf[4]);
									// go home paket !
									if ( tsbuf[4] > 182 )
									{
										dprintk("gtx_dmx: warning afle=%d (ignore)\n",tsbuf[4]);
										continue;
									}

									tr-=tsbuf[4]+1;
									p+=tsbuf[4]+1;
								}

								if (tsbuf[1] & 0x40)							// Start einer neuen Section
								{
									if (tsbuf[p] != 0)							// neues Paket fängt mittendrin an
									{
										if (gtxfeed->sec_recv)				// haben wir den Anfang des vorherigen Paketes ?
										{
											r = gtxfeed->sec_len - gtxfeed->sec_recv;
											if (r > tsbuf[p])					// wenn eine neue Section kommt muß die vorherige abgeschlossen werden
											{
												dprintk("gtx_dmx: dropping section because length-confusion.\n");
											}
											else {
												memcpy(gtxfeed->sec_buffer+gtxfeed->sec_recv,tsbuf+p+1,r);
												gtxfeed->sec_recv+=r;
												gtx_handle_section(gtxfeed);
											}
										}
										tr -= tsbuf[p] + 1;
										p += tsbuf[p] + 1;
									}
									else {
									 tr--;
									 p++;
									}
									gtxfeed->sec_recv = 0;
									gtxfeed->sec_len = 0;
								}

								while (tr)
								{
									if (gtxfeed->sec_recv) {				// haben bereits einen Anfang
										r = gtxfeed->sec_len - gtxfeed->sec_recv;
										if (r > tr)										// ein kleines Teilstück kommt hinzu
										{
											r = tr;
										}
										memcpy(gtxfeed->sec_buffer+gtxfeed->sec_recv,tsbuf+p,r);
										gtxfeed->sec_recv += r;
										if ( gtxfeed->sec_len == gtxfeed->sec_recv)
										{
											gtx_handle_section(gtxfeed);
										}
									}
									else {													// neue Section
										if (tsbuf[p] == 0xFF)					// Rest padding ?
										{
											break;
										}
										if (tr < 3)										// eine neue Section mit weniger als 3 Bytes...
										{
											break;											// Wenn wir hier kopieren gibt's oben wegen der fehlenden Länge ein Problem
										}
										r = ((tsbuf[p+1] & 0x0F) << 8) + tsbuf[p+2] + 3;
										if (r <= 4096) {							// größer darf nicht
											gtxfeed->sec_len = r;
											if (r > tr)									// keine komplette Section
											{
												r = tr;
											}
											memcpy(gtxfeed->sec_buffer,tsbuf+p,r);
											gtxfeed->sec_recv += r;
											if ( gtxfeed->sec_len == gtxfeed->sec_recv)
											{
												gtx_handle_section(gtxfeed);
											}
										}
									}
									tr -= r;
									p += r;
								}
							}
						}
						break;
						case DMX_TYPE_PES:
	//					gtx->feed[queue].cb.pes(b1, b1l, b2, b2l, & gtxfeed->feed.pes, 0);
							break;

					} // switch end

				}
			}
		clear_bit(queue,&datamask);
	}
}

static gtx_demux_filter_t *GtxDmxFilterAlloc(gtx_demux_feed_t *gtxfeed)
{
	gtx_demux_t *gtx=gtxfeed->demux;
	int i = (int)0;
#if 0
	for (i=0; i<32; i++)
		if (gtx->filter[i].state==DMX_STATE_FREE)
			break;
	if (i==32)
		return 0;
#endif
	i=gtxfeed->index;
	if (gtx->filter[i].state!=DMX_STATE_FREE)
		printk("ASSERTION FAILED: feed is not free but should be\n");
	gtx->filter[i].state=DMX_STATE_ALLOCATED;
	return &gtx->filter[i];
}

static gtx_demux_feed_t *GtxDmxFeedAlloc(gtx_demux_t *gtx, int type)
{
	int i = 0;

	switch (type)
	{
/*	case DMX_TS_PES_USER:
		return 0; */
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
								// TODO: evtl. system-queue nehmen wenn nix anderes mehr da ist..
		if (i==LAST_USER_QUEUE)
			return 0;
		break;
	}

	if (gtx->feed[i].state!=DMX_STATE_FREE)
		return 0;
	gtx->feed[i].state=DMX_STATE_ALLOCATED;
	dprintk(KERN_DEBUG "gtx-dmx: using queue %d for %d\n", i, type);
	return &gtx->feed[i];
}

static int dmx_open(struct dmx_demux_s* demux)
{
	gtx_demux_t *gtx=(gtx_demux_t*)demux;
	gtx->users++;
	return 0;
}

static int dmx_close (struct dmx_demux_s* demux)
{
	gtx_demux_t *gtx=(gtx_demux_t*)demux;
	if (!gtx->users)
		return -ENODEV;
	gtx->users--;
	dprintk(KERN_DEBUG "gtx_dmx: close.\n");
	if (!gtx->users)
	{
		int i;
		// clear resources
		gtx_tasklet.data=0;
		for (i=0; i<32; i++)
			if (gtx->feed[i].state!=DMX_STATE_FREE)
				dprintk(KERN_ERR "gtx-dmx.o: LEAK: queue %d used but it shouldn't.\n", i);
	}
	return 0;
}

static int dmx_write (struct dmx_demux_s* demux, const char* buf, size_t count)
{
	dprintk(KERN_ERR "gtx-dmx: dmx_write not yet implemented!\n");
	return 0;
}

static void dmx_set_filter(gtx_demux_filter_t *filter)
{
	if (filter->invalid)
		avia_gt_dmx_set_pid_table(filter->index, filter->wait_pusi, filter->invalid, filter->pid);
#ifdef GTX_SECTIONS
	if ( filter->output != GTX_OUTPUT_8BYTE)
#endif
		avia_gt_dmx_set_pid_control_table(filter->index, filter->output, filter->queue, filter->fork, filter->cw_offset, filter->cc, filter->start_up, filter->pec, 0, 0);
#ifdef GTX_SECTIONS
	else
		avia_gt_dmx_set_pid_control_table(filter->index, filter->output, filter->queue, filter->fork, filter->cw_offset, filter->cc, filter->start_up, filter->pec, filter->index, 1);
#endif
	if (!filter->invalid)
		avia_gt_dmx_set_pid_table(filter->index, filter->wait_pusi, filter->invalid, filter->pid);
}

static int dmx_ts_feed_set(struct dmx_ts_feed_s* feed, __u16 pid, size_t callback_length, size_t circular_buffer_size, int descramble, struct timespec timeout)
{
	gtx_demux_feed_t *gtxfeed=(gtx_demux_feed_t*)feed;
	gtx_demux_filter_t *filter=gtxfeed->filter;

	if (pid>0x1FFF)
		return -EINVAL;

	gtxfeed->pid=pid;

	filter->pid=pid;
	filter->wait_pusi=0;	// right?

	if ((auto_pcr_pid) && (gtxfeed->pes_type == DMX_TS_PES_VIDEO)) {

		dprintk("avia_gt_napi: assuming PCR_PID == VPID == 0x%04x\n", pid);

		avia_gt_dmx_set_pcr_pid(pid);

	}

	filter->type=GTX_FILTER_PID;

	if (gtxfeed->output&TS_DECODER)
		gtxfeed->output|=TS_PAYLOAD_ONLY;	 // weil: wir haben dual-pes

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

static void dmx_enable_tap(struct gtx_demux_feed_s *gtxfeed)
{

	if (gtxfeed->tap)
		return;

	gtxfeed->tap = 1;

	avia_gt_alloc_irq(avia_gt_dmx_get_queue_irq(gtxfeed->index), gtx_queue_interrupt);

}

static void dmx_disable_tap(struct gtx_demux_feed_s *gtxfeed)
{

    if (!gtxfeed->tap)
		return;

	gtxfeed->tap = 0;

    avia_gt_free_irq(avia_gt_dmx_get_queue_irq(gtxfeed->index));

}

static void dmx_update_pid(gtx_demux_t *gtx, int pid)
{

	u8 i = 0;
	u8 used = 0;
	
	for (i = 0; i < LAST_USER_QUEUE; i++) {
	
		if ((gtx->feed[i].state == DMX_STATE_GO) && (gtx->feed[i].pid == pid) && (gtx->feed[i].output & TS_PACKET)) {
		
			used++;
			
			break;
			
		}
			
	}

	for (i = 0; i < LAST_USER_QUEUE; i++) {
	
		if ((gtx->feed[i].state == DMX_STATE_GO) && (gtx->feed[i].pid == pid)) {
		
			if (used && (gtx->feed[i].type != DMX_TYPE_HW_SEC))
				dmx_enable_tap(&gtx->feed[i]);
			else
				dmx_disable_tap(&gtx->feed[i]);
				
		}
		
	}
		
}

static int dmx_ts_feed_start_filtering(struct dmx_ts_feed_s* feed)
{
	gtx_demux_feed_t *gtxfeed=(gtx_demux_feed_t*)feed;
	gtx_demux_filter_t *filter=gtxfeed->filter;

	if (gtxfeed->state!=DMX_STATE_READY)
	{
		dprintk("gtx_dmx: feed not DMX_STATE_READY\n");
		return -EINVAL;
	}

//	gtxfeed->readptr=avia_gt_dmx_get_queue_write_pointer(gtxfeed->index);
	gtx_reset_queue(gtxfeed);

	filter->start_up=1;
	filter->invalid=0;
	dmx_set_filter(gtxfeed->filter);
	feed->is_filtering=1;

	gtxfeed->state=DMX_STATE_GO;

	dmx_update_pid(gtxfeed->demux, gtxfeed->pid);

//	udelay(100);
//	enx_tdp_trace();

	return 0;
}

static int dmx_ts_feed_set_type(struct dmx_ts_feed_s* feed, int type, dmx_ts_pes_t pes_type)
{
//	gtx_demux_feed_t *gtxfeed=(gtx_demux_feed_t*)feed;
	dprintk(KERN_DEBUG "gtx_dmx: dmx_ts_feed_set_type(%d, %d)\n", type, pes_type);
	return 0;
}

static int dmx_ts_feed_stop_filtering(struct dmx_ts_feed_s* feed)
{

	gtx_demux_feed_t *gtxfeed = (gtx_demux_feed_t *)feed;
	gtx_demux_filter_t *filter = gtxfeed->filter;

	filter->invalid = 1;

	dmx_set_filter(gtxfeed->filter);

	feed->is_filtering = 0;

	gtxfeed->state = DMX_STATE_READY;

	dmx_update_pid(gtxfeed->demux, gtxfeed->pid);

	dmx_disable_tap(gtxfeed);

	gtxfeed->readptr = avia_gt_dmx_get_queue_write_pointer(gtxfeed->index);

	gtx_reset_queue(gtxfeed);

	return 0;

}

static int dmx_allocate_ts_feed (struct dmx_demux_s* demux, dmx_ts_feed_t** feed, dmx_ts_cb callback, int type, dmx_ts_pes_t pes_type)
{

	gtx_demux_t *gtx = (gtx_demux_t *)demux;
	gtx_demux_feed_t *gtxfeed = (gtx_demux_feed_t *)NULL;

	if (!(gtxfeed = GtxDmxFeedAlloc(gtx, pes_type))) {

		dprintk(KERN_ERR "gtx_dmx: couldn't get gtx feed\n");

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

	if (!(gtxfeed->filter=GtxDmxFilterAlloc(gtxfeed)))
	{
		dprintk(KERN_ERR "gtx_dmx: couldn't get gtx filter\n");
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

static int dmx_section_feed_allocate_filter (struct dmx_section_feed_s* feed, dmx_section_filter_t** filter)
{
	gtx_demux_feed_t *gtxfeed=(gtx_demux_feed_t*)feed;
	gtx_demux_t *gtx=gtxfeed->demux;
//	gtx_demux_filter_t *gtxfilter=gtxfeed->filter;
	gtx_demux_secfilter_t *gtxsecfilter=0;
	int i = (int)0;

	dprintk("gtx_dmx: dmx_section_feed_allocate_filter.\n");

	if (gtxfeed->filter->no_of_filters >= 32)
		return -ENOSPC;

	for (i=0; i<32; i++)
		if (gtx->secfilter[i].state==DMX_STATE_FREE)
		{
			gtxsecfilter=gtx->secfilter+i;
			break;
		}

	if (!gtxsecfilter)
		return -ENOSPC;

	*filter=&gtxsecfilter->filter;
	(*filter)->parent=feed;
	(*filter)->priv=0;
	gtxsecfilter->feed=gtxfeed;
	gtxsecfilter->state=DMX_STATE_READY;

	gtxsecfilter->next=gtxfeed->secfilter;
	mb();
	gtxfeed->secfilter=gtxsecfilter;
	gtxfeed->filter->no_of_filters++;
	return 0;
}

static int dmx_section_feed_release_filter(dmx_section_feed_t *feed, dmx_section_filter_t* filter)
{
	gtx_demux_feed_t *gtxfeed=(gtx_demux_feed_t*)feed;
	gtx_demux_secfilter_t *f, *gtxfilter=(gtx_demux_secfilter_t*)filter;

	dprintk("gtx_dmx: dmx_section_feed_release_filter.\n");
	if (gtxfilter->feed!=gtxfeed)
	{
		dprintk("FAILED (gtxfilter->feed!=gtxfeed) (%p != %p)\n", gtxfilter->feed, gtxfeed);
		return -EINVAL;
	}
	if (feed->is_filtering)
	{
		dprintk("FAILED (feed->is_filtering)\n");
		return -EBUSY;
	}

	f=gtxfeed->secfilter;
	if (f==gtxfilter)
		gtxfeed->secfilter=gtxfilter->next;
	else
	{
		while (f->next!=gtxfilter)
			f=f->next;
		f->next=f->next->next;
	}
	gtxfilter->state=DMX_STATE_FREE;
	gtxfeed->filter->no_of_filters--;
	return 0;
}

static int dmx_section_feed_set(struct dmx_section_feed_s* feed,
										 __u16 pid, size_t circular_buffer_size,
										 int descramble, int check_crc)
{
	gtx_demux_feed_t *gtxfeed=(gtx_demux_feed_t*)feed;
	gtx_demux_filter_t *filter=gtxfeed->filter;

	if (pid>0x1FFF)
		return -EINVAL;

	gtxfeed->pid=pid;
	gtxfeed->check_crc=check_crc;

	filter->pid=pid;
	filter->queue=gtxfeed->index;

	filter->invalid=1;
	filter->fork=0;
	filter->cw_offset=0;
	filter->cc=0;
	filter->start_up=0;
#ifdef GTX_SECTIONS
	if (gtxfeed->demux->hw_sec_filt_enabled)
	{
		filter->output=GTX_OUTPUT_8BYTE;
		filter->pec=1;
		filter->wait_pusi=1;
	}
	else
	{
#endif
		filter->output=GTX_OUTPUT_TS;
		filter->pec=0;
		filter->wait_pusi=0;
#ifdef GTX_SECTIONS
	}
#endif
	dmx_set_filter(gtxfeed->filter);

	return 0;
}

static int dmx_section_feed_start_filtering(dmx_section_feed_t *feed)
{
#ifdef GTX_SECTIONS
	gtx_demux_feed_t *gtxfeed=(gtx_demux_feed_t*)feed;
	gtx_demux_filter_t *filter=gtxfeed->filter;
	int rc = (int)0;



	if (filter->output == GTX_OUTPUT_8BYTE)
	{
		rc = avia_gt_dmx_set_section_filter(gtxfeed->demux,gtxfeed->index,filter->no_of_filters,gtxfeed->secfilter);
		if (rc < 0)
		{
			return -ENOSPC;
		}
		gtx_reset_queue(gtxfeed);
		dprintk("gtx_dmx: section filtering start (%d filter)\n", filter->no_of_filters);
	}
#endif

	dmx_ts_feed_start_filtering((dmx_ts_feed_t*)feed);

	return 0;
}

static int dmx_section_feed_stop_filtering(struct dmx_section_feed_s* feed)
{
	dprintk("gtx_dmx: dmx_section_feed_stop_filtering.\n");
	dmx_ts_feed_stop_filtering((dmx_ts_feed_t*)feed);
	return 0;
}

static int dmx_allocate_section_feed (struct dmx_demux_s* demux, dmx_section_feed_t** feed, dmx_section_cb callback)
{
	gtx_demux_t *gtx=(gtx_demux_t*)demux;
	gtx_demux_feed_t *gtxfeed = (gtx_demux_feed_t *)NULL;

	dprintk("gtx_dmx: dmx_allocate_section_feed.\n");

	if (!(gtxfeed=GtxDmxFeedAlloc(gtx, DMX_TS_PES_OTHER)))
	{
		dprintk("gtx_dmx: couldn't get gtx feed (for section_feed)\n");
		return -EBUSY;
	}

	gtxfeed->cb.sec=callback;
	gtxfeed->demux=gtx;
	gtxfeed->pid=0xFFFF;
	gtxfeed->secfilter=0;

	*feed=&gtxfeed->feed.sec;
	(*feed)->is_filtering=0;
	(*feed)->parent=demux;
	(*feed)->priv=0;
	(*feed)->set=dmx_section_feed_set;
	(*feed)->allocate_filter=dmx_section_feed_allocate_filter;
	(*feed)->release_filter=dmx_section_feed_release_filter;
	(*feed)->start_filtering=dmx_section_feed_start_filtering;
	(*feed)->stop_filtering=dmx_section_feed_stop_filtering;

	gtxfeed->pes_type=DMX_TS_PES_OTHER;
	gtxfeed->sec_buffer=kmalloc(4096, GFP_KERNEL);
	gtxfeed->sec_recv=0;
	gtxfeed->sec_len=0;
	gtxfeed->sec_ccn=16;

#ifdef GTX_SECTIONS
	if (gtx->hw_sec_filt_enabled)
	{
		gtxfeed->type=DMX_TYPE_HW_SEC;
		gtxfeed->output = TS_PACKET | TS_PAYLOAD_ONLY;
	}
	else {
#endif
		gtxfeed->type=DMX_TYPE_SEC;
		gtxfeed->output = TS_PACKET;
#ifdef GTX_SECTIONS
	}
#endif

	gtxfeed->state=DMX_STATE_READY;

	if (!(gtxfeed->filter=GtxDmxFilterAlloc(gtxfeed)))
	{
		dprintk("gtx_dmx: couldn't get gtx filter\n");
		gtxfeed->state=DMX_STATE_FREE;
		return -EBUSY;
	}

	dprintk("gtx_dmx: allocating section feed, filter %d.\n", gtxfeed->filter->index);

	gtxfeed->filter->type=DMX_TYPE_SEC;
	gtxfeed->filter->feed=gtxfeed;
	gtxfeed->filter->state=DMX_STATE_READY;
	gtxfeed->filter->no_of_filters = 0;
	return 0;
}

static int dmx_release_section_feed (struct dmx_demux_s* demux,	dmx_section_feed_t* feed)
{
	gtx_demux_feed_t *gtxfeed=(gtx_demux_feed_t*)feed;
#ifdef GTX_SECTIONS
	gtx_demux_t *gtx=(gtx_demux_t*)demux;
#endif

	dprintk("gtx_dmx: dmx_release_section_feed.\n");

	if (gtxfeed->secfilter)
	{
		dprintk("gtx_dmx: BUSY.\n");
		return -EBUSY;
	}
	kfree(gtxfeed->sec_buffer);
	dmx_release_ts_feed (demux, (dmx_ts_feed_t*)feed);						// free corresponding queue
#ifdef GTX_SECTIONS
	if (gtx->hw_sec_filt_enabled)
	{
		avia_gt_dmx_release_section_filter(demux,gtxfeed->index);
	}
#endif
	return 0;
}

static int dmx_add_frontend (struct dmx_demux_s* demux, dmx_frontend_t* frontend)
{
	gtx_demux_t *gtx=(gtx_demux_t*)demux;
	struct list_head *pos = (struct list_head *)NULL, *head=&gtx->frontend_list;
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

static int dmx_remove_frontend (struct dmx_demux_s* demux,	dmx_frontend_t* frontend)
{
	gtx_demux_t *gtx=(gtx_demux_t*)demux;
	struct list_head *pos = (struct list_head *)NULL, *head=&gtx->frontend_list;
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
	if (list_empty(&gtx->frontend_list))
		return 0;
	return &gtx->frontend_list;
}

static int dmx_connect_frontend (struct dmx_demux_s* demux, dmx_frontend_t* frontend)
{
	if (demux->frontend)
		return -EINVAL;
	demux->frontend=frontend;
	return -EINVAL;			 // was soll das denn? :)
}

static int dmx_disconnect_frontend (struct dmx_demux_s* demux)
{
	demux->frontend=0;
	return -EINVAL;
}

static void gtx_dmx_set_pcr_pid(int pid)
{

	avia_gt_dmx_set_pcr_pid((u16)pid);

}

int GtxDmxInit(gtx_demux_t *gtxdemux)
{
	dmx_demux_t *dmx=&gtxdemux->dmx;
	int i =(int)0, ptr =(int)0;
#ifdef GTX_SECTIONS
	u8 nullmask[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
#endif
	gtxdemux->users=0;

	gtxdemux->frontend_list.next=
		gtxdemux->frontend_list.prev=
			&gtxdemux->frontend_list;

	for (i=0; i<NUM_PID_FILTER; i++)			// disable all pid filters
		avia_gt_dmx_set_pid_table(i, 0, 1, 0);

	for (i=0; i<NUM_QUEUES; i++)
		avia_gt_dmx_set_queue_irq(i, 0, 0);

	ptr=AVIA_GT_MEM_DMX_OFFS;

	for (i=0; i<NUM_QUEUES; i++)
	{
		gtxdemux->feed[i].size=(1<<buffersize[i])*64;
		if (ptr&(gtxdemux->feed[i].size-1))
		{
			printk("avia_gt_napi: warning, misaligned queue %d (is %x, size %x), aligning...\n", i, ptr, gtxdemux->feed[i].size);
			ptr+=gtxdemux->feed[i].size;
			ptr&=~(gtxdemux->feed[i].size-1);
		}
		gtxdemux->feed[i].base=ptr;
		ptr+=gtxdemux->feed[i].size;
		gtxdemux->feed[i].end=gtxdemux->feed[i].base+gtxdemux->feed[i].size;
		//		gtx_queue[i].base=avia_gt_alloc_dram(gtx_queue[i].size, gtx_queue[i].size);
		avia_gt_dmx_set_queue((unsigned char)i, gtxdemux->feed[i].base, buffersize[i]);
		gtxdemux->feed[i].index=i;
		gtxdemux->feed[i].state=DMX_STATE_FREE;
		gtxdemux->feed[i].tap=0;
	}
	dprintk("%dkb ram used for queues\n", ptr/1024);

	for (i=0; i<32; i++)
	{
		gtxdemux->filter[i].index=i;
		gtxdemux->filter[i].state=DMX_STATE_FREE;
	}

	for (i=0; i<32; i++)
	{
		gtxdemux->secfilter[i].index=i;
		gtxdemux->secfilter[i].state=DMX_STATE_FREE;
	}

	for (i=0; i<32; i++)
	{
		gtxdemux->filter_definition_table_entry_user[i] = -1;
	}

#ifdef GTX_SECTIONS
	if ( (gtxdemux->hw_sec_filt_enabled = avia_gt_dmx_get_hw_sec_filt_avail()) == 1) {
		printk(KERN_INFO "avia_gt_napi: hardware section filtering enabled.\n");
		avia_gt_dmx_set_filter_parameter_table(31,nullmask,nullmask,0,0);
	}
	else
	{
		printk(KERN_INFO "avia_gt_napi: hardware section filtering disabled.\n");
	}
#else
	gtxdemux->hw_sec_filt_enabled = 0;
#endif

	gtx_set_queue_pointer(Q_VIDEO, gtxdemux->feed[VIDEO_QUEUE].base, gtxdemux->feed[VIDEO_QUEUE].base, buffersize[VIDEO_QUEUE], 0);				// set system queues
	gtx_set_queue_pointer(Q_AUDIO, gtxdemux->feed[AUDIO_QUEUE].base, gtxdemux->feed[AUDIO_QUEUE].base, buffersize[AUDIO_QUEUE], 0);
	gtx_set_queue_pointer(Q_TELETEXT, gtxdemux->feed[TELETEXT_QUEUE].base, gtxdemux->feed[TELETEXT_QUEUE].base, buffersize[TELETEXT_QUEUE], 0);

	dmx->id="demux0";
	dmx->vendor="C-Cube";
	dmx->model="AViA eNX/GTX";
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

	dmx->add_frontend = dmx_add_frontend;
	dmx->remove_frontend = dmx_remove_frontend;
	dmx->get_frontends = dmx_get_frontends;
	dmx->connect_frontend = dmx_connect_frontend;
	dmx->disconnect_frontend = dmx_disconnect_frontend;
	dmx->flush_pcr = avia_gt_dmx_force_discontinuity;
	dmx->set_pcr_pid = gtx_dmx_set_pcr_pid;

	gtx_tasklet.data=gtxdemux;

	if (dmx_register_demux(dmx)<0)
		return -1;

	if (dmx->open(dmx)<0)
		return -1;

#ifdef GTX_SECTIONS
	if (gtxdemux->hw_sec_filt_enabled) {
	
		gtx_reset_queue(gtxdemux->feed + MESSAGE_QUEUE);

		gtxdemux->feed[MESSAGE_QUEUE].type = DMX_TYPE_MESSAGE;
		gtxdemux->feed[MESSAGE_QUEUE].pid = 0x2000;
		gtxdemux->feed[MESSAGE_QUEUE].output = TS_PAYLOAD_ONLY;
		gtxdemux->feed[MESSAGE_QUEUE].state = DMX_STATE_GO;

		dmx_enable_tap(gtxdemux->feed + MESSAGE_QUEUE);
		
	}
#endif

	return 0;

}

int GtxDmxCleanup(gtx_demux_t *gtxdemux)
{
	dmx_demux_t *dmx=&gtxdemux->dmx;

#ifdef GTX_SECTIONS
	if ( (gtxdemux->feed[MESSAGE_QUEUE].type == DMX_TYPE_MESSAGE) &&
	     (gtxdemux->feed[MESSAGE_QUEUE].state == DMX_STATE_GO) )
	{
		dmx_disable_tap(gtxdemux->feed+MESSAGE_QUEUE);
		gtxdemux->feed[MESSAGE_QUEUE].state = DMX_STATE_FREE;
	}
#endif

	if (dmx_unregister_demux(dmx)<0)
		return -1;

	return 0;
}

int __init avia_gt_napi_init(void)
{

	printk("avia_gt_napi: $Id: avia_gt_napi.c,v 1.96 2002/09/02 20:56:06 Jolt Exp $\n");

	gt_info = avia_gt_get_info();

	if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {

		printk("avia_gt_napi: Unsupported chip type\n");

		return -EIO;

    }

	GtxDmxInit(&gtx);
	register_demux(&gtx.dmx);

	return 0;

}

void __exit avia_gt_napi_exit(void)
{

	gtx_dmx_close();

}

#ifdef MODULE
module_init(avia_gt_napi_init);
module_exit(avia_gt_napi_exit);
#endif
