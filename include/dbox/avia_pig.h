/*
 *   avia_pig.h - capture driver for AViA (dbox-II-project)
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

#define AVIA_PIG_HIDE	 	1
#define AVIA_PIG_SET_POS 	2
#define AVIA_PIG_SET_SOURCE 	3
#define AVIA_PIG_SET_SIZE 	4
#define AVIA_PIG_SET_STACK 	5
#define AVIA_PIG_SHOW	 	6

#define avia_pig_hide(fd) 		ioctl(fd, AVIA_PIG_HIDE, 0)
#define avia_pig_set_pos(fd, x, y) 	ioctl(fd, AVIA_PIG_SET_POS, (x | (y << 16)))
#define avia_pig_set_source(fd, x, y) 	ioctl(fd, AVIA_PIG_SET_SOURCE, (x | (y << 16)))
#define avia_pig_set_size(fd, x, y) 	ioctl(fd, AVIA_PIG_SET_SIZE, (x | (y << 16)))
#define avia_pig_set_stack(fd, order) 	ioctl(fd, AVIA_PIG_SET_SIZE, order)
#define avia_pig_show(fd) 		ioctl(fd, AVIA_PIG_SHOW, 0)
