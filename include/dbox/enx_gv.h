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
 *   $Log: enx_gv.h,v $
 *   Revision 1.1  2001/11/01 18:19:09  Jolt
 *   graphic viewport driver added
 *
 *
 *   $Revision: 1.1 $
 *
 */

#ifndef ENX_GV_H
#define ENX_GV_H

extern void enx_gv_cursor_hide(void);
extern void enx_gv_cursor_show(void);
extern unsigned short enx_gv_get_stride(void);
extern void enx_gv_get_info(unsigned char **gv_mem_phys, unsigned char **gv_mem_lin, unsigned int *gv_mem_size);
extern void enx_gv_hide(void);
extern void enx_gv_set_input_mode(unsigned char mode);
extern int enx_gv_set_input_size(unsigned short width, unsigned short height);
extern int enx_gv_set_pos(unsigned short x, unsigned short y);
extern void enx_gv_set_size(unsigned short width, unsigned short height);
extern void enx_gv_show(void);

#endif