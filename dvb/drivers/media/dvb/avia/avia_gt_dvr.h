/*
 * $Id: avia_gt_dvr.h,v 1.2 2003/04/12 11:08:50 alexw Exp $
 *
 * (C) 2003 Andreas Oberritter <obi@tuxbox.org>
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

#ifndef AVIA_GT_DVR_H
#define AVIA_GT_DVR_H

void avia_gt_dvr_queue_irq(struct avia_gt_dmx_queue *queue, void *priv_data);
ssize_t avia_gt_dvr_write(const char *buf, size_t count);
void avia_gt_dvr_enable(void);
void avia_gt_dvr_disable(void);

extern int avia_gt_dvr_module_init(void);
extern void avia_gt_dvr_module_exit(void);
	    
#endif
