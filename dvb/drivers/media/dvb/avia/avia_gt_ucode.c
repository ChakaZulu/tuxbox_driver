/*
 * $Id: avia_gt_ucode.c,v 1.12 2004/05/23 10:52:31 derget Exp $
 *
 * AViA eNX/GTX dmx driver (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2002 Florian Schirmer (jolt@tuxbox.org)
 * Copyright (C) 2004 Carsten Juttner (carjay@gmx.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#define __KERNEL_SYSCALLS__

#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/tqueue.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>

#include "demux.h"
#include "dvb_demux.h"

#include "avia_gt.h"
#include "avia_gt_dmx.h"
#include "avia_gt_ucode_firmware.h"
#include "avia_gt_ucode.h"

static sAviaGtInfo *gt_info;
struct avia_gt_ucode_info ucode_info;

static volatile sRISC_MEM_MAP *risc_mem_map;
static volatile u16 *riscram;
static char *ucode;
/* all feeds */
#define AVIA_GT_UCODE_MAXIMUM_FEED_NUMBER	32

/* for double PID setup workaround */
#define AVIA_GT_UCODE_INVALID_PID	0x2000

static sAviaGtFeed ucode_feed[AVIA_GT_UCODE_MAXIMUM_FEED_NUMBER];
/* chosen by both prop ucodes and c-ucodes */
#define AVIA_GT_UCODE_MAXIMUM_SECTION_FILTERS	32
static s8 section_map[AVIA_GT_UCODE_MAXIMUM_SECTION_FILTERS];

/* proprietary interface */
static volatile u16 *pst;
static volatile u16 *ppct;
static sFilter_Definition_Entry filter_definition_table[32];

/* DRAM functions */ 
static void avia_gt_dmx_memcpy16(volatile u16 *dest, const u16 *src, size_t n)
{
	while (n--) {
		*dest++ = *src++;
		mb();
	}
}

static void avia_gt_dmx_memset16(volatile u16 *s, const u16 c, size_t n)
{
	while (n--) {
		*s++ = c;
		mb();
	}
}

static int pid2feedidx(u16 pid,u8 *feedidx){
	u32 idx;
	for (idx=0;idx<AVIA_GT_UCODE_MAXIMUM_FEED_NUMBER;idx++){
		if ((ucode_feed[idx].dest_queue!=-1)&&(ucode_feed[idx].pid==pid)){
			if (feedidx) *feedidx=idx;		
			return 1;
		}
	}
	return 0;
}


static ssize_t avia_gt_dmx_risc_write(volatile void *dest, const void *src, size_t n)
{
	if (((u16 *)dest < riscram) || (&((u16 *)dest)[n >> 1] > &riscram[DMX_RISC_RAM_SIZE])) {
		printk(KERN_CRIT "avia_gt_ucode: invalid risc write destination\n");
		return -EINVAL;
	}

	if (n & 1) {
		printk(KERN_CRIT "avia_gt_ucode: odd size risc writes are not allowed\n");
		return -EINVAL;
	}

	avia_gt_dmx_memset16(dest, 0, n >> 1);
	avia_gt_dmx_memcpy16(dest, src, n >> 1);

	return n;
}

/* Ucode Interface functions */

/* Proprietary Ucodes */

static void prop_ucode_init(void)
{
	u8 queue_nr;

	pst = &riscram[DMX_PID_SEARCH_TABLE];
	ppct = &riscram[DMX_PID_PARSING_CONTROL_TABLE];

	for (queue_nr = 0; queue_nr < 32; queue_nr++) {
		filter_definition_table[queue_nr].and_or_flag = 0;
		filter_definition_table[queue_nr].filter_param_id = queue_nr;
		filter_definition_table[queue_nr].Reserved = 0;
	}
}

/*
Proprietary Design:
	3 tables: pid->flt_def->flt_param
	2nd table points 1:1 to 3rd table
	3rd table is reflected by section_map
	(its entries are the 2nd table's index)
*/

static void prop_ucode_free_section_filter(sAviaGtSection *section)
{
	u8 i;
	u8 nr = section->idx;

	if (!section){
		printk (KERN_CRIT "avia_gt_ucode: trying to free invalid section filter\n");
		return;
	}
	
	if ((nr > 31) || (section_map[nr] != nr)) {
		printk(KERN_CRIT "avia_gt_ucode: trying to free section filters with wrong index!\n");
		return;
	}

	/* check if already setup */
	for (i=0;i<AVIA_GT_UCODE_MAXIMUM_FEED_NUMBER;i++){
		if (ucode_feed[i].section_idx==nr){
			printk(KERN_CRIT "avia_gt_ucode: trying to free still allocated filter\n");
			return;
		}
	}

	/* mark "unused" in section_map */
	while (nr < 32) {
		if (section_map[nr] == section->idx)
			section_map[nr++] = -1;
		else
			break;
	}
}

