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
 * $Id: tuxbox_hardware_dbox2.h,v 1.1 2003/02/19 16:39:00 waldi Exp $
 */

#ifndef TUXBOX_HARDWARE_DBOX2_H
#define TUXBOX_HARDWARE_DBOX2_H

#include <tuxbox/tuxbox_hardware.h>

#define TUXBOX_HARDWARE_DBOX2_MID_NOKIA			1
#define TUXBOX_HARDWARE_DBOX2_MID_PHILIPS		2
#define TUXBOX_HARDWARE_DBOX2_MID_SAGEM			3

#define TUXBOX_HARDWARE_DBOX2_CAPABILITIES		( TUXBOX_CAPABILITIES_IR_RC | \
							  TUXBOX_CAPABILITIES_IR_KEYBOARD | \
		                                          TUXBOX_CAPABILITIES_LCD | \
							  TUXBOX_CAPABILITIES_NETWORK | \
							  TUXBOX_CAPABILITIES_CAM_EMBEDDED )

#endif
