/*
 *   tuxbox.h - Tuxbox Hardware info
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

#ifndef TUXBOX_H
#define TUXBOX_H

#define TUXBOX_VERSION						KERNEL_VERSION(1,0,1)

#define TUXBOX_CAPABILITIES_IR_RC			0x00000001
#define TUXBOX_CAPABILITIES_IR_KEYBOARD		0x00000002
#define TUXBOX_CAPABILITIES_LCD				0x00000004
#define TUXBOX_CAPABILITIES_NETWORK			0x00000008
#define TUXBOX_CAPABILITIES_HDD				0x00000010

#define	TUXBOX_MANUFACTURER_UNKNOWN			0x0000
#define	TUXBOX_MANUFACTURER_NOKIA			0x0001
#define	TUXBOX_MANUFACTURER_SAGEM			0x0002
#define	TUXBOX_MANUFACTURER_PHILIPS			0x0003
#define	TUXBOX_MANUFACTURER_DREAM_MM		0x0004

#define TUXBOX_MODEL_UNKNOWN				0x0000
#define TUXBOX_MODEL_DBOX2					0x0001
#define TUXBOX_MODEL_DREAMBOX_DM7000		0x0002
#define TUXBOX_MODEL_DREAMBOX_DM5600		0x0003

#endif
