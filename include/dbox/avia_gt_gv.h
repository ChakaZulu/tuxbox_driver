/*
 *   enx_gv.h - AViA eNX graphic viewport driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2001 Florian Schirmer (jolt@tuxbox.org)
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
 *   $Log: avia_gt_gv.h,v $
 *   Revision 1.4  2002/04/17 21:50:57  Jolt
 *   Capture driver fixes
 *
 *   Revision 1.3  2002/04/15 10:40:50  Jolt
 *   eNX/GTX
 *
 *   Revision 1.2  2002/04/15 04:44:24  Jolt
 *   eNX/GTX merge
 *
 *   Revision 1.1  2001/11/01 18:19:09  Jolt
 *   graphic viewport driver added
 *
 *
 *   $Revision: 1.4 $
 *
 */

#ifndef AVIA_GT_GV_H
#define AVIA_GT_GV_H

#define AVIA_GT_GV_INPUT_MODE_OFF	0x00
#define AVIA_GT_GV_INPUT_MODE_RGB4	0x02
#define AVIA_GT_GV_INPUT_MODE_RGB8	0x06
#define AVIA_GT_GV_INPUT_MODE_RGB16	0x03
#define AVIA_GT_GV_INPUT_MODE_RGB32	0x07

extern void avia_gt_gv_cursor_hide(void);
extern void avia_gt_gv_cursor_show(void);
extern unsigned short avia_gt_gv_get_stride(void);
extern void avia_gt_gv_get_info(unsigned char **gv_mem_phys, unsigned char **gv_mem_lin, unsigned int *gv_mem_size);
extern void avia_gt_gv_hide(void);
extern void avia_gt_gv_set_input_mode(unsigned char mode);
extern int avia_gt_gv_set_input_size(unsigned short width, unsigned short height);
extern int avia_gt_gv_set_pos(unsigned short x, unsigned short y);
extern void avia_gt_gv_set_size(unsigned short width, unsigned short height);
extern void avia_gt_gv_show(void);
extern int avia_gt_gv_init(void);
extern void avia_gt_gv_exit(void);

#endif