/* Analyzes the section filters and convert them into an usable format. */
#define MAX_SECTION_FILTERS	16
static u8 prop_ucode_alloc_section_filter(void *f, sAviaGtSection *section)
{
	u8 mask[MAX_SECTION_FILTERS][8];
	u8 value[MAX_SECTION_FILTERS][8];
	u8 mode[MAX_SECTION_FILTERS][8];
	s8 in_use[MAX_SECTION_FILTERS];
	u8 ver_not[MAX_SECTION_FILTERS];
	u8 unchanged[MAX_SECTION_FILTERS];
	u8 new_mask[8];
	u8 new_value[8];
	u8 new_valid;
	u8 not_value[MAX_SECTION_FILTERS];
	u8 not_mask[MAX_SECTION_FILTERS];
	u8 temp_mask;
	u8 and_or;
	u8 anz_not;
	u8 anz_mixed;
	u8 anz_normal;
	u8 compare_len;
	s8 different_bit_index;
	u8 xor = 0;
	u8 not_the_first = 0;
	u8 not_the_second = 0;
	struct dvb_demux_filter *filter = (struct dvb_demux_filter *) f;

	unsigned anz_filters = 0;
	unsigned old_anz_filters;
	unsigned i,j,k;
	signed entry;
	unsigned entry_len;
	signed new_entry;
	unsigned new_entry_len;
	sFilter_Parameter_Entry1 fpe1[MAX_SECTION_FILTERS];
	sFilter_Parameter_Entry2 fpe2[MAX_SECTION_FILTERS];
	sFilter_Parameter_Entry3 fpe3[MAX_SECTION_FILTERS];
	u32 flags;

	/*
	 * Copy and "normalize" the filters. The section-length cannot be filtered.
	 */

	if (!filter)
		return 0;

	while (filter && (anz_filters < MAX_SECTION_FILTERS)) {
		mask[anz_filters][0]  = filter->filter.filter_mask[0];
		/* only masked value bits have a meaning */
		value[anz_filters][0] = filter->filter.filter_value[0] & filter->filter.filter_mask[0];
		/* non-masked bits have no meaning, so mode is reset for them */
		mode[anz_filters][0]  = filter->filter.filter_mode[0] | ~filter->filter.filter_mask[0];

		/* a filter with no masked bits is useless, so not in_use */
		if (mask[anz_filters][0])
			in_use[anz_filters] = 0;
		else
			in_use[anz_filters] = -1;

		for (i = 1; i < 8; i++) {
			mask[anz_filters][i]  = filter->filter.filter_mask[i+2];
			value[anz_filters][i] = filter->filter.filter_value[i+2] & filter->filter.filter_mask[i+2];
			mode[anz_filters][i]  = filter->filter.filter_mode[i+2] | ~filter->filter.filter_mask[i+2];
			in_use[anz_filters] = mask[anz_filters][i] ? i : in_use[anz_filters];
		}

		unchanged[anz_filters] = 1;

		/* 
		 * The ucode is able to single out the version byte as one single not-filter
		 * so we check if there are any neg. filters requested and if so we also check if 
		 * ALL masked bytes are used for negative filtering (if not we need additional filters)
		 */
		if ((mode[anz_filters][3] != 0xFF) && ((mode[anz_filters][3] & mask[anz_filters][3]) == 0)) {
			ver_not[anz_filters] = 1;
			not_value[anz_filters] = value[anz_filters][3];
			not_mask[anz_filters] = mask[anz_filters][3];
		}
		else {
			ver_not[anz_filters] = 0;
		}

		/*
		 * Don't need to filter because one filter does contain a mask with
		 * all bits set to zero.
		 */

		if (in_use[anz_filters] == -1)
			return 0;

		anz_filters++;
		filter = filter->next;
	}

	if (filter) {
		printk(KERN_WARNING "avia_gt_ucode: too many section filters for hw-acceleration in this feed.\n");
		return 0;
	}

	i = anz_filters;

	while (i < MAX_SECTION_FILTERS)
		in_use[i++] = -1;

	/*
	 * Special case: only one section filter in this feed.
	 */

	if (anz_filters == 1) {
		/*
		 * Check whether we need a not filter
		 */

		anz_not = 0;
		anz_mixed = 0;
		anz_normal = 0;
		and_or = 0;	// AND

		for (i = 0; i < 8; i++) {
			if (mode[0][i] != 0xFF) {
				anz_not++;
				if (mode[0][i] & mask[0][i])	/* 0: only neg. */
					anz_mixed++;
			}
			else if (mask[0][i]) {
				anz_normal++;
			}
		}

		/*
		 * Only the byte with the version has a mode != 0xFF.
		 */
		if ((anz_not == 1) && (anz_mixed == 0) && (mode[0][3] != 0xFF)) {
		}

		/*
		 * Mixed mode (both pos. and neg. filter).
		 */
		else if ((anz_not > 0) && (anz_normal > 0)) {
			anz_filters = 2;
			ver_not[1]=0;
			unchanged[1]=1;
			for (i = 0; i < 8; i++) {
				value[1][i] = value[0][i] & ~mode[0][i];
				mask[1][i] = ~mode[0][i];
				mask[0][i] = mask[0][i] & mode[0][i];
				value[0][i] = value[0][i] & mask[0][i];
				in_use[1] = in_use[0];
				not_the_second = 1;
			}
		}
		/*
		 * All relevant bits have mode-bit 0. (neg. filter only)
		 */
		else if (anz_not > 0) {
			not_the_first = 1;
		}
	}

	/*
	 * More than one filter
	 */

	else {
		and_or = 1;	// OR

		/*
		 * Cannot check for "mode-0" bits. Delete them from the mask.
		 */

		for (i = 0; i < anz_filters; i++) {
			in_use[i] = -1;
			for (j = 0; j < 8; j++) {
				mask[i][j] = mask[i][j] & mode[i][j];
				value[i][j] = value[i][j] & mask[i][j];
				if (mask[i][j])	/* pos filter bits ? */
					in_use[i] = j;
			}
			if (in_use[i] == -1)
				return 0;	// We cannot filter. Damn, thats a really bad case.
		}

		/*
		 * Eliminate redundant filters.
		 */

		old_anz_filters = anz_filters + 1;

		while (anz_filters != old_anz_filters) {
			old_anz_filters = anz_filters;
			for (i = 0; (i < MAX_SECTION_FILTERS - 1) && (anz_filters > 1); i++) {
				if (in_use[i] == -1)
					continue;
				for (j = i + 1; j < MAX_SECTION_FILTERS && (anz_filters > 1); j++) {
					if (in_use[j] == -1)
						continue;

					if (in_use[i] < in_use[j])
						compare_len = in_use[i] + 1;
					else
						compare_len = in_use[j] + 1;

					different_bit_index = -1;

					/*
					 * Check wether the filters are equal or different only in
					 * one bit.
					 */

					for (k = 0; k < compare_len; k++) {
						if ((mask[i][k] == mask[j][k]) && (value[i][k] != value[j][k])) {
							if (different_bit_index != -1)
								goto next_check;

							xor = value[i][k] ^ value[j][k];

							if (hweight8(xor) == 1)
								different_bit_index = k;
							else
								goto next_check;
						}
						else {
							goto next_check;
						}
					}

					if (different_bit_index != -1) {
						mask[i][different_bit_index] -= xor;
						value[i][different_bit_index] &= mask[i][different_bit_index];
						if (different_bit_index == in_use[i]) {
							in_use[i] = -1;
							for (k = 0; k < compare_len; k++)
								if (mask[i][k])
									in_use[i] = k;
						}
						else {
							in_use[i] = compare_len - 1;
						}
						if (in_use[i] == -1)
							return 0;	// Uups, eliminated all filters...
					}
					else {
						in_use[i] = compare_len - 1;
					}

					if ((not_value[i] != not_value[j]) || (not_mask[i] != not_mask[j]) || (ver_not[i] != ver_not[j]))
						unchanged[i] = 0;

					k = compare_len;

					while (k < 8) {
						mask[i][k] = 0;
						value[i][k++] = 0;
					}

					in_use[j] = -1;
					anz_filters--;
					continue;

next_check:
					/*
					 * If mask1 has less bits set than mask2 and all bits set in mask1 are set in mask2, too, then
					 * they are redundant if the corresponding bits in both values are equal.
					 */

					new_valid = 1;
					memset(new_mask, 0, sizeof(new_mask));
					memset(new_value, 0, sizeof(new_value));

					for (k = 0; k < compare_len; k++) {
						temp_mask = mask[i][k] & mask[j][k];
						if (((temp_mask == mask[i][k]) ||
						      (temp_mask == mask[j][k])) &&
						      ((value[i][k] & temp_mask) == (value[j][k] & temp_mask)))	{
							new_mask[k] = temp_mask;
							new_value[k] = value[i][k] & temp_mask;
						}
						else {
							new_valid = 0;
							break;
						}

					}
					if (new_valid) {
						memcpy(mask[i], new_mask, 8);
						memcpy(value[i], new_value, 8);
						if ((not_value[i] != not_value[j]) || (not_mask[i] != not_mask[j]) || (ver_not[i] != ver_not[j]))
							unchanged[i] = 0;
						in_use[i] = compare_len - 1;
						in_use[j] = -1;
						anz_filters--;
						continue;
					}
				}
			}
		}
	}

	/*
	 * Now we have anz_filters filters in value and mask. Look for best space
	 * in the filter_param_table.
	 */

	i = 0;
	j = 0;
	entry_len = 33;
	entry = -1;
	new_entry_len = 33;
	new_entry = -1;

	while (i < 32) {
		if (section_map[i] == -1) {
			if (j == 1) {
				new_entry_len++;
			}
			else {
				if ((new_entry_len < entry_len) && (new_entry_len >= anz_filters)) {
					entry_len = new_entry_len;
					entry = new_entry;
				}
				new_entry_len = 1;
				new_entry = i;
				j = 1;
			}
		}
		else {
			j = 0;
		}
		i++;
	}

	if (((entry == -1) && (new_entry != -1) && (new_entry_len >= anz_filters)) ||
	     ((new_entry_len < entry_len) && (new_entry_len >= anz_filters))) {
		entry = new_entry;
		entry_len = new_entry_len;
	}

	if (entry == -1)
		return 0;

	/*
	 * Mark filter_param_table as used.
	 */

	i = entry;

	while (i < entry + anz_filters)
		section_map[i++] = entry;

	/*
	 * Set filter_definition_table.
	 */

	filter_definition_table[entry].and_or_flag = and_or;

	/*
	 * Set filter parameter tables.
	 */

	i = 0;
	j = 0;
	while (j < anz_filters) {
		if (in_use[i] == -1) {
			i++;
			continue;
		}

		fpe1[j].mask_0  = mask[i][0];
		fpe1[j].param_0 = value[i][0];
		fpe1[j].mask_1  = mask[i][1];
		fpe1[j].param_1 = value[i][1];
		fpe1[j].mask_2  = mask[i][2];
		fpe1[j].param_2 = value[i][2];

		fpe2[j].mask_4  = mask[i][4];
		fpe2[j].param_4 = value[i][4];
		fpe2[j].mask_5  = mask[i][5];
		fpe2[j].param_5 = value[i][5];

		fpe3[j].mask_6  = mask[i][6];
		fpe3[j].param_6 = value[i][6];
		fpe3[j].mask_7  = mask[i][7];
		fpe3[j].param_7 = value[i][7];
		fpe3[j].Reserved1 = 0;
		fpe3[j].not_flag = 0;
		fpe3[j].Reserved2 = 0;

		if (unchanged[i] && ver_not[i]) {
			fpe3[j].not_flag_ver_id_byte = 1;
			fpe2[j].mask_3 = not_mask[i];
			fpe2[j].param_3 = not_value[i];
		}
		else {
			fpe3[j].not_flag_ver_id_byte = 0;
			fpe2[j].mask_3  = mask[i][3];
			fpe2[j].param_3 = value[i][3];
		}

		fpe3[j].Reserved3 = 0;
		fpe3[j].Reserved4 = 0;

		j++;
		i++;
	}

	fpe3[0].not_flag = not_the_first;
	fpe3[1].not_flag = not_the_second;

	/* copy to riscram */
	local_irq_save(flags);
	avia_gt_dmx_risc_write(((u8 *) (&risc_mem_map->Filter_Definition_Table)) + (entry & 0xFE), ((u8*) filter_definition_table) + (entry & 0xFE), 2);
	avia_gt_dmx_risc_write(&risc_mem_map->Filter_Parameter_Table1[entry], fpe1, anz_filters * sizeof(sFilter_Parameter_Entry1));
	avia_gt_dmx_risc_write(&risc_mem_map->Filter_Parameter_Table2[entry], fpe2, anz_filters * sizeof(sFilter_Parameter_Entry2));
	avia_gt_dmx_risc_write(&risc_mem_map->Filter_Parameter_Table3[entry], fpe3, anz_filters * sizeof(sFilter_Parameter_Entry3));
	local_irq_restore(flags);

#if 0
if (anz_filters>1){
	{int c;
	filter = (struct dvb_demux_filter *) f;
	printk ("Value:");
	for (c=0;c<10;c++) printk (" 0x%02x ",filter->filter.filter_value[c]);
	printk ("\n Mask:");
	for (c=0;c<10;c++) printk (" 0x%02x ",filter->filter.filter_mask[c]);
	printk ("\n Mode:");
	for (c=0;c<10;c++) printk (" 0x%02x ",filter->filter.filter_mode[c]);
	printk ("\n");}

	printk("I programmed following filters, And-Or: %d:\n",filter_definition_table[entry].and_or_flag);
	for (j = 0; j <anz_filters; j++)
	{
	printk("%d: %02X %02X %02X %02X %02X %02X %02X %02X\n",j,
			fpe1[j].param_0,fpe1[j].param_1,fpe1[j].param_2,
			fpe2[j].param_3,fpe2[j].param_4,fpe2[j].param_5,
			fpe3[j].param_6,fpe3[j].param_7);
		printk("   %02X %02X %02X %02X %02X %02X %02X %02X\n",
			fpe1[j].mask_0,fpe1[j].mask_1,fpe1[j].mask_2,
			fpe2[j].mask_3,fpe2[j].mask_4,fpe2[j].mask_5,
			fpe3[j].mask_6,fpe3[j].mask_7);
		printk("Not-Flag: %d, Not-Flag-Version: %d\n",fpe3[j].not_flag,fpe3[j].not_flag_ver_id_byte);
	}
}
#endif

	/*
	 * Tell caller the allocated filter_definition_table entry and the amount of used filters.
	 */
	section->idx=entry;
	section->filtercount=anz_filters;
	return 1;
}

