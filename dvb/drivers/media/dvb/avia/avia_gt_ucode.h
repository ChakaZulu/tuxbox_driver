/*
 * $Id: avia_gt_ucode.h,v 1.3 2004/05/19 20:15:00 derget Exp $
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

#ifndef _AVIA_GT_UCODE
#define _AVIA_GT_UCODE

/* filter modes */
/* same as type except for PES */
enum {
		TS=0,
		PES,	/* depends on ucode */
		SECTION,
};
#define AVIA_GT_UCODE_CAP_ECD				0x0001
#define AVIA_GT_UCODE_CAP_PES				0x0002
#define AVIA_GT_UCODE_CAP_SEC				0x0004
#define AVIA_GT_UCODE_CAP_TS					0x0008
#define AVIA_GT_UCODE_CAP_MSGQ				0x0010
#define AVIA_GT_UCODE_CAP_C_INTERFACE 		0x0020

/* flags for ucode */
enum {
	DISABLE_UCODE_SECTION_FILTERING=1	/* 0x01 */
};

typedef struct {
	u8 ucode_flags;
} sAviaGtDmxRiscInit;

/* 
	The proprietary Ucodes offer 32 8-byte section 
	filters for up to 32 feeds. They also allow to 
	chain together filters, so you can use different
	filters on the same feed. There is a 16-Byte section
	filter mode but it is currently unsupported.

	Each filter can be set for one queue only. 

	For SPTS (or multi-PID streaming) the same queue
	could be used in several feeds. The DVB-API doesn't
	currently support multi-PID streaming in this way.
*/

/* proprietary interface flags */
enum {
	CAN_WAITPUSI=1,				/* valid bit in PID_Search_Table */
	CAN_FORKQUEUE=2 	/* queue 0 is high speed queue (unused on dbox2 hardware) */
};

typedef struct {
	u8 idx;
	u8 filtercount;
} sAviaGtSection;

typedef struct {
	s8 dest_queue;
	u8 type;	/* TS, PES or Section */
	u8 section_idx;	/* index into section filters */
	u8 filtercount;
	u16 pid;
} sAviaGtFeed;

/*
	Kernel Ucode-API
	All functions must be implemented!
	2 possible views: queue or feed
	first alloc_feed once or several times
	then either start_feed:
		starts only one feed
	OR start_queue_feeds:
		starts all feeds related to a queue
	this could be used to enable multi-pid streaming
	same for stop_feed and stop_queue_feeds

	A section filter can be allocated once and 
	used mutiple times but a feed can only be set up
	once. The only way to free a section filter is to
	remove all queues that use it (automatic 
	destruction). Since this leads to race conditions
	it's recommended to atomically set up multiple
	filters.
	
	Note that if a section filter cannot be connected
	to a feed for some reason it needs to be removed
	by directly calling "free_section_filter". This 
	function will fail if any filters are already set up.
	
	Trying to allocate a PID twice will fail (must be 
	handled by higher layer) because the ucode stops
	when it finds a valid entry in the PID table.
	
*/

struct avia_gt_ucode_info {
	u32 caps;
	u8 prop_interface_flags;
	u8 qid_offset;
	u8 queue_mode[3];
	void (*init) (void);
	/* return feed_idx */
	u8 (*alloc_feed)(u8 queue_nr, u8 type, u16 pid);
	u8 (*alloc_section_feed)(u8 queue_nr, sAviaGtSection *section, u16 pid);
	
	void (*start_feed)(u8 feed_idx);
	void (*start_queue_feeds)(u8 queue_nr);

	void (*stop_feed)(u8 feed_idx,u8 remove);
	void (*stop_queue_feeds)(u8 queue_nr,u8 remove);
	
	/* returns 0 if error */
	u8 (*alloc_section_filter)(void *filter, sAviaGtSection *);
	void (*free_section_filter)(sAviaGtSection *);

	void (*ecd_reset)(void);
	int (*ecd_set_key)(u8 index, u8 parity, const u8 *cw);
	int (*ecd_set_pid)(u8 index, u16 pid);
	
	void (*handle_msgq)(struct avia_gt_dmx_queue*,void *);
};

#pragma pack(1)

/* Proprietary ucodes */
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
} sPRIVATE_ADAPTATION_MESSAGE;

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

struct avia_gt_ucode_info *avia_gt_dmx_get_ucode_info(void);
void avia_gt_dmx_risc_reset(int reenable);
int avia_gt_dmx_risc_init(sAviaGtDmxRiscInit *risc_info);
void avia_gt_dmx_set_ucode_info(u8 ucode_flags);
#endif
