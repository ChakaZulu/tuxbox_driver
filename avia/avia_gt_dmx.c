/*
 *   avia_gt_dmx.c - AViA eNX/GTX dmx driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2002 Florian Schirmer (jolt@tuxbox.org)
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
 *
 *   $Log: avia_gt_dmx.c,v $
 *   Revision 1.92  2002/08/22 13:39:33  Jolt
 *   - GCC warning fixes
 *   - screen flicker fixes
 *   Thanks a lot to Massa
 *
 *   Revision 1.91  2002/07/08 15:12:47  wjoost
 *
 *   ein paar nicht benutzte Felder initialisiert (wie in avia_gt_napi.c 1.88)
 *
 *   Revision 1.90  2002/07/07 16:55:42  wjoost
 *
 *   Meine Sagem hustet mir sonst was :-(
 *
 *   Revision 1.89  2002/06/11 20:35:43  Jolt
 *   Sections cleanup
 *
 *   Revision 1.88  2002/06/07 18:06:03  Jolt
 *   GCC31 fixes 2nd shot (GTX version) - sponsored by Frankster (THX!)
 *
 *   Revision 1.87  2002/06/07 17:53:45  Jolt
 *   GCC31 fixes 2nd shot - sponsored by Frankster (THX!)
 *
 *   Revision 1.86  2002/05/09 07:29:21  waldi
 *   add correct license
 *
 *   Revision 1.85  2002/05/08 03:47:26  obi
 *   changed PRCPID to PCRPID
 *
 *   Revision 1.84  2002/05/07 16:59:19  Jolt
 *   Misc stuff and cleanups
 *
 *   Revision 1.83  2002/05/03 22:09:58  Jolt
 *   Do not require ucodes for framebuffer mode
 *
 *   Revision 1.82  2002/05/03 21:52:58  Jolt
 *   YEAH YEAH YEAH :(
 *
 *   Revision 1.81  2002/05/03 16:45:17  obi
 *   replaced r*() by gtx_reg_*()
 *   formatted source
 *
 *   Revision 1.80  2002/05/02 21:26:01  Jolt
 *   Test
 *
 *   Revision 1.79  2002/05/02 20:23:10  Jolt
 *   Fixes
 *
 *   Revision 1.78  2002/05/02 12:37:35  Jolt
 *   Merge
 *
 *   Revision 1.77  2002/05/02 04:56:47  Jolt
 *   Merge
 *
 *   Revision 1.76  2002/05/01 21:53:00  Jolt
 *   Merge
 *
 *
 *
 *
 *   $Revision: 1.92 $
 *
 */

#define __KERNEL_SYSCALLS__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/byteorder/swab.h>

#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/unistd.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/tqueue.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>

#include <ost/demux.h>
#include <dbox/avia_gt.h>
#include <dbox/avia_gt_dmx.h>
#include <dbox/avia_gt_napi.h>

static int						 errno							= (int)0;
static sAviaGtInfo		*gt_info						= (sAviaGtInfo *)NULL;
static sRISC_MEM_MAP	*risc_mem_map				= (sRISC_MEM_MAP *)NULL;
static char						*ucode							= (char *)NULL;
static int						 discont						= (int)1;
static int						 large_delta_count	= (int)0;
static int						 deltaClk_max				= (int)0;
static int						 deltaClk_min				= (int)0;
static int						 deltaPCR_AVERAGE		= (int)0;
static Pcr_t					 oldClk;
extern void avia_set_pcr(u32 hi, u32 lo);
static void gtx_pcr_interrupt(unsigned short irq);

#ifdef MODULE
MODULE_PARM(ucode, "s");
#endif

unsigned char avia_gt_dmx_map_queue(unsigned char queue_nr)
{

	if (avia_gt_chip(ENX)) {

		if (queue_nr >= 16)
			enx_reg_set(CFGR0, UPQ, 1);
		else
			enx_reg_set(CFGR0, UPQ, 0);

	} else if (avia_gt_chip(GTX)) {

		if (queue_nr >= 16)
			gtx_reg_set(CR1, UPQ, 1);
		else
			gtx_reg_set(CR1, UPQ, 0);

	}

	mb();

	return (queue_nr & 0x0F);

}