static int prop_ucode_set_pid_control_table(u8 idx, u8 queue_nr, u8 type, u8 fork, u8 cw_offset, u8 cc, u8 start_up, u8 pec, u8 filt_tab_idx, u8 _psh, u8 no_of_filter)
{
	sPID_Parsing_Control_Entry ppc_entry;
	u8 target_queue_nr;
	u32 flags;

	if (queue_nr > AVIA_GT_DMX_QUEUE_COUNT) {
		printk(KERN_CRIT "avia_gt_ucode: pid control table entry out of bounds (entry=%d)!\n", queue_nr);
		return -EINVAL;
	}

	dprintk(KERN_DEBUG "avia_gt_dmx_set_pid_control_table, entry %d, type %d, fork %d, cw_offset %d, cc %d, start_up %d, pec %d, filt_tab_idx %d, _psh %d\n",
		queue_nr, type, fork, cw_offset, cc, start_up, pec, filt_tab_idx, _psh);

	/* Special case for SPTS audio queue */
	if ((queue_nr == AVIA_GT_DMX_QUEUE_AUDIO) && (type == TS))
		target_queue_nr = AVIA_GT_DMX_QUEUE_VIDEO;
	else
		target_queue_nr = queue_nr;

	target_queue_nr += ucode_info.qid_offset;

	ppc_entry.type = type;
	ppc_entry.QID = target_queue_nr;
	ppc_entry.fork = !!fork;
	ppc_entry.CW_offset = cw_offset;
	ppc_entry.CC = cc;
	ppc_entry._PSH = _psh;
	ppc_entry.start_up = !!start_up;
	ppc_entry.PEC = !!pec;
	ppc_entry.filt_tab_idx = filt_tab_idx;
	//ppc_entry.State = 0;
	ppc_entry.Reserved1 = 0;
	ppc_entry.no_of_filter = no_of_filter;
	/* FIXME: ppc_entry.State = 7; */
	ppc_entry.State = 7;

	local_irq_save(flags);
	avia_gt_dmx_risc_write(&risc_mem_map->PID_Parsing_Control_Table[idx], &ppc_entry, sizeof(ppc_entry));
	local_irq_restore(flags);

	return 0;
}

