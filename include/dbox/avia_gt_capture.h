/*
 *   enx_capture.h - capture driver for enx (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2001 Florian Schirmer <jolt@tuxbox.org>
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

extern unsigned char *enx_capture_get_buf_addr(void);
extern int enx_capture_set_input(unsigned short x, unsigned short y, unsigned short width, unsigned short height);
extern int enx_capture_set_output(unsigned short width, unsigned short height);
extern int enx_capture_start(unsigned char **capture_buffer, unsigned short *stride);
extern void enx_capture_stop(void);
