/*
 * $Id: avia_gt_ucode.h,v 1.2 2004/04/07 23:17:45 carjay Exp $
 *
 * AViA eNX/GTX dmx driver (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2002 Florian Schirmer (jolt@tuxbox.org)
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

#ifndef _AVIA_GT_UCODE
#define _AVIA_GT_UCODE

#define AVIA_GT_DMX_QUEUE_MODE_TS		0
#define AVIA_GT_DMX_QUEUE_MODE_PRIVATE		2
/* depends on ucode version
#define AVIA_GT_DMX_QUEUE_MODE_PES		3
*/
#define AVIA_GT_DMX_QUEUE_MODE_SEC8		4
#define AVIA_GT_DMX_QUEUE_MODE_SEC16		5

#define AVIA_GT_UCODE_CAP_ECD			0x0001
#define AVIA_GT_UCODE_CAP_PES			0x0002
#define AVIA_GT_UCODE_CAP_SEC			0x0004
#define AVIA_GT_UCODE_CAP_TS				0x0008

// flags for ucode
enum {
	DISABLE_UCODE_SECTION_FILTERING=1	// 0x01
};

typedef struct {
	u8 ucode_flags;
} sAviaGtDmxRiscInit;

struct avia_gt_ucode_info {
	u32 caps;
	u8 qid_offset;
	u8 queue_mode_pes;
};

#pragma pack(1)

typedef struct {
	unsigned wait_pusi		: 1;
	unsigned VALID			: 1;
	unsigned Reserved1		: 1;
	unsigned PID			: 13;
} sPID_Entry;

typedef struct {
	unsigned type			: 3;
	unsigned QID			: 5;
	unsigned fork			: 1;
	unsigned CW_offset		: 3;
	unsigned CC			: 4;
	unsigned _PSH			: 1;
	unsigned start_up		: 1;
	unsigned PEC			: 1;
	unsigned filt_tab_idx		: 5;
	unsigned State			: 3;
	unsigned Reserved1		: 1;
	unsigned no_of_filter		: 4;
} sPID_Parsing_Control_Entry;

typedef struct {
	unsigned and_or_flag		: 1;
	unsigned filter_param_id	: 5;
	unsigned Reserved		: 2;
} sFilter_Definition_Entry;

typedef struct {
	unsigned mask_0			: 8;
	unsigned param_0		: 8;
	unsigned mask_1			: 8;
	unsigned param_1		: 8;
	unsigned mask_2			: 8;
	unsigned param_2		: 8;
} sFilter_Parameter_Entry1;

typedef struct {
	unsigned mask_3			: 8;
	unsigned param_3		: 8;
	unsigned mask_4			: 8;
	unsigned param_4		: 8;
	unsigned mask_5			: 8;
	unsigned param_5		: 8;
} sFilter_Parameter_Entry2;

typedef struct {
	unsigned mask_6			: 8;
	unsigned param_6		: 8;
	unsigned mask_7			: 8;
	unsigned param_7		: 8;
	unsigned Reserved1		: 3;
	unsigned not_flag		: 1;
	unsigned Reserved2		: 2;
	unsigned not_flag_ver_id_byte	: 1;
	unsigned Reserved3		: 1;
	unsigned Reserved4		: 8;
} sFilter_Parameter_Entry3;

typedef struct {
    u8 Microcode[1024];
    sFilter_Parameter_Entry1 Filter_Parameter_Table1[32];
    u8 Reserved1[16];
    u8 Reserved1c[48];
    sFilter_Parameter_Entry2 Filter_Parameter_Table2[32];
    u8 Reserved2[16];
    u8 Reserved2c[48];
    sFilter_Parameter_Entry3 Filter_Parameter_Table3[32];
    u8 Reserved3[16];
    u8 Reserved3c[48];
    sPID_Entry PID_Search_Table[32];
    sPID_Parsing_Control_Entry PID_Parsing_Control_Table[32];
    sFilter_Definition_Entry Filter_Definition_Table[32];
    u8 Reserved4[30];
    u16 Version_no;
} sRISC_MEM_MAP;

typedef struct {
	unsigned type			: 8;
	unsigned expected_cc		: 8;
	unsigned detected_cc		: 8;
	unsigned pid			: 16;
} sCC_ERROR_MESSAGE;

typedef struct {
	unsigned type			: 8;
	unsigned filter_index		: 8;
	unsigned pid			: 16;
} sSECTION_COMPLETED_MESSAGE;

typedef struct {
	unsigned type			: 8;
	unsigned pid			: 16;
	unsigned cc			: 8;
	unsigned length			: 8;
} sPRIVATE_ADAPTION_MESSAGE;

#pragma pack()

/* offsets and size in words */
#define DMX_MICROCODE				(0x000 >> 1)
#define DMX_FILTER_PARAMETER_TABLE_1		(0x400 >> 1)
#define DMX_CONTROL_WORDS_1			(0x4d0 >> 1)
#define DMX_FILTER_PARAMETER_TABLE_2		(0x500 >> 1)
#define DMX_CONTROL_WORDS_2			(0x5d0 >> 1)
#define DMX_FILTER_PARAMETER_TABLE_3		(0x600 >> 1)
#define DMX_CONTROL_WORDS_3			(0x6d0 >> 1)
#define DMX_PID_SEARCH_TABLE			(0x700 >> 1)
#define DMX_PID_PARSING_CONTROL_TABLE		(0x740 >> 1)
#define DMX_FILTER_DEFINITION_TABLE		(0x7c0 >> 1)
#define DMX_VERSION_NO				(0x7fe >> 1)
#define DMX_RISC_RAM_SIZE			(0x800 >> 1)

#define DMX_MESSAGE_CC_ERROR			0xFE
#define DMX_MESSAGE_SYNC_LOSS			0xFD
#define DMX_MESSAGE_ADAPTATION			0xFC
#define DMX_MESSAGE_SECTION_COMPLETED		0xCE

int avia_gt_dmx_set_pid_control_table(u8 queue_nr, u8 type, u8 fork, u8 cw_offset, u8 cc, u8 start_up, u8 pec, u8 filt_tab_idx, u8 _psh, u8 no_of_filter);
int avia_gt_dmx_set_pid_table(u8 entry, u8 wait_pusi, u8 valid, u16 pid);
void avia_gt_dmx_free_section_filter(u8 index);
int avia_gt_dmx_alloc_section_filter(void *f);
void avia_gt_dmx_ecd_reset(void);
int avia_gt_dmx_ecd_set_key(u8 index, u8 parity, const u8 *key);
int avia_gt_dmx_ecd_set_pid(u8 index, u16 pid);
struct avia_gt_ucode_info *avia_gt_dmx_get_ucode_info(void);
void avia_gt_dmx_risc_reset(int reenable);
int avia_gt_dmx_risc_init(sAviaGtDmxRiscInit *risc_info);

#endif
