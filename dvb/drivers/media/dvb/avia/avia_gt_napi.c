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
 *   $Revision: 1.148 $
 *   $Log: avia_gt_napi.c,v $
 *   Revision 1.148  2002/11/01 22:36:35  Jolt
 *   Basic Soft DMX support
 *
 *   Revision 1.147  2002/10/31 23:29:30  Jolt
 *   Porting
 *
 *   Revision 1.146  2002/10/31 22:36:08  Jolt
 *   Porting
 *
 *   Revision 1.145  2002/10/30 20:12:34  Jolt
 *   Cleanups / Merging
 *
 *   Revision 1.144  2002/10/30 19:46:54  Jolt
 *   Cleanups / Porting
 *
 *   Revision 1.143  2002/10/30 18:58:24  Jolt
 *   First skeleton of the new API
 *
 *   Revision 1.142  2002/10/20 20:38:26  Jolt
 *   Compile fixes
 *
 *   Revision 1.141  2002/10/09 20:20:36  Jolt
 *   DMX & Section fixes
 *
 *   Revision 1.140  2002/10/07 23:12:40  Jolt
 *   Bugfixes
 *
 *   Revision 1.139  2002/10/07 21:13:25  Jolt
 *   Cleanups / Fixes
 *
 *   Revision 1.138  2002/10/07 08:24:14  Jolt
 *   NAPI cleanups
 *
 *   Revision 1.137  2002/10/06 22:05:13  Jolt
 *   NAPI cleanups
 *
 *   Revision 1.136  2002/10/06 21:43:54  Jolt
 *   NAPI cleanups
 *
 *   Revision 1.135  2002/10/06 19:26:23  wjoost
 *   bug--;
 *
 *   Revision 1.134  2002/10/06 18:53:13  wjoost
 *   Debug-Code raus ;)
 *
 *   Revision 1.133  2002/10/06 18:49:02  wjoost
 *   Gleichzeitiger PES und TS-Empfang möglich, wenn TS zuerst gestartet wird.
 *
 *   Revision 1.132  2002/10/05 15:01:12  Jolt
 *   New NAPI compatible VBI interface
 *
 *   Revision 1.131  2002/09/30 19:46:10  Jolt
 *   SPTS support
 *
 *   Revision 1.130  2002/09/18 15:57:24  Jolt
 *   Queue handling changes #3
 *
 *   Revision 1.129  2002/09/18 12:13:20  Jolt
 *   Queue handling changes #2
 *
 *   Revision 1.128  2002/09/18 09:57:42  Jolt
 *   Queue handling changes
 *
 *   Revision 1.127  2002/09/16 21:41:37  wjoost
 *   noch was vergessen
 *
 *   Revision 1.126  2002/09/16 21:35:04  wjoost
 *   BUG hunting
 *
 *   Revision 1.125  2002/09/15 16:27:33  wjoost
 *   SW-Sections: no copy
 *   some fixes
 *
 *   Revision: 1.124
 *   Revision 1.124  2002/09/14 22:04:56  Jolt
 *   NAPI cleanup
 *
 *   Revision 1.123  2002/09/14 18:15:48  Jolt
 *   HW CRC for SW sections
 *
 *   Revision 1.122  2002/09/14 18:03:38  Jolt
 *   NAPI cleanup
 *
 *   Revision 1.121  2002/09/14 14:43:21  Jolt
 *   NAPI cleanup
 *
 *   Revision 1.120  2002/09/13 23:06:27  Jolt
 *   - Directly pass hw sections to napi
 *   - Enable hw crc for hw sections
 *
 *   Revision 1.119  2002/09/13 19:23:40  Jolt
 *   NAPI cleanup
 *
 *   Revision 1.118  2002/09/13 19:00:49  Jolt
 *   Changed queue handling
 *
 *   Revision 1.117  2002/09/12 14:58:52  Jolt
 *   HW sections bugfixes
 *
 *   Revision 1.116  2002/09/10 21:15:34  Jolt
 *   NAPI cleanup
 *
 *   Revision 1.115  2002/09/10 16:31:38  Jolt
 *   SW sections fix
 *
 *   Revision 1.114  2002/09/10 13:44:44  Jolt
 *   DMX/NAPI cleanup
 *
 *   Revision 1.113  2002/09/09 21:59:01  Jolt
 *   HW sections fix
 *
 *   Revision 1.112  2002/09/09 18:30:36  Jolt
 *   Symbol fix
 *
 *   Revision 1.111  2002/09/09 17:46:30  Jolt
 *   Compile fix
 *
 *   Revision 1.110  2002/09/05 12:42:51  Jolt
 *   DMX/NAPI cleanup
 *
 *   Revision 1.109  2002/09/05 12:30:53  Jolt
 *   NAPI cleanup
 *
 *   Revision 1.108  2002/09/05 11:57:44  Jolt
 *   NAPI bugfix
 *
 *   Revision 1.107  2002/09/05 09:40:32  Jolt
 *   - DMX/NAPI cleanup
 *   - Bugfixes (Thanks obi)
 *
 *   Revision 1.106  2002/09/04 22:40:47  Jolt
 *   DMX/NAPI cleanup
 *
 *   Revision 1.105  2002/09/04 22:07:40  Jolt
 *   DMX/NAPI cleanup
 *
 *   Revision 1.104  2002/09/04 21:12:52  Jolt
 *   DMX/NAPI cleanup
 *
 *   Revision 1.103  2002/09/04 13:25:01  Jolt
 *   DMX/NAPI cleanup
 *
 *   Revision 1.102  2002/09/04 07:46:29  Jolt
 *   - Removed GTX_SECTION
 *   - Removed auto pcr pid handling
 *
 *   Revision 1.101  2002/09/03 21:00:34  Jolt
 *   DMX/NAPI cleanup
 *
 *   Revision 1.100  2002/09/03 15:37:50  wjoost
 *   Ein Bug weniger
 *
 *   Revision 1.99  2002/09/03 14:02:05  Jolt
 *   DMX/NAPI cleanup
 *
 *   Revision 1.98  2002/09/03 13:17:34  Jolt
 *   - DMX/NAPI cleanup
 *   - HW sections workaround
 *
 *   Revision 1.97  2002/09/03 05:17:38  Jolt
 *   HW sections workaround
 *
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

