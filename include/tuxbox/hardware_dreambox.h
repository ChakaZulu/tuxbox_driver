/*
 * tuxbox_hardware_dreambox.h - TuxBox hardware info - dreambox
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
 * $Id: hardware_dreambox.h,v 1.1 2003/03/05 09:51:20 waldi Exp $
 */

#ifndef TUXBOX_HARDWARE_DREAMBOX_H
#define TUXBOX_HARDWARE_DREAMBOX_H

#define TUXBOX_HARDWARE_DREAMBOX_ID_DM5600		0x51510950
#define TUXBOX_HARDWARE_DREAMBOX_ID_DM7000		0x418108D1

#define TUXBOX_HARDWARE_DREAMBOX_DM5600_CAPABILITIES	\
	(TUXBOX_CAPABILITIES_IR_RC | \
	 TUXBOX_CAPABILITIES_IR_KEYBOARD | \
	 TUXBOX_CAPABILITIES_CAM_CI)

#define TUXBOX_HARDWARE_DREAMBOX_DM7000_CAPABILITIES	\
	(TUXBOX_CAPABILITIES_IR_RC | \
	 TUXBOX_CAPABILITIES_IR_KEYBOARD | \
	 TUXBOX_CAPABILITIES_LCD | \
	 TUXBOX_CAPABILITIES_NETWORK | \
	 TUXBOX_CAPABILITIES_HDD | \
	 TUXBOX_CAPABILITIES_CAM_CI)

#endif