void avia_gt_dmx_force_discontinuity(void)
{

	discont = 1;

	if (avia_gt_chip(ENX))
		enx_reg_16(FC) |= 0x100;
	else if (avia_gt_chip(GTX))
		gtx_reg_16(FCR) |= 0x100;

}

unsigned char avia_gt_dmx_get_queue_size(unsigned char queue_nr)
{

	queue_nr = avia_gt_dmx_map_queue(queue_nr);

	if (avia_gt_chip(ENX))
		return enx_reg_so(QWPnH, 4 * queue_nr)->Q_Size;
	else if (avia_gt_chip(GTX))
		return gtx_reg_so(QWPnH, 4 * queue_nr)->Q_Size;

	return 0;

}

unsigned int avia_gt_dmx_get_queue_write_pointer(unsigned char queue_nr)
{

	unsigned int previous_write_pointer;
	unsigned int write_pointer = 0xFFFFFFFF;

	queue_nr = avia_gt_dmx_map_queue(queue_nr);

    /*
     *
     * CAUTION: The correct sequence for accessing Queue
     * Pointer registers is as follows:
     * For reads,
     * Read low word
     * Read high word
     * CAUTION: Not following these sequences will yield
     * invalid data.
     *
     */

	do {

		previous_write_pointer = write_pointer;

		if (avia_gt_chip(ENX))
			write_pointer = ((enx_reg_so(QWPnL, 4 * queue_nr)->Queue_n_Write_Pointer) | (enx_reg_so(QWPnH, 4 * queue_nr)->Queue_n_Write_Pointer << 16));
		else if (avia_gt_chip(GTX))
			write_pointer = ((gtx_reg_so(QWPnL, 4 * queue_nr)->Queue_n_Write_Pointer) | (gtx_reg_so(QWPnH, 4 * queue_nr)->Upper_WD_n << 16));

	} while (previous_write_pointer != write_pointer);

	return write_pointer;

}

int avia_gt_dmx_load_ucode(void)
{

	int fd = (int)0;
	loff_t file_size;
	mm_segment_t fs;

	fs = get_fs();
	set_fs(get_ds());

	if ((fd = open(ucode, 0, 0)) < 0) {

		printk (KERN_ERR "avia_gt_dmx: Unable to load firmware file '%s'\n", ucode);

		set_fs(fs);

		return -EFAULT;

	}

	file_size = lseek(fd, 0L, 2);

	if (file_size <= 0) {

		printk (KERN_ERR "avia_gt_dmx: Firmware wrong size '%s'\n", ucode);

		sys_close(fd);
		set_fs(fs);

		return -EFAULT;

	}

	lseek(fd, 0L, 0);

	if (read(fd, (void *)risc_mem_map, file_size) != file_size) {

		printk (KERN_ERR "avia_gt_dmx: Failed to read firmware file '%s'\n", ucode);

		sys_close(fd);
		set_fs(fs);

		return -EIO;

	}

	close(fd);
	set_fs(fs);
	
	printk("avia_gt_dmx: Successfully loaded ucode V%X.%X\n", risc_mem_map->Version_no[0], risc_mem_map->Version_no[1]);
	printk("avia_gt_dmx: RISC mem map size: %d\n", sizeof(sRISC_MEM_MAP));

	return 0;

}

int avia_gt_dmx_set_filter_definition_table(u8 entry, u8 and_or_flag, u8 filter_param_id)
{
	sFilter_Definition_Entry e;

	if (entry > 31) {
	
		printk("avia_gt_napi: pid control table entry out of bounce (entry=%d)!\n", entry);
		
		return -EINVAL;
	
	}

	e.and_or_flag = !!and_or_flag;
	e.filter_param_id = filter_param_id;
	e.Reserved = 0;
	*((u8 *) &risc_mem_map->Filter_Definition_Table[entry]) = *((u8 *) &e); 
	
	return 0;
	
}

