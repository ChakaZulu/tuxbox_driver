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

void avia_gt_dmx_force_discontinuity(void);
void avia_gt_dmx_set_pcr_pid(u16 pid);
unsigned int avia_gt_dmx_get_queue_write_pointer(unsigned char queue_nr);
void avia_gt_dmx_set_queue_write_pointer(unsigned char queue_nr, unsigned int write_pointer);
void avia_gt_dmx_set_queue_irq(unsigned char queue_nr, unsigned char qim, unsigned int irq_addr);
void avia_gt_dmx_set_queue(unsigned char queue_nr, unsigned int write_pointer, unsigned char size);
void gtx_set_queue_pointer(int queue, u32 read, u32 write, int size, int halt);
int avia_gt_dmx_init(void);
void avia_gt_dmx_exit(void);
	
#endif