static int prop_ucode_set_pid_table(u8 entry, u8 wait_pusi, u8 valid, u16 pid)
{
	sPID_Entry pid_entry;
	u32 flags;

	if (entry > 31) {
		printk(KERN_CRIT "avia_gt_ucode: pid search table entry out of bounds (entry=%d)!\n", entry);
		return -EINVAL;
	}

	dprintk(KERN_DEBUG "avia_gt_dmx_set_pid_table, entry %d, wait_pusi %d, valid %d, pid 0x%04X\n", entry, wait_pusi, valid, pid);

	pid_entry.wait_pusi = wait_pusi;
	pid_entry.VALID = !!valid;			// 0 = VALID, 1 = INVALID
	pid_entry.Reserved1 = 0;
	pid_entry.PID = pid;

	local_irq_save(flags);
	avia_gt_dmx_risc_write(&risc_mem_map->PID_Search_Table[entry], &pid_entry, sizeof(pid_entry));
	local_irq_restore(flags);

	return 0;
}

static u8 prop_ucode_alloc_generic_feed(u8 queue_nr, u8 type, sAviaGtSection *section, u16 pid)
{
	u32 flags;
	u8 feed_idx=0;
	local_irq_save(flags);
	for (feed_idx=0;feed_idx<AVIA_GT_UCODE_MAXIMUM_FEED_NUMBER;feed_idx++){
		if (ucode_feed[feed_idx].dest_queue==-1)
			break;
	}
	if (feed_idx==AVIA_GT_UCODE_MAXIMUM_FEED_NUMBER){
		printk (KERN_INFO "avia_gt_ucode: out of feeds\n");
		return 0xff;
	}
	if (section){
		if (section_map[section->idx]==-1){
			printk (KERN_CRIT "avia_gt_ucode: trying to set unallocated section filter\n");
			return 0xff;
		}
		ucode_feed[feed_idx].section_idx=section->idx;
		ucode_feed[feed_idx].filtercount=section->filtercount-1;
	} else {
		ucode_feed[feed_idx].section_idx=0xff;
		ucode_feed[feed_idx].filtercount=0;
	}
	ucode_feed[feed_idx].dest_queue=queue_nr;
	local_irq_restore(flags);
	ucode_feed[feed_idx].pid=AVIA_GT_UCODE_INVALID_PID;
	ucode_feed[feed_idx].type=type;
	/* FIXME: the DVB-API shouldn't allow this to happen, verify why it does */
	if (!pid2feedidx(pid,NULL)){	/* check for pid already setup inside the RISCRAM */
		ucode_feed[feed_idx].pid=pid;
		prop_ucode_set_pid_table(feed_idx,(ucode_info.prop_interface_flags&CAN_WAITPUSI)?1:0,1,pid);
	}
	return feed_idx;
}