int avia_gt_dmx_set_filter_parameter_table(u8 entry, u8 mask[8], u8 param[8], u8 not_flag, u8 not_flag_ver_id_byte)
{
	sFilter_Parameter_Entry1 e1;
	sFilter_Parameter_Entry2 e2;
	sFilter_Parameter_Entry3 e3;

	if (entry > 31) {
	
		printk("avia_gt_napi: pid control table entry out of bounce (entry=%d)!\n", entry);
		
		return -EINVAL;
	
	}

	e1.mask_0 = mask[0];
	e1.param_0 = param[0];
	e1.mask_1 = mask[1];
	e1.param_1 = param[1];
	e1.mask_2 = mask[2];
	e1.param_2 = param[2];

	e2.mask_3 = mask[3];
	e2.param_3 = param[3];
	e2.mask_4 = mask[4];
	e2.param_4 = param[4];
	e2.mask_5 = mask[5];
	e2.param_5 = param[5];

	e3.mask_6 = mask[6];
	e3.param_6 = param[6];
	e3.mask_7 = mask[7];
	e3.param_7 = param[7];
	e3.not_flag = !!not_flag;
	e3.not_flag_ver_id_byte = !!not_flag_ver_id_byte;
	e3.Reserved1 = 0;
	e3.Reserved2 = 0;
	e3.Reserved3 = 0;
	e3.Reserved4 = 0;

	*((u16 *) &risc_mem_map->Filter_Parameter_Table1[entry]) = *((u16 *) &e1);
	*(((u16 *) &risc_mem_map->Filter_Parameter_Table1[entry]) + 1) = *(((u16 *) &e1) + 1);
	*(((u16 *) &risc_mem_map->Filter_Parameter_Table1[entry]) + 2) = *(((u16 *) &e1) + 2);
	*((u16 *) &risc_mem_map->Filter_Parameter_Table2[entry]) = *((u16 *) &e2);
	*(((u16 *) &risc_mem_map->Filter_Parameter_Table2[entry]) + 1) = *(((u16 *) &e2) + 1);
	*(((u16 *) &risc_mem_map->Filter_Parameter_Table2[entry]) + 2) = *(((u16 *) &e2) + 2);
	*((u16 *) &risc_mem_map->Filter_Parameter_Table3[entry]) = *((u16 *) &e3);
	*(((u16 *) &risc_mem_map->Filter_Parameter_Table3[entry]) + 1) = *(((u16 *) &e3) + 1);
	*(((u16 *) &risc_mem_map->Filter_Parameter_Table3[entry]) + 2) = *(((u16 *) &e3) + 2);

	return 0;
	
}

void avia_gt_dmx_set_pcr_pid(u16 pid)
{

	if (avia_gt_chip(ENX)) {

		//enx_reg_set(PCR_PID, E, 0);
		//enx_reg_set(PCR_PID, PID, pid);
		//enx_reg_set(PCR_PID, E, 1);

		enx_reg_16(PCR_PID) = (1 << 13) | pid;

		avia_gt_free_irq(ENX_IRQ_PCR);
		avia_gt_alloc_irq(ENX_IRQ_PCR, gtx_pcr_interrupt);			 // pcr reception

	} else if (avia_gt_chip(GTX)) {

		gtx_reg_16(PCRPID) = (1 << 13) | pid;
		avia_gt_free_irq(GTX_IRQ_PCR);
		avia_gt_alloc_irq(GTX_IRQ_PCR, gtx_pcr_interrupt);			 // pcr reception

	}

	avia_gt_dmx_force_discontinuity();

}

int avia_gt_dmx_set_pid_control_table(u8 entry, u8 type, u8 queue, u8 fork, u8 cw_offset, u8 cc, u8 start_up, u8 pec, u8 filt_tab_idx, u8 no_of_filter)
{
	sPID_Parsing_Control_Entry e;

	if (entry > 31) {
	
		printk("avia_gt_dmx: pid control table entry out of bounce (entry=%d)!\n", entry);
		
		return -EINVAL;
	
	}

	if (risc_mem_map->Version_no[0] < 0xA0)
		queue++;

	e.type = type;
	e.QID = queue;
	e.fork = !!fork;
	e.CW_offset = cw_offset;
	e.CC = cc;
	e._PSH = 0;
	e.start_up = !!start_up;
	e.PEC = !!pec;
	e.filt_tab_idx = filt_tab_idx;
	e.no_of_filter = no_of_filter;
	e.State = 0;

	*((u32 *) &risc_mem_map->PID_Parsing_Control_Table[entry]) = *((u32 *) &e);

	return 0;
	
}

