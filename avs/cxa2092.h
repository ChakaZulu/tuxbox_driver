/*
    cxa20920.c - dbox-II-project

    Copyright (C) 2000 Gillem htoa@gmx.net

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#define SCARTSWITCHES		1

#define SCARTSWITCH_SONY     	0

/* outputs */
#define SCSW_OUT1		0
#define SCSW_OUT2		1

/* inputs */
#define SCSW_INP1		1
#define SCSW_INP2		2
#define SCSW_INP3		4
#define SCSW_INP4		8
#define SCSW_INP5		16
#define SCSW_INP6		32
#define SCSW_INP7		64
#define SCSW_INP8		128

typedef struct scsw_switch {
	int out;
	int inp;
} scsw_switch;

/* ioctl's */
#define IOCTL_WRITE_CONTROL_REG 1
#define IOCTL_READ_STATUS 			2
#define IOCTL_SWITCH			      3