static u8 prop_ucode_alloc_section_feed(u8 queue_nr, sAviaGtSection *section, u16 pid)
{
//printk ("prop_alloc_section_feed: q:%d sidx:%d pid:0x%04x\n",queue_nr, section->idx, pid);
	if (!section){
		printk (KERN_ERR "avia_gt_ucode: trying to call alloc_section_feed with invalid section\n");
		return 0xff;
	}
	return prop_ucode_alloc_generic_feed(queue_nr, SECTION, section, pid);
}

static u8 prop_ucode_alloc_feed(u8 queue_nr, u8 type, u16 pid)
{
//printk ("prop_alloc_feed %d %d 0x%04x\n",queue_nr,type,pid);
	return prop_ucode_alloc_generic_feed(queue_nr, type, NULL, pid);
}

static void prop_ucode_start_feed(u8 feed_idx)
{
//printk ("prop_start_feed %d\n",feed_idx);
	if (ucode_feed[feed_idx].dest_queue==-1){
			printk(KERN_CRIT "avia_gt_ucode_start_feed: trying to start unallocated feed\n");
			return;
	}
	prop_ucode_set_pid_control_table(feed_idx,ucode_feed[feed_idx].dest_queue,ucode_info.queue_mode[ucode_feed[feed_idx].type],
								0,0,0,0,1,ucode_feed[feed_idx].section_idx,
								(ucode_feed[feed_idx].type==SECTION)?1:0,ucode_feed[feed_idx].filtercount);
	/* FIXME: see above (double PID setup workaround) */
	if (ucode_feed[feed_idx].pid != AVIA_GT_UCODE_INVALID_PID)
		prop_ucode_set_pid_table(feed_idx,(ucode_info.prop_interface_flags&CAN_WAITPUSI)?1:0,0,ucode_feed[feed_idx].pid);
}