int avia_gt_dmx_set_pid_table(u8 entry, u8 wait_pusi, u8 valid, u16 pid)
{
	sPID_Entry e;

	if (entry > 31) {
	
		printk("avia_gt_dmx: pid search table entry out of bounce (entry=%d)!\n", entry);
		
		return -EINVAL;
	
	}

	e.wait_pusi = wait_pusi;
	e.VALID = !!valid;			// 0 = VALID, 1 = INVALID
	e.Reserved1 = 0;
	e.PID = pid;
	*((u16 *) &risc_mem_map->PID_Search_Table[entry]) = *((u16 *) &e);
	
	return 0;

}

void avia_gt_dmx_set_queue(unsigned char queue_nr, unsigned int write_pointer, unsigned char size)
{

	queue_nr = avia_gt_dmx_map_queue(queue_nr);

	if (queue_nr > 30) {

		printk("avia_gt_dmx: set_queue queue (%d) out of bounce\n", queue_nr);

		return;

	}

	if (avia_gt_chip(ENX)) {

		enx_reg_16(QWPnL + 4 * queue_nr) = write_pointer & 0xFFFF;
		enx_reg_16(QWPnH + 4 * queue_nr) = ((write_pointer >> 16) & 0x3F) | (size << 6);

	} else if (avia_gt_chip(GTX)) {

		gtx_reg_16(QWPnL + 4 * queue_nr) = write_pointer & 0xFFFF;
		gtx_reg_16(QWPnH + 4 * queue_nr) = ((write_pointer >> 16) & 0x3F) | (size << 6);

	}

}

void avia_gt_dmx_set_queue_read_pointer(unsigned char queue_nr, unsigned int read_pointer)
{

	queue_nr = avia_gt_dmx_map_queue(queue_nr);

	if (queue_nr > 2) {

		printk("avia_gt_dmx: read_pointer queue (%d) out of bounce\n", queue_nr);

		return;

	}

	if (avia_gt_chip(ENX)) {

		enx_reg_16(QWPnL + 4 * queue_nr) = read_pointer & 0xFFFF;
		enx_reg_16(QWPnH + 4 * queue_nr) = (read_pointer >> 16) & 0x3F;

	} else if (avia_gt_chip(GTX)) {

		gtx_reg_16(QWPnL + 4 * queue_nr) = read_pointer & 0xFFFF;
		gtx_reg_16(QWPnH + 4 * queue_nr) = (read_pointer >> 16) & 0x3F;

	}

}

void avia_gt_dmx_set_queue_write_pointer(unsigned char queue_nr, unsigned int write_pointer)
{

	avia_gt_dmx_set_queue(queue_nr, write_pointer, avia_gt_dmx_get_queue_size(queue_nr));

}

void avia_gt_dmx_set_queue_irq(unsigned char queue_nr, unsigned char qim, unsigned int irq_addr)
{

	if (!qim)
		irq_addr = 0;

	queue_nr = avia_gt_dmx_map_queue(queue_nr);

	if (avia_gt_chip(ENX))
		enx_reg_16n(0x08C0 + queue_nr * 2) = ((qim << 15) | (irq_addr & 0x7C00));
	else if (avia_gt_chip(GTX))
		gtx_reg_16(QIn + queue_nr * 2) = ((qim << 15) | (irq_addr & 0x7C00));

}

void gtx_set_queue_pointer(int queue, u32 read, u32 write, int size, int halt)
{
	int	base = (int)0;

	if (avia_gt_chip(ENX)) {

		base = queue * 8 + 0x8E0;

		enx_reg_16n(base) = read & 0xFFFF;
		enx_reg_16n(base + 4) = write & 0xFFFF;
		enx_reg_16n(base + 6) = ((write >> 16) & 63) | (size << 6);
		enx_reg_16n(base + 2) = ((read >> 16) & 63);

	} else if (avia_gt_chip(GTX)) {

		base = queue * 8 + 0x1E0;

		gtx_reg_16n(base) = read & 0xFFFF;
		gtx_reg_16n(base + 4) = write & 0xFFFF;
		gtx_reg_16n(base + 6) = ((write >> 16) & 63) | (size << 6);
		gtx_reg_16n(base + 2) = ((read >> 16) & 63) | (halt << 15);

	}

}

void avia_gt_dmx_reset(unsigned char reenable)
{

	if (avia_gt_chip(ENX)) {

		enx_reg_set(RSTR0, TDMP, 1);

	} else if (avia_gt_chip(GTX)) {

		gtx_reg_set(RR1, RISC, 1);

	}

	if (reenable)
	{
		if (avia_gt_chip(ENX)) {

			enx_reg_set(RSTR0, TDMP, 0);

		} else if (avia_gt_chip(GTX)) {

			gtx_reg_set(RR1, RISC, 0);

		}

	}

}

