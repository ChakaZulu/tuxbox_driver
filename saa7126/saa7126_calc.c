/*
 *   saa7126_calc.c - pal driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2001 Gillem (htoa@gmx.net)
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
 *
 *   $Log: saa7126_calc.c,v $
 *   Revision 1.3  2001/01/06 10:06:55  gillem
 *   cvs check
 *
 *   $Revision: 1.3 $
 *
 */

#include "saa7126_calc.h"

void cyc2rgb( int cr, int y, int cb, int *r, int *g, int *b )
{
/*	*r = y + 1.3707*(cr-128);
	*g = y - 0.3365*(cb-128) - 0.6982*(cr-128);
	*b = y + 1.7324*(cb-128);
*/
}

unsigned int fsc( int ffsc, int fiic )
{
	return((ffsc/fiic)*(0x100000000));
}