static void prop_ucode_start_queue_feeds(u8 queue_nr)
{
//printk ("prop_start_queue_feeds %d\n",queue_nr);
	u8 fd=0;
	u8 feed_idx;
	for (feed_idx=0;feed_idx<AVIA_GT_UCODE_MAXIMUM_FEED_NUMBER;feed_idx++){
		if (ucode_feed[feed_idx].dest_queue==queue_nr){
			fd=1;
			prop_ucode_start_feed(feed_idx);
		}
	}
	if (!fd) printk(KERN_CRIT "avia_gt_ucode_start_queue_feeds: trying to start queue with no active feeds\n");
}


static void prop_ucode_stop_feed(u8 feed_idx, u8 remove)
{
//printk("prop_stop_feed: idx:%d rem:%d\n",feed_idx,remove);
	if (ucode_feed[feed_idx].dest_queue==-1){
			printk(KERN_CRIT "avia_gt_ucode_start_feed: trying to stop unallocated feed\n");
			return;
	}
	if (remove){
		sAviaGtSection section; /* we only need the index part for removal */
		ucode_feed[feed_idx].dest_queue=-1;
		ucode_feed[feed_idx].pid=0;
		if ((section.idx=ucode_feed[feed_idx].section_idx)!=0xff){
			u8 i;	/* remove section_filter if it's the last one */
			ucode_feed[feed_idx].section_idx=0xff;
			for (i=0;i<AVIA_GT_UCODE_MAXIMUM_FEED_NUMBER;i++){
				if (ucode_feed[i].section_idx==section.idx) break;
			}
			if (i==AVIA_GT_UCODE_MAXIMUM_FEED_NUMBER) prop_ucode_free_section_filter (&section);
		}
	}
	prop_ucode_set_pid_table(feed_idx,1,1,ucode_feed[feed_idx].pid);
}

static void prop_ucode_stop_queue_feeds(u8 queue_nr, u8 remove)
{
//printk ("prop_stop_queue_feeds %d\n",queue_nr);
	u8 fd=0;
	u8 feed_idx;
	for (feed_idx=0;feed_idx<AVIA_GT_UCODE_MAXIMUM_FEED_NUMBER;feed_idx++){
		if (ucode_feed[feed_idx].dest_queue==queue_nr){
			fd=1;
			prop_ucode_stop_feed(feed_idx,remove);
		}
	}
	if (!fd) printk(KERN_CRIT "avia_gt_ucode_start_queue_feeds: trying to stop queue with no active feeds\n");
}

static void prop_ucode_ecd_reset(void)
{
	u32 flags;

	local_irq_save(flags);
	avia_gt_dmx_memset16(&riscram[DMX_CONTROL_WORDS_1], 0, 24);
	avia_gt_dmx_memset16(&riscram[DMX_CONTROL_WORDS_2], 0, 24);
	avia_gt_dmx_memset16(&riscram[DMX_CONTROL_WORDS_3], 0, 16);
	local_irq_restore(flags);
}

static int prop_ucode_ecd_set_key(u8 index, u8 parity, const u8 *cw)
{
	u16 offset;
	u32 flags;

	offset = DMX_CONTROL_WORDS_1 + ((index / 3) << 7) + ((index % 3) << 3) + ((parity ^ 1) << 2);

	local_irq_save(flags);
	avia_gt_dmx_risc_write(&riscram[offset], cw, 8);
	local_irq_restore(flags);

	return 0;
}

static int prop_ucode_ecd_set_pid(u8 index, u16 pid)
{
	u16 offset, control;
	u32 flags;

	for (offset = 0; offset < 0x20; offset++) {
		if ((pst[offset] & 0x1fff) == pid) {
			control = ppct[offset << 1];
			if (((control >> 4) & 7) != index) {
				local_irq_save(flags);
				avia_gt_dmx_memset16(&ppct[offset << 1], 0, 1);
				avia_gt_dmx_memset16(&ppct[offset << 1], (control & 0xff8f) | (index << 4), 1);
				local_irq_restore(flags);
			}
			return 0;
		}
	}

	printk(KERN_DEBUG "avia_gt_ucode: pid %04x not found\n", pid);
	return -EINVAL;
}