int avia_gt_dmx_risc_init(void)
{

	avia_gt_dmx_reset(0);

	if (avia_gt_dmx_load_ucode()) {

		printk("avia_gt_dmx: No valid firmware found! TV mode disabled.\n");

		return 0;

	}

	if (avia_gt_chip(ENX)) {

		avia_gt_dmx_reset(1);
		enx_reg_32(RSTR0) &= ~(1 << 22); //clear tdp-reset bit
		//enx_tdp_trace();
		enx_reg_16(EC) = 0;

	} else if (avia_gt_chip(GTX)) {

		avia_gt_dmx_reset(1);

	}

	return 0;

}

void enx_tdp_stop(void)
{

	enx_reg_32(EC) = 2;			//stop tdp

}

static Pcr_t gtx_read_transport_pcr(void)
{

	Pcr_t pcr;

	pcr.lo = 0;
	pcr.hi = 0;

	if (avia_gt_chip(ENX)) {

		pcr.hi = (enx_reg_16(TP_PCR_2) << 16) | enx_reg_16(TP_PCR_1);
		pcr.lo = enx_reg_16(TP_PCR_0) & 0x8000;

	} else if (avia_gt_chip(GTX)) {

		pcr.hi = (gtx_reg_16(PCR2) << 16) | gtx_reg_16(PCR1);
		pcr.lo = gtx_reg_16(PCR0) & 0x81FF;

	}

	return pcr;

}

static Pcr_t avia_gt_dmx_get_latched_stc(void)
{

	Pcr_t pcr;

	pcr.lo = 0;
	pcr.hi = 0;

	if (avia_gt_chip(ENX)) {

		pcr.hi = (enx_reg_s(LC_STC_2)->Latched_STC_Base << 16) | enx_reg_s(LC_STC_1)->Latched_STC_Base;
		pcr.lo = enx_reg_s(LC_STC_0)->Latched_STC_Base << 15;

	} else if (avia_gt_chip(GTX)) {

		pcr.hi = (gtx_reg_16(LSTC2) << 16) | gtx_reg_16(LSTC1);
		pcr.lo = gtx_reg_16(LSTC0) & 0x81FF;

	}

	return pcr;

}

Pcr_t avia_gt_dmx_get_stc(void)
{

	Pcr_t pcr;

	pcr.lo = 0;
	pcr.hi = 0;

	if (avia_gt_chip(ENX)) {

		pcr.hi = (enx_reg_s(STC_COUNTER_2)->STC_Count << 16) | enx_reg_s(STC_COUNTER_1)->STC_Count;
		pcr.lo = enx_reg_s(STC_COUNTER_0)->STC_Count << 15;

	} else if (avia_gt_chip(GTX)) {

		pcr.hi = (gtx_reg_16(STCC2) << 16) | gtx_reg_16(STCC1);
		pcr.lo = gtx_reg_16(STCC0) & 0x81FF;

	}

	return pcr;

}

static void gtx_set_pcr(Pcr_t pcr)
{

	if (avia_gt_chip(ENX)) {

		enx_reg_set(STC_COUNTER_2, STC_Count, (pcr.hi >> 16));
		enx_reg_set(STC_COUNTER_1, STC_Count, (pcr.hi & 0xFFFF));
		enx_reg_16(STC_COUNTER_0) = pcr.lo & 0x81FF;

	} else if (avia_gt_chip(GTX)) {

		gtx_reg_16(STCC2) = pcr.hi >> 16;
		gtx_reg_16(STCC1) = pcr.hi & 0xFFFF;
		gtx_reg_16(STCC0) = pcr.lo & 0x81FF;

	}

}

static s32 gtx_calc_diff(Pcr_t clk1, Pcr_t clk2)
{

	s32 delta;

	delta = (s32) clk1.hi;
	delta -= (s32) clk2.hi;
	delta *= (s32) 2;
	delta += (s32) ((clk1.lo>>15)&1);
	delta -= (s32) ((clk2.lo>>15)&1);
	delta *= (s32) 300;
	delta += (s32) (clk1.lo & 0x1FF);
	delta -= (s32) (clk2.lo & 0x1FF);

	return delta;

}