#include "../dvb-core/demux.h"
#include "../dvb-core/dvb_demux.h"
#include "../dvb-core/dmxdev.h"

#include "avia_gt.h"
#include "avia_gt_dmx.h"
#include "avia_gt_napi.h"

static sAviaGtInfo *gt_info = (sAviaGtInfo *)NULL;
static struct dvb_demux demux;
gtx_demux_filter_t pid2filter[0x2000];

//#undef dprintk
//#define dprintk printk


#ifdef MODULE
MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("Avia eNX/GTX demux driver");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#endif

void avia_gt_napi_queue_callback(u8 queue_nr, void *data)
{

	sAviaGtDmxQueue *queue = avia_gt_dmx_get_queue_info(queue_nr);
	sAviaGtDmxQueueInfo *queue_info = &queue->info;
	struct dvb_demux_feed *dvbdmxfeed = (struct dvb_demux_feed *)data;
	u32 chunkall;
	u8 ts_buf[188];
	
	chunkall = queue_info->bytes_avail(queue_nr);
	
	if (chunkall < 188)
		return;
		
	if (queue_info->get_data8(queue_nr, 1) != 0x47) {
	
		printk("avia_gt_napi: lost sync on queue %d\n", queue_nr);
	
		queue_info->get_data(queue_nr, NULL, chunkall, 0);
		
		return;
		
	}

	chunkall -= chunkall % 188;
	
	while (chunkall) {
	
		if ((queue->read_pos + 188) > (queue->size)) {
				
			queue_info->get_data(queue_nr, ts_buf, 188, 0);
			dvb_dmx_swfilter_packet(dvbdmxfeed->demux, ts_buf);
					
		} else {

			dvb_dmx_swfilter_packet(dvbdmxfeed->demux, gt_info->mem_addr + queue->mem_addr + queue->read_pos);
				
		}
		
		chunkall -= 188;
		queue_info->get_data(queue_nr, NULL, 188, 0);
		
	}
					
	return;				

}

