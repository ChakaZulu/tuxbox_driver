/*
 * tuxbox_hardware_dbox2.h - TuxBox hardware info - dbox2
 *
 * Copyright (C) 2003 Florian Schirmer <jolt@tuxbox.org>
 *                    Bastian Blank <waldi@tuxbox.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Id: tuxbox_hardware_dbox2.h,v 1.3 2003/03/04 23:06:28 waldi Exp $
 */

#ifndef TUXBOX_HARDWARE_DBOX2_H
#define TUXBOX_HARDWARE_DBOX2_H

#include <tuxbox/tuxbox_hardware.h>
#include <tuxbox/tuxbox_info_dbox2.h>

#define TUXBOX_HARDWARE_DBOX2_CAPABILITIES		( TUXBOX_CAPABILITIES_IR_RC | \
							  TUXBOX_CAPABILITIES_IR_KEYBOARD | \
		                                          TUXBOX_CAPABILITIES_LCD | \
							  TUXBOX_CAPABILITIES_NETWORK | \
							  TUXBOX_CAPABILITIES_CAM_EMBEDDED )

extern tuxbox_dbox2_av_t tuxbox_dbox2_av;
extern u8 tuxbox_dbox2_fp_revision;
extern tuxbox_dbox2_mid_t tuxbox_dbox2_mid;

#endif