static s32 gtx_bound_delta(s32 bound, s32 delta)
{

	if (delta > bound)
		delta = bound;

	if (delta < -bound)
		delta = -bound;

	return delta;

}

static void gtx_pcr_interrupt(unsigned short irq)
{
	Pcr_t TPpcr;
	Pcr_t latchedClk;
	Pcr_t currentClk;
	s32 delta_PCR_AV;

	s32 deltaClk, elapsedTime;

	TPpcr=gtx_read_transport_pcr();
	latchedClk = avia_gt_dmx_get_latched_stc();
	currentClk = avia_gt_dmx_get_stc();

	if (discont) {

		oldClk = currentClk;
		large_delta_count = 0;
		dprintk(/* KERN_DEBUG*/	"gtx_dmx: we have a discont:\n");
		dprintk(KERN_DEBUG "gtx_dmx: new stc: %08x%08x\n", TPpcr.hi, TPpcr.lo);
		deltaPCR_AVERAGE = 0;

		avia_gt_dmx_force_discontinuity();
		discont = 0;

		gtx_set_pcr(TPpcr);
		avia_set_pcr(TPpcr.hi, TPpcr.lo);

		return;

	}

	elapsedTime = latchedClk.hi - oldClk.hi;

	if (elapsedTime > TIME_HI_THRESHOLD)
	{
		dprintk("gtx_dmx: elapsed time disc.\n");
		goto WE_HAVE_DISCONTINUITY;
	}

	deltaClk=TPpcr.hi-latchedClk.hi;

	if ((deltaClk < -MAX_HI_DELTA) || (deltaClk > MAX_HI_DELTA))
	{
		dprintk("gtx_dmx: large_delta disc.\n");
		if (large_delta_count++>MAX_DELTA_COUNT)
			goto WE_HAVE_DISCONTINUITY;
		return;
	} else
		large_delta_count=0;

	deltaClk=gtx_calc_diff(TPpcr, latchedClk);
	deltaPCR_AVERAGE*=99;
	deltaPCR_AVERAGE+=deltaClk*10;
	deltaPCR_AVERAGE/=100;

	if ((deltaPCR_AVERAGE < -0x20000) || (deltaPCR_AVERAGE > 0x20000))
	{
		dprintk("gtx_dmx: tmb pcr\n");
		goto WE_HAVE_DISCONTINUITY;
	}

	delta_PCR_AV=deltaClk-deltaPCR_AVERAGE/10;
	if (delta_PCR_AV > deltaClk_max)
		deltaClk_max=delta_PCR_AV;
	if (delta_PCR_AV < deltaClk_min)
		deltaClk_min=delta_PCR_AV;

	elapsedTime=gtx_calc_diff(latchedClk, oldClk);

	if (elapsedTime > TIME_THRESHOLD)
		goto WE_HAVE_DISCONTINUITY;

//	printk("elapsed %08x, delta %c%08x\n", elapsedTime, deltaClk<0?'-':'+', deltaClk<0?-deltaClk:deltaClk);
//	printk("%x (%x)\n", deltaClk, gtx_bound_delta(MAX_DAC, deltaClk));
/*	deltaClk=gtx_bound_delta(MAX_DAC, deltaClk);

	rw(DPCR)=((-deltaClk)<<16)|0x0009; */

    if (avia_gt_chip(ENX)) {

	// Fix gegen Horizontales "Zucken" (von chkdesign):
	//	deltaClk=-gtx_bound_delta(MAX_DAC, deltaClk*1);
	deltaClk=-gtx_bound_delta(MAX_DAC, deltaClk*16);

	enx_reg_16(DAC_PC)=deltaClk;
	enx_reg_16(DAC_CP)=9;

    } else if (avia_gt_chip(GTX)) {

	deltaClk=-gtx_bound_delta(MAX_DAC, deltaClk*16);

	gtx_reg_16(DPCR) = (deltaClk<<16)|9;

    }

	oldClk=latchedClk;
	return;
WE_HAVE_DISCONTINUITY:
	dprintk("gtx_dmx: WE_HAVE_DISCONTINUITY\n");
	discont=1;
}