void prop_ucode_handle_msgq(struct avia_gt_dmx_queue *queue, void *null)
{
	u8 cmd,byte;
	u8 feedidx;
	u32 flags;
	u32 bytes_avail = queue->bytes_avail(queue);

	if (!bytes_avail) return;
	queue->get_data(queue,&cmd,1,1);

	switch (cmd){
	case DMX_MESSAGE_INIT: /* init */
		queue->get_data(queue,NULL,1,0);
		queue->flush(queue);
		dprintk (KERN_DEBUG "avia_gt_ucode: risc init/reset\n");
		return;

	case DMX_MESSAGE_ADAPTATION: /* private data */
		{sPRIVATE_ADAPTATION_MESSAGE priv;
		if (bytes_avail<5) return;
		queue->get_data(queue,&priv,5,1);
		if (bytes_avail<(5+priv.length)) return;
		queue->get_data(queue,NULL,5+priv.length,0);
		return;
		}

	case DMX_MESSAGE_SYNC_LOSS:
		queue->get_data(queue,NULL,1,0);
		avia_gt_dmx_risc_reset(1);
		queue->flush(queue);
		printk (KERN_INFO "avia_gt_ucode: framer error\n");
		return;

	case DMX_MESSAGE_CC_ERROR: /* CC was lost, restart feed */
		// CC_exp CC_rec PID_H PID_L
		{sCC_ERROR_MESSAGE ccerr;
		if (bytes_avail<5) return;
		queue->get_data(queue,&ccerr,5,0);
		local_irq_save(flags);
		if (pid2feedidx(ccerr.pid,&feedidx)){
			ucode_info.stop_feed(feedidx,0);
			/* reset not needed here, ucode handles this error well */
			//if (avia_gt_chip(ENX)) avia_gt_dmx_risc_reset(1); // TODO: GTX  
			queue->flush(queue);
			ucode_info.start_feed(feedidx);
		}
		local_irq_restore(flags);
		dprintk (KERN_DEBUG "avia_gt_ucode: ccerr: pid 0x%04x in %d\n",ccerr.pid,feedidx);
		return;}
	
	case DMX_MESSAGE_SECTION_COMPLETED: /* section finished, consume */
		{sSECTION_COMPLETED_MESSAGE comp;
		if (bytes_avail<4) return;
		queue->get_data(queue,&comp,4,0);
		//printk ("sect: 0x%04x\n",comp.pid);
		return;}

	default:	/* print what we got */
		queue->get_data(queue,NULL,1,0);
		bytes_avail--;
		dprintk (KERN_DEBUG "avia_gt_ucode: msgq received unknown: 0x%02x: ",cmd);
		while (bytes_avail--){
			queue->get_data(queue,&byte,1,0);
			dprintk (KERN_DEBUG "0x%02x ",byte);
		}
		dprintk (KERN_DEBUG "\n");
	}

	return;
}

// static void set_ucode_info(u8 ucode_flags)
void avia_gt_dmx_set_ucode_info(u8 ucode_flags)
{
	u16 version_no = riscram[DMX_VERSION_NO];
	switch (version_no) {
	case 0x0013:
	case 0x0014:
 		ucode_info.caps = (AVIA_GT_UCODE_CAP_ECD |
 			AVIA_GT_UCODE_CAP_PES |
 			AVIA_GT_UCODE_CAP_SEC |
 			AVIA_GT_UCODE_CAP_TS |
			AVIA_GT_UCODE_CAP_MSGQ);
		ucode_info.prop_interface_flags=CAN_WAITPUSI;
		ucode_info.qid_offset = 1;
		ucode_info.queue_mode[PES] = 3;
 		break;
	case 0x001a:
		ucode_info.caps = (AVIA_GT_UCODE_CAP_ECD |
			AVIA_GT_UCODE_CAP_PES |
			AVIA_GT_UCODE_CAP_TS);
		ucode_info.prop_interface_flags=0;
		ucode_info.qid_offset = 0;
		ucode_info.queue_mode[PES] = 5;
		break;
 	case 0x00F0:
	case 0x00F1:
	case 0x00F2:
	case 0x00F3:
	case 0x00F4:
                ucode_info.caps = (
                        AVIA_GT_UCODE_CAP_PES |
                        AVIA_GT_UCODE_CAP_SEC |
                        AVIA_GT_UCODE_CAP_TS);
		ucode_info.prop_interface_flags=CAN_WAITPUSI;
                ucode_info.qid_offset = 1;
                ucode_info.queue_mode[PES] = 3;
                break;
	case 0xb102:
	case 0xb107:
	case 0xb121:
		ucode_info.caps = (AVIA_GT_UCODE_CAP_PES |
			AVIA_GT_UCODE_CAP_TS);
		ucode_info.prop_interface_flags=0;
		ucode_info.qid_offset = 0;
		ucode_info.queue_mode[PES] = 3;
		break;
	case 0xc001:
		ucode_info.caps = (AVIA_GT_UCODE_CAP_C_INTERFACE |
			AVIA_GT_UCODE_CAP_PES |
			AVIA_GT_UCODE_CAP_SEC |
			AVIA_GT_UCODE_CAP_TS |
			AVIA_GT_UCODE_CAP_MSGQ);
		ucode_info.qid_offset = 0;
		break;
	default:
		ucode_info.caps = 0;
		ucode_info.prop_interface_flags=0;
		if (version_no < 0xa000)
			ucode_info.qid_offset = 1;
		else 
			ucode_info.qid_offset = 0;
		ucode_info.queue_mode[PES]=3;
		break;
	}

	if (ucode_flags&DISABLE_UCODE_SECTION_FILTERING)	// workaround
		ucode_info.caps &= ~AVIA_GT_UCODE_CAP_SEC;

	if ((ucode_info.caps & AVIA_GT_UCODE_CAP_SEC) == AVIA_GT_UCODE_CAP_SEC)
		printk(KERN_INFO "avia_gt_ucode: ucode section filters enabled.\n");
	else
		printk(KERN_INFO "avia_gt_ucode: ucode section filters disabled.\n");
	
	ucode_info.queue_mode[TS] = 0;	/* common for all */
	if (ucode_info.caps&AVIA_GT_UCODE_CAP_C_INTERFACE){
		ucode_info.queue_mode[PES]=1;
		ucode_info.queue_mode[SECTION]=3;
		} else {
		ucode_info.queue_mode[SECTION]=4;
		ucode_info.init = prop_ucode_init;
		ucode_info.alloc_feed = prop_ucode_alloc_feed;
		ucode_info.alloc_section_feed = prop_ucode_alloc_section_feed;

		ucode_info.start_feed = prop_ucode_start_feed;
		ucode_info.start_queue_feeds = prop_ucode_start_queue_feeds;

		ucode_info.stop_feed = prop_ucode_stop_feed;
		ucode_info.stop_queue_feeds = prop_ucode_stop_queue_feeds;

		ucode_info.alloc_section_filter = prop_ucode_alloc_section_filter;
		ucode_info.free_section_filter = prop_ucode_free_section_filter;

		ucode_info.ecd_reset = prop_ucode_ecd_reset;
		ucode_info.ecd_set_key = prop_ucode_ecd_set_key;
		ucode_info.ecd_set_pid = prop_ucode_ecd_set_pid;
		
		ucode_info.handle_msgq = prop_ucode_handle_msgq;
	}
}

