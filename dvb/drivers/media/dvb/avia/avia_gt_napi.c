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
 *   $Revision: 1.85 $
 *   $Log: avia_gt_napi.c,v $
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
		this driver implements the Nokia-DVB-Api (Kernel level Demux driver),
		but it isn't yet complete.

		It does not yet support section filtering and descrambling (and some
		minor features as well).

		writing isn't supported, either.
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
#include "crc32.c"

static sAviaGtInfo *gt_info;
static unsigned char auto_pcr_pid = 0;

// #undef GTX_SECTIONS

#ifdef MODULE
MODULE_PARM(auto_pcr_pid, "i");
MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("Avia eNX/GTX demux driver");
#endif

static gtx_demux_t gtx;

static void gtx_task(void *);
static int GtxDmxInit(gtx_demux_t *gtxdemux);
static int GtxDmxCleanup(gtx_demux_t *gtxdemux);

static int wantirq=0;

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

void gtx_set_pid_table(int entry, int wait_pusi, int invalid, int pid);
void gtx_set_pid_control_table(int entry, int type, int queue, int fork, int cw_offset, int cc, int start_up, int pec);

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

void gtx_set_pid_table(int entry, int wait_pusi, int invalid, int pid)
{

	if (avia_gt_chip(ENX))
		enx_reg_16n(TDP_INSTR_RAM + 0x700 + entry * 2) = ((!!wait_pusi) << 15) | ((!!invalid) << 14) | pid;
	else if (avia_gt_chip(GTX))
		gtx_reg_16n(GTX_REG_RISC + 0x700 + entry * 2) = ((!!wait_pusi) << 15) | ((!!invalid) << 14) | pid;

}

void gtx_set_pid_control_table(int entry, int type, int queue, int fork, int cw_offset, int cc, int start_up, int pec)
{

    u8 w[4];
    w[0] = type << 5;

    if (avia_gt_chip(ENX)) {

	if ((enx_reg_16n(TDP_INSTR_RAM * 0x7FE) & 0xFF00) >= 0xA000)
	    w[0] |= (queue) & 31;
	else
	    w[0] |= (queue + 1) & 31;

    } else if (avia_gt_chip(GTX)) {

	if ((gtx_reg_16n(GTX_REG_RISC + 0x7FE) & 0xFF00) >= 0xA000)
		w[0] |= (queue) & 31;
	else
		w[0] |= (queue + 1) & 31;

    }

    w[1] = (!!fork) << 7;
    w[1] |= cw_offset << 4;
    w[1] |= cc;
    w[2] = (!!start_up) << 6;
    w[2] |= (!!pec) << 5;
    w[3] = 0;

    if (avia_gt_chip(ENX))
	enx_reg_32n(TDP_INSTR_RAM + 0x740 + entry * 4) = *(u32*)w;
    else if (avia_gt_chip(GTX))
	gtx_reg_32n(GTX_REG_RISC + 0x740 + entry * 4) = *(u32*)w;

}

#ifdef GTX_SECTIONS
void gtx_set_pid_control_table_section(int entry, int type, int queue, int fork, int cw_offset, int cc, int start_up, int pec, int filt_tab_idx, int no_of_filters)
{
#error please fix driver first! (enx support!)
	u8 w[4];
	w[0]=type<<5;
	if ((rh(RISC+0x7FE)&0xFF00)==0xB100)
		w[0]|=(queue)&31;
	else
		w[0]|=(queue+1)&31;
	w[1]=(!!fork)<<7;
	w[1]|=cw_offset<<4;
	w[1]|=cc;
	w[2]=1<<7;
	w[2]|=(!!start_up)<<6;
	w[2]|=(!!pec)<<5;
	w[2]|=filt_tab_idx;
	w[3]=no_of_filters;
	dprintk("avia_gt_napi: no_of_filters %x (%08x)\n", no_of_filters, *(u32*)w);
	rw(RISC+0x740+entry*4)=*(u32*)w;
	udelay(1000*1000);
	dprintk("avia_gt_napi: read %08x\n", rw(RISC+0x740+entry*4));
}