int __init avia_gt_dmx_init(void)
{

	int result = (int)0;

	printk("avia_gt_dmx: $Id: avia_gt_dmx.c,v 1.92 2002/08/22 13:39:33 Jolt Exp $\n");

	gt_info = avia_gt_get_info();

	if ((!gt_info) || ((!avia_gt_chip(ENX)) && (!avia_gt_chip(GTX)))) {

		printk("avia_gt_dmx: Unsupported chip type\n");

		return -EIO;

	}

//	avia_gt_dmx_reset(1);


	if (avia_gt_chip(ENX)) {

		enx_reg_32(RSTR0)|=(1<<31)|(1<<23)|(1<<22);

		risc_mem_map = (sRISC_MEM_MAP *)enx_reg_o(TDP_INSTR_RAM);

	} else if (avia_gt_chip(GTX)) {

		risc_mem_map = (sRISC_MEM_MAP *)gtx_reg_o(GTX_REG_RISC);

	}

	if ((result = avia_gt_dmx_risc_init()))
		return result;

	if (avia_gt_chip(ENX)) {

		enx_reg_32(RSTR0) &= ~(1 << 27);
		enx_reg_32(RSTR0) &= ~(1 << 13);
		enx_reg_32(RSTR0) &= ~(1 << 11);
		enx_reg_32(RSTR0) &= ~(1 << 9);
		enx_reg_32(RSTR0) &= ~(1 << 23);
		enx_reg_32(RSTR0) &= ~(1 << 31);

		enx_reg_32(CFGR0) &= ~(1 << 3);
		enx_reg_32(CFGR0) &= ~(1 << 1);
		enx_reg_32(CFGR0) &= ~(1 << 0);

		enx_reg_16(FC) = 0x9147;
		enx_reg_16(SYNC_HYST) =0x21;
		enx_reg_16(BQ) = 0x00BC;

		enx_reg_32(CFGR0) |= 1 << 24;		// enable dac output

		enx_reg_16(AVI_0) = 0xF;					// 0x6CF geht nicht (ordentlich)
		enx_reg_16(AVI_1) = 0xA;

		enx_reg_32(CFGR0) &= ~3;				// disable clip mode

		printk("ENX-INITed -> %x\n", enx_reg_16(FIFO_PDCT));

		if (!enx_reg_16(FIFO_PDCT))
			printk("there MIGHT be no TS :(\n");

	} else if (avia_gt_chip(GTX)) {

	//	rh(RR1)&=~0x1C;							 // take framer, ci, avi module out of reset
		gtx_reg_set(RR1, DAC, 1);
		gtx_reg_set(RR1, DAC, 0);

		gtx_reg_16(RR0) = 0;						// autsch, das muss so. kann das mal wer überprüfen?
		gtx_reg_16(RR1) = 0;
		gtx_reg_16(RISCCON) = 0;

		gtx_reg_16(FCR) = 0x9147;							 // byte wide input
		gtx_reg_16(SYNCH) = 0x21;

		gtx_reg_16(AVI) = 0x71F;
		gtx_reg_16(AVI+2) = 0xF;

	}

	return 0;

}

void __exit avia_gt_dmx_exit(void)
{

	avia_gt_dmx_reset(0);

}

#ifdef MODULE
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

EXPORT_SYMBOL(avia_gt_dmx_force_discontinuity);
EXPORT_SYMBOL(avia_gt_dmx_get_queue_size);
EXPORT_SYMBOL(avia_gt_dmx_get_queue_write_pointer);
EXPORT_SYMBOL(avia_gt_dmx_set_filter_definition_table);
EXPORT_SYMBOL(avia_gt_dmx_set_filter_parameter_table);
EXPORT_SYMBOL(avia_gt_dmx_set_pcr_pid);
EXPORT_SYMBOL(avia_gt_dmx_set_pid_control_table);
EXPORT_SYMBOL(avia_gt_dmx_set_pid_table);
EXPORT_SYMBOL(avia_gt_dmx_set_queue);
EXPORT_SYMBOL(avia_gt_dmx_set_queue_irq);
EXPORT_SYMBOL(avia_gt_dmx_set_queue_write_pointer);
EXPORT_SYMBOL(gtx_set_queue_pointer);
#endif

#if defined(MODULE) && defined(STANDALONE)
module_init(avia_gt_dmx_init);
module_exit(avia_gt_dmx_exit);
#endif
