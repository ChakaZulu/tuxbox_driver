/*
 *   avia_gt_dmx.h - dmx driver for AViA (dbox-II-project)
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
 */

#ifndef AVIA_GT_DMX_H
#define AVIA_GT_DMX_H

#pragma pack(1)

typedef struct {

    u32 wait_pusi: 1;
    u32 VALID: 1;
    u32 Reserved1: 1;
    u32 PID: 13;

} sPID_Entry;

typedef struct {

	u32 type: 3;
	u32 QID: 5;
	u32 fork: 1;
	u32 CW_offset: 3;
	u32 CC: 4;
	u32 _PSH: 1;
	u32 start_up: 1;
	u32 PEC: 1;
	u32 filt_tab_idx: 5;
	u32 State: 3;
	u32 Reserved1: 1;
	u32 no_of_filter: 4;
	
} sPID_Parsing_Control_Entry;

typedef struct {

	u32 and_or_flag: 1;
	u32 filter_param_id: 6;
	u32 Reserved: 1;
	
} sFilter_Definition_Entry;

typedef struct {

	u8 mask_0;
	u8 param_0;
	u8 mask_1;
	u8 param_1;
	u8 mask_2;
	u8 param_2;

} sFilter_Parameter_Entry1;

typedef struct {

	u8 mask_3;
	u8 param_3;
	u8 mask_4;
	u8 param_4;
	u8 mask_5;
	u8 param_5;

} sFilter_Parameter_Entry2;

typedef struct {

	u8 mask_6;
	u8 param_6;
	u8 mask_7;
	u8 param_7;
	u32 Reserved1: 3;
	u32 not_flag: 1;
	u32 Reserved2: 2;
	u32 not_flag_ver_id_byte: 1;
	u32 Reserved3: 1;
	u8 Reserved4;

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
    u8 Version_no[2];
    
} sRISC_MEM_MAP;

#pragma pack()

void avia_gt_dmx_force_discontinuity(void);
int avia_gt_dmx_set_filter_definition_table(u8 entry, u8 and_or_flag, u8 filter_param_id);
int avia_gt_dmx_set_filter_parameter_table(u8 entry, u8 mask[8], u8 param[8], u8 not_flag, u8 not_flag_ver_id_byte);
void avia_gt_dmx_set_pcr_pid(u16 pid);
int avia_gt_dmx_set_pid_control_table(u8 entry, u8 type, u8 queue, u8 fork, u8 cw_offset, u8 cc, u8 start_up, u8 pec, u8 filt_tab_idx, u8 no_of_filter);
int avia_gt_dmx_set_pid_table(u8 entry, u8 wait_pusi, u8 valid, u16 pid);
unsigned int avia_gt_dmx_get_queue_write_pointer(unsigned char queue_nr);
void avia_gt_dmx_set_queue_write_pointer(unsigned char queue_nr, unsigned int write_pointer);
void avia_gt_dmx_set_queue_irq(unsigned char queue_nr, unsigned char qim, unsigned int irq_addr);
void avia_gt_dmx_set_queue(unsigned char queue_nr, unsigned int write_pointer, unsigned char size);
void gtx_set_queue_pointer(int queue, u32 read, u32 write, int size, int halt);
int avia_gt_dmx_init(void);
void avia_gt_dmx_exit(void);
	
#endif