void gtx_set_filter_definition_table(int entry, int and_or_flag, int filter_param_id)
{
	u8 w[4]={0, 0};
	w[0]=(!!and_or_flag)<<7;
	w[0]|=filter_param_id<<6;
	*((char*)&rh(RISC+0x7C0+entry))=w[0];
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

static __u32 datamask=0;

static void gtx_queue_interrupt(unsigned short irq)
{

    unsigned char nr = AVIA_GT_IRQ_REG(irq);
    unsigned char bit = AVIA_GT_IRQ_BIT(irq);
    int queue;

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

	datamask|=1<<queue;
	wantirq++;

	if (gtx_tasklet.data)
		schedule_task(&gtx_tasklet);
}

extern int register_demux(struct dmx_demux_s *demux);
extern int unregister_demux(struct dmx_demux_s *demux);

void gtx_dmx_close(void)
{
	int i;
	int j;

	unregister_demux(&gtx.dmx);
	GtxDmxCleanup(&gtx);

    if (avia_gt_chip(ENX)) {

	for (i=1; i<16; i++)
	{
		avia_gt_free_irq(AVIA_GT_IRQ(3, i));
		avia_gt_free_irq(AVIA_GT_IRQ(4, i));
	}
	avia_gt_free_irq(AVIA_GT_IRQ(5, 6));
	avia_gt_free_irq(AVIA_GT_IRQ(5, 7));
	avia_gt_free_irq(ENX_IRQ_PCR);					 // PCR

    } else if (avia_gt_chip(GTX)) {

	for (j=0; j<2; j++)
		for (i=0; i<16; i++)
			avia_gt_free_irq(AVIA_GT_IRQ(j+2, i));

	avia_gt_free_irq(GTX_IRQ_PCR);					 // PCR

    }

}
								// nokia api

static void gtx_handle_section(gtx_demux_feed_t *gtxfeed)
{
	gtx_demux_secfilter_t *secfilter;

	if (gtxfeed->sec_recv != gtxfeed->sec_len)
	{
		dprintk("gtx_dmx: have: %d, want %d\n", gtxfeed->sec_recv, gtxfeed->sec_len);
	}

	if (!gtxfeed->sec_recv)
	{
		gtxfeed->sec_len=gtxfeed->sec_recv=0;
		return;
	}

	for (secfilter=gtxfeed->secfilter; secfilter; secfilter=secfilter->next)
	{
		int ok=1, i;

		for (i=0; i<DMX_MAX_FILTER_SIZE && ok; i++)
		{
			if ( ((gtxfeed->sec_buffer[i]^secfilter->filter.filter_value[i])&secfilter->filter.filter_mask[i]) )
			{
				ok=0;
			}
		}

		if (ok)
		{
			if ( (!gtxfeed->check_crc) || (crc32(gtxfeed->sec_buffer, gtxfeed->sec_len) == 0) )
				gtxfeed->cb.sec(gtxfeed->sec_buffer, gtxfeed->sec_len, 0, 0, &secfilter->filter, 0);
			else
				dprintk("gtx_dmx: CRC Problem !!!\n");
		}
	}

	gtxfeed->sec_len=gtxfeed->sec_recv=0;
}

static void gtx_task(void *data)
{
	gtx_demux_t *gtx=(gtx_demux_t*)data;
	int queue;
	int ccn;
	static int c;

	for (queue=0; datamask && queue<32; queue++)
		if (datamask&(1<<queue))
		{
			gtx_demux_feed_t *gtxfeed=gtx->feed+queue;

			if (gtxfeed->state!=DMX_STATE_GO)
			{
				dprintk("gtx_dmx: DEBUG: interrupt on non-GO feed\n!");
			}
			else
			{
//				if (gtxfeed->output&TS_PACKET)
				{
					int wptr;
					int rptr;
					int i;

					__u8 *b1, *b2;
					size_t b1l, b2l;

					wptr = avia_gt_dmx_get_queue_write_pointer(queue);
					rptr = gtxfeed->readptr;

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
					}	else
						gtx->feed[queue].readptr=wptr;

					if (gtxfeed->queeue)	// workaround for videoqueue PES packets w/ wrong offset
					{
						b1l-=*b1;
						b1+=*b1;
						if (b1l<0)
						{
							b2l+=b1l;
							b1l=0;
						}
					}

					switch (gtx->feed[queue].type)
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
		datamask&=~(1<<queue);
	}

	if (c++ > 100)
	{
		// display wantirq stat
		c=0;
	}
	wantirq=0;
}

static gtx_demux_filter_t *GtxDmxFilterAlloc(gtx_demux_feed_t *gtxfeed)
{
	gtx_demux_t *gtx=gtxfeed->demux;
	int i;
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
	int i;

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
	gtx_set_pid_table(filter->index, filter->wait_pusi, filter->invalid, filter->pid);
#if GTX_SECTIONS
	if (filter->type==GTX_FILTER_PID)
#endif
		gtx_set_pid_control_table(filter->index, filter->output, filter->queue, filter->fork, filter->cw_offset, filter->cc, filter->start_up, filter->pec);
#if GTX_SECTIONS
	else
		gtx_set_pid_control_table_section(filter->index, filter->output, filter->queue, filter->fork, filter->cw_offset, filter->cc, filter->start_up, filter->pec, filter->filt_tab_idx, filter->no_of_filters);
#endif
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
	if (!gtxfeed->tap)
	{
		gtxfeed->tap=1;

    if (avia_gt_chip(ENX)) {

		if (gtxfeed->index>=17)
		{
			gtxfeed->int_nr=3;
			gtxfeed->int_bit=gtxfeed->index-16;
		} else if (gtxfeed->index>=2)
		{
			gtxfeed->int_nr=4;
			gtxfeed->int_bit=gtxfeed->index-1;
		}	else
		{
			gtxfeed->int_nr=5;
			gtxfeed->int_bit=gtxfeed->index+6;
		}
		avia_gt_alloc_irq(AVIA_GT_IRQ(gtxfeed->int_nr, gtxfeed->int_bit), gtx_queue_interrupt);

    } else if (avia_gt_chip(GTX)) {
	avia_gt_alloc_irq(AVIA_GT_IRQ(2+!!(gtxfeed->index&16), gtxfeed->index&15), gtx_queue_interrupt);
    }
	}
}

static void dmx_disable_tap(struct gtx_demux_feed_s *gtxfeed)
{

    if (gtxfeed->tap) {

	gtxfeed->tap = 0;

	if (avia_gt_chip(ENX))
	    avia_gt_free_irq(AVIA_GT_IRQ(gtxfeed->int_nr, gtxfeed->int_bit));
	else if (avia_gt_chip(GTX))
	    avia_gt_free_irq(AVIA_GT_IRQ(2+!!(gtxfeed->index&16), gtxfeed->index&15));

    }

}

static void dmx_update_pid(gtx_demux_t *gtx, int pid)
{
	int i, used=0;
	for (i=0; i<LAST_USER_QUEUE; i++)
		if ((gtx->feed[i].state==DMX_STATE_GO) && (gtx->feed[i].pid==pid) && (gtx->feed[i].output&TS_PACKET))
			used++;

	for (i=0; i<LAST_USER_QUEUE; i++)
		if ((gtx->feed[i].state==DMX_STATE_GO) && (gtx->feed[i].pid==pid))
		{
			if (used)
				dmx_enable_tap(&gtx->feed[i]);
			else
				dmx_disable_tap(&gtx->feed[i]);
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

	gtxfeed->readptr=avia_gt_dmx_get_queue_write_pointer(gtxfeed->index);
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
	gtx_demux_feed_t *gtxfeed;

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
	int i;

	dprintk("gtx_dmx: dmx_section_feed_allocate_filter.\n");

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
	filter->pec=0;
#ifdef GTX_SECTIONS
	filter->output=GTX_OUTPUT_8BYTE;
#else
	filter->output=GTX_OUTPUT_TS;
#endif
	dmx_set_filter(gtxfeed->filter);

	return 0;
}

static int dmx_section_feed_start_filtering(dmx_section_feed_t *feed)
{
#ifdef GTX_SECTIONS
	int numflt=0;
	gtx_demux_feed_t *gtxfeed=(gtx_demux_feed_t*)feed;
	gtx_demux_filter_t *filter=gtxfeed->filter;
	gtx_demux_secfilter_t *secfilter;

	gtx_set_filter_definition_table(gtxfeed->secfilter->index, 0, gtxfeed->secfilter->index);
	for (secfilter=gtxfeed->secfilter; secfilter; secfilter=secfilter->next)
	{
		int i;
		gtx_set_filter_parameter_table(secfilter->index, secfilter->filter.filter_mask, secfilter->filter.filter_value, 0, 0);
		for (i=0; i<DMX_MAX_FILTER_SIZE; i++)
			dprintk("gtx_dmx: %02x ", secfilter->filter.filter_mask[i]);
		dprintk("\n");
		for (i=0; i<DMX_MAX_FILTER_SIZE; i++)
			dprintk("gtx_dmx: %02x ", secfilter->filter.filter_value[i]);
		dprintk(" %d -> %d\n", secfilter->index, secfilter->feed->index);
		if (secfilter->index != gtxfeed->secfilter->index+numflt)
			dprintk("gtx_dmx: warning: filter %d is not %d+%d\n", secfilter->index, gtxfeed->secfilter->index, numflt);
		numflt++;
	}

	filter->filt_tab_idx=gtxfeed->secfilter->index;
	filter->no_of_filters=numflt-1;
	dprintk("gtx_dmx: section filtering start (%d filter)\n", numflt);
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
	gtx_demux_feed_t *gtxfeed;

	dprintk("gtx_dmx: dmx_allocate_section_feed.\n");

	if (!(gtxfeed=GtxDmxFeedAlloc(gtx, DMX_TS_PES_OTHER)))
	{
		dprintk("gtx_dmx: couldn't get gtx feed (for section_feed)\n");
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
	(*feed)->allocate_filter=dmx_section_feed_allocate_filter;
	(*feed)->release_filter=dmx_section_feed_release_filter;
	(*feed)->start_filtering=dmx_section_feed_start_filtering;
	(*feed)->stop_filtering=dmx_section_feed_stop_filtering;

	gtxfeed->pes_type=DMX_TS_PES_OTHER;
	gtxfeed->sec_buffer=kmalloc(4096, GFP_KERNEL);
	gtxfeed->sec_recv=0;
	gtxfeed->sec_len=0;
	gtxfeed->sec_ccn=16;

	gtxfeed->output=TS_PACKET;
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
	return 0;
}

static int dmx_release_section_feed (struct dmx_demux_s* demux,	dmx_section_feed_t* feed)
{
	gtx_demux_feed_t *gtxfeed=(gtx_demux_feed_t*)feed;

	dprintk("gtx_dmx: dmx_release_section_feed.\n");

	if (gtxfeed->secfilter)
	{
		dprintk("gtx_dmx: BUSY.\n");
		return -EBUSY;
	}
	kfree(gtxfeed->sec_buffer);
	dmx_release_ts_feed (demux, (dmx_ts_feed_t*)feed);						// free corresponding queue
	return 0;
}

static int dmx_add_frontend (struct dmx_demux_s* demux, dmx_frontend_t* frontend)
{
	gtx_demux_t *gtx=(gtx_demux_t*)demux;
	struct list_head *pos, *head=&gtx->frontend_list;
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
	struct list_head *pos, *head=&gtx->frontend_list;
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
	int i, ptr;
	gtxdemux->users=0;

	gtxdemux->frontend_list.next=
		gtxdemux->frontend_list.prev=
			&gtxdemux->frontend_list;

	for (i=0; i<NUM_PID_FILTER; i++)			// disable all pid filters
		gtx_set_pid_table(i, 0, 1, 0);

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

	return 0;

}

int GtxDmxCleanup(gtx_demux_t *gtxdemux)
{
	dmx_demux_t *dmx=&gtxdemux->dmx;

	if (dmx_unregister_demux(dmx)<0)
		return -1;

	return 0;
}

int __init avia_gt_napi_init(void)
{

	printk("avia_gt_napi: $Id: avia_gt_napi.c,v 1.85 2002/05/05 19:58:13 Jolt Exp $\n");

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
