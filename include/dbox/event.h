/*
 *	event.h - global event driver (dbox-II-project)
 *
 *	Homepage: http://dbox2.elxsi.de
 *
 *	Copyright (C) 2001 Gillem (gillem@berlios.de)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *	$Log: event.h,v $
 *	Revision 1.2  2001/12/18 17:58:52  gillem
 *	- add events
 *	- TODO: filter events
 *	
 *	Revision 1.1  2001/12/08 15:12:33  gillem
 *	- initial release
 *	
 *
 *	$Revision: 1.2 $
 *
 */

#ifndef EVENT_H
#define EVENT_H

/* global event defines
 */
#define EVENT_NOP		0
#define EVENT_VCR_ON		1
#define EVENT_VCR_OFF		2
#define EVENT_VHSIZE_CHANGE	3
#define EVENT_ARATIO_CHANGE	4

/* other data
 */

struct event_t {
	unsigned int event;
} event_t;

#ifdef __KERNEL__
extern int event_write_message( struct event_t * buf, size_t count );
#endif

#endif