struct avia_gt_ucode_info *avia_gt_dmx_get_ucode_info(void)
{
	return &ucode_info;
}


static
void avia_gt_dmx_load_ucode(void)
{
	int fd;
	mm_segment_t fs;
	u8 ucode_fs_buf[2048];
	u16 *ucode_buf = NULL;
	loff_t file_size;
	u32 flags;

	fs = get_fs();
	set_fs(get_ds());

	if ((ucode) && ((fd = open(ucode, 0, 0)) >= 0)) {
		file_size = lseek(fd, 0, 2);
		lseek(fd, 0, 0);

		if ((file_size <= 0) || (file_size > 2048))
			printk(KERN_ERR "avia_gt_ucode: Firmware wrong size '%s'\n", ucode);
		else if (read(fd, ucode_fs_buf, file_size) != file_size)
			printk(KERN_ERR "avia_gt_ucode: Failed to read firmware file '%s'\n", ucode);
		else
			ucode_buf = (u16 *)ucode_fs_buf;
		close(fd);

		/* the proprietary firmwares seem to include already set-up tables
		    so better make sure there are no active feeds */
		if ((ucode_buf) && (file_size >= 0x740))
			for (fd = DMX_PID_SEARCH_TABLE; fd < DMX_PID_PARSING_CONTROL_TABLE; fd++)
				ucode_buf[fd] = 0xdfff;
		}

	set_fs(fs);

	/* use internal ucode if loading failed for any reason */
	if (!ucode_buf) {
		ucode_buf = (u16 *)avia_gt_dmx_ucode_0014_img;
		file_size = avia_gt_dmx_ucode_size;
	}

	local_irq_save(flags);
	avia_gt_dmx_risc_write(risc_mem_map, ucode_buf, file_size);
	if (riscram[DMX_VERSION_NO] == 0xb107)
		avia_gt_dmx_memset16(&riscram[0x80], 0x0000, 4);
	local_irq_restore(flags);

	printk(KERN_INFO "avia_gt_ucode: loaded ucode v%04X\n", riscram[DMX_VERSION_NO]);
}

/* 
 * the eNX is diva-esque and wants a complete
 * reset in case the sync is lost in the framer 
 */
/* TODO: verify for GTX */ 
void avia_gt_dmx_risc_reset(int reenable)
{
	u32 flags;
	local_irq_save(flags);
	if (avia_gt_chip(ENX)) enx_reg_16 (EC) |= 2;
	avia_gt_reg_set(RSTR0,TDMP, 1);
	if (avia_gt_chip(ENX)) avia_gt_reg_set(RSTR0,FRMR, 1);
	if (reenable){
		avia_gt_reg_set(RSTR0,TDMP, 0);
		if (avia_gt_chip(ENX)){
			avia_gt_reg_set(RSTR0,FRMR, 0);
			enx_reg_16(SYNC_HYST) = 0x21;
			enx_reg_16(BQ) = 0x00BC;
			enx_reg_16(FC) = 0x9147;
			enx_reg_16(EC) &= ~3;
		} /* else {
			gtx_reg_16(SYNCH) = 0x21;
			gtx_reg_16(FC) = 0x9147;
		} */
	}
	local_irq_restore(flags);
}

// info holds configuration info from the main module
int avia_gt_dmx_risc_init(sAviaGtDmxRiscInit *risc_info)
{
	gt_info = avia_gt_get_info();
	
	risc_mem_map = avia_gt_reg_o(gt_info->tdp_ram);
	riscram = (volatile u16*) risc_mem_map;

	memset (section_map, 0xff, sizeof(section_map));
	memset (ucode_feed, 0xff, sizeof(ucode_feed));
	
	avia_gt_dmx_risc_reset(0);
	avia_gt_dmx_load_ucode();
	avia_gt_dmx_set_ucode_info(risc_info->ucode_flags);
	ucode_info.init();
	avia_gt_dmx_risc_reset(1);

	if (avia_gt_chip(ENX))
		enx_reg_16(EC) = 0;

	return 0;
}