static void dmx_set_filter(gtx_demux_filter_t *filter)
{

	if (filter->invalid)
		avia_gt_dmx_set_pid_table(filter->index, filter->wait_pusi, filter->invalid, filter->pid);

	avia_gt_dmx_set_pid_control_table(filter->index, filter->output, filter->queue, filter->fork, filter->cw_offset, filter->cc, filter->start_up, filter->pec, 0, 0);

	if (!filter->invalid)
		avia_gt_dmx_set_pid_table(filter->index, filter->wait_pusi, filter->invalid, filter->pid);
		
}

static void dmx_update_pid(u8 queue_nr)
{

	avia_gt_dmx_queue_irq_enable(queue_nr);

}

static int avia_gt_napi_write_to_decoder(struct dvb_demux_feed *dvbdmxfeed, u8 *buf, size_t count)
{

	printk("avia_gt_napi: write_to_decoder\n");

	return 0;

}

static int avia_gt_napi_start_feed(struct dvb_demux_feed *dvbdmxfeed)
{

	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;
	gtx_demux_filter_t *filter = &pid2filter[dvbdmxfeed->pid];
	int queue_nr;

	if (!dvbdmx->dmx.frontend)
		return -EINVAL;

	if ((queue_nr = avia_gt_dmx_alloc_queue_user(NULL, avia_gt_napi_queue_callback, dvbdmxfeed)) < 0)
		return queue_nr;
		
//	printk("avia_gt_napi: got queue %d for pid 0x%X\n", queue_nr, dvbdmxfeed->pid);
		
	filter->output = 0;
	filter->pid = dvbdmxfeed->pid;
	filter->wait_pusi = 0;
	filter->index = queue_nr;
	filter->queue = queue_nr;
	filter->invalid = 1;
	filter->fork = 0;
	filter->cw_offset = 0;
	filter->cc = 0;
	filter->start_up = 0;
	filter->pec = 0;

	dmx_set_filter(filter);

	avia_gt_dmx_queue_reset(filter->queue);

	filter->start_up = 1;
	filter->invalid = 0;
	dmx_set_filter(filter);

	dmx_update_pid(filter->queue);
	
	// TODO
	
	return 0;

}

static int avia_gt_napi_stop_feed(struct dvb_demux_feed *dvbdmxfeed)
{

	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;
	gtx_demux_filter_t *filter = &pid2filter[dvbdmxfeed->pid];

	if (!dvbdmx->dmx.frontend)
		return -EINVAL;

//	printk("avia_gt_napi: closing queue %d for pid 0x%X\n", filter->index, dvbdmxfeed->pid);
	
	filter->invalid = 1;

	dmx_set_filter(filter);

	avia_gt_dmx_queue_irq_disable(filter->queue);
	avia_gt_dmx_queue_reset(filter->queue);
	avia_gt_dmx_free_queue(filter->index);

	return 0;

}

struct dvb_demux *avia_gt_napi_get_demux(void)
{

	return &demux;
	
}

int __init avia_gt_napi_init(void)
{

	printk("avia_gt_napi: $Id: avia_gt_napi.c,v 1.148 2002/11/01 22:36:35 Jolt Exp $\n");

	gt_info = avia_gt_get_info();

	if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {

		printk("avia_gt_napi: Unsupported chip type\n");

		return -EIO;

    }
	
	memset(&demux, 0, sizeof(demux));

	demux.dmx.vendor = "C-Cube";
	demux.dmx.model = "AViA GTX/eNX";
	demux.dmx.id = "demux0_0";
	demux.dmx.capabilities = DMX_TS_FILTERING | DMX_SECTION_FILTERING;

	demux.priv = NULL;	
	demux.filternum = 31;
	demux.feednum = 31;
	demux.start_feed = avia_gt_napi_start_feed;
	demux.stop_feed = avia_gt_napi_stop_feed;
	demux.write_to_decoder = avia_gt_napi_write_to_decoder;
	
	if (dvb_dmx_init(&demux)) {
	
		printk("avia_gt_napi: dvb_dmx_init failed\n");
		
		return -EFAULT;
		
	}

	return 0;

}

void __exit avia_gt_napi_exit(void)
{

	dvb_dmx_release(&demux);

}

#ifdef MODULE
module_init(avia_gt_napi_init);
module_exit(avia_gt_napi_exit);
#endif
