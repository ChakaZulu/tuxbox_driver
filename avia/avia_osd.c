/*
 *   avia_osd.c - AViA OSD driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2001 Gillem (htoa@gmx.net)
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
 *   $Log: avia_osd.c,v $
 *   Revision 1.3  2001/03/03 00:24:44  gillem
 *   - not ready ...
 *
 *   Revision 1.2  2001/02/22 22:49:08  gillem
 *   - add functions
 *
 *   Revision 1.1  2001/02/22 15:30:59  gillem
 *   - initial release
 *
 *
 *   $Revision: 1.3 $
 *
 */

/* ---------------------------------------------------------------------- */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/malloc.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>

#include "dbox/avia.h"
//#include "fontfix.h"

/* ---------------------------------------------------------------------- */

typedef struct osd_header {

	u32 res01 	: 8;
	u32 next  	: 22;	// next frame pointer
	u32 res02 	: 2;

	u32 res03 	: 8;
	u32 bmsize1 : 18;
	u32 n1 		: 1; 	// set to 1
	u32 res05 	: 5;

	u32 res06 	: 14;
	u32 n2 		: 1; 	// set to 1
	u32 res08 	: 3;
	u32 n3 		: 1; 	// set to 1
	u32 res10 	: 2;
	u32 bmsize2 : 9; 	// 8:0 bms
	u32 res11 	: 2;

	u32 res12 	: 8;
	u32 bmp 	: 22;
	u32 res13 	: 2;

	u32 res14 	: 23;
	u32 peltyp	: 2;
	u32 n4 		: 1; 	// set to 1
	u32 gbf 	: 5;
	u32 n5 		: 1; 	// set to 1

	u32 res17 	: 12;
	u32 colstart: 10;
	u32 colstop : 10;

	u32 res18 	: 12;
	u32 rowstart: 10;
	u32 rowstop : 10;

	u32 res19 	: 8;
	u32 palette : 22;
	u32 res20 	: 2;
} osd_header;

typedef struct osd_frame {
	struct osd_header even;
	struct osd_header odd;
	struct osd_header header[16*2]; // max 16 * (odd&even)
} osd_frame;

/* ---------------------------------------------------------------------- */

static void rgb2crycb( int r, int g, int b, int blend, u32 * pal )
{
	int cr,y,cb;

	if(!pal)
		return;

	y  = ((257*r  + 504*g + 98*b)/1000 + 16)&0x7f;
	cr = ((439*r  - 368*g - 71*b)/1000 + 128)&0x7f;
	cb = ((-148*r - 291*g + 439*b)/1000 + 128)&0x7f;

	*pal = (y<<16)|(cr<<9)|(cb<<2)|(blend&3);

	printk("DATA: %d %d %d\n",cr,y,cb);

/*
Y   =   0.29900 * R + 0.58700 * G + 0.11400 * B
Cb = -0.16874 * R - 0.33126 * G + 0.50000 * B  + CENTER
Cr  =   0.50000 * R - 0.41869 * G - 0.08131 * B  + CENTER

Y = 0.257R + 0.504G + 0.098B + 16
Cr = 0.439R - 0.368G - 0.071B + 128
Cb = -0.148R - 0.291G + 0.439B + 128
*/
}

static int osd_create_frame( struct osd_frame * frame, int x, int y, \
						int w, int h, int gbf, int pel )
{
	memset(frame,0,sizeof(osd_frame));

	frame->even.n1 = 1;
	frame->even.n2 = 1;
	frame->even.n3 = 1;
	frame->even.n4 = 1;
	frame->even.n5 = 1;

	frame->odd.n1 = 1;
	frame->odd.n2 = 1;
	frame->odd.n3 = 1;
	frame->odd.n4 = 1;
	frame->odd.n5 = 1;

	frame->odd.gbf = frame->even.gbf = gbf;

	frame->odd.peltyp = frame->even.peltyp = pel;

	frame->odd.colstart = frame->even.colstart = x;
	frame->odd.colstop  = frame->even.colstop  = w;

	/* pal rulez */
	frame->even.rowstart = 22+(y/2);
	frame->even.rowstop  = 22+(h/2);

	frame->odd.rowstart = 335+(y-1)/2;
	frame->odd.rowstop  = 335+(h-1)/2;

	frame->odd.next = frame->even.next = 0;

	return 0;
}

/* ---------------------------------------------------------------------- */

static int osd_show_frame( struct osd_frame * frame, u32 * palette, \
						u32* bitmap, int bmsize )
{
	u32 osds,osde;
	u32 *odd,*even;
    int i;

	osds = rDR(OSD_BUFFER_START);
	osde = rDR(OSD_BUFFER_END);

	frame->odd.bmsize1 = frame->even.bmsize1 = (bmsize>>9);
	frame->odd.bmsize2 = frame->even.bmsize2 = (bmsize&0x1ff);

	frame->odd.palette = frame->even.palette = (osds+0x1000)>>2;
	frame->odd.bmp = frame->even.bmp = (osds+0x1100)>>2;

	/* copy header */
	even = (u32*)&frame->even;
	odd  = (u32*)&frame->odd;

	for(i=0;i<sizeof(osd_header)/4;i++)
	{
		wDR(osds+(i*4),*(even+i));
		wDR(osds+(i*4)+sizeof(osd_header),*(odd+i));
	}

	/* copy palette */
	for(i=0;i<16;i++)
	{
		wDR( osds+0x1000+(i*4), palette[i] );
	}

	/* copy bitmap */
	for(i=0;i<bmsize;i++)
	{
		wDR( ((osds+0x1100)&(~3))+(i*4), bitmap[i] );
	}

	/* enable osd */
	wDR(OSD_ODD_FIELD, (osds+sizeof(osd_header)) );
	wDR(OSD_EVEN_FIELD, osds);

	return 0;
}

/* ---------------------------------------------------------------------- */

static int init_avia_osd()
{
	u32 i;
	struct osd_frame frame;
	u32 palette[16];
	static u32 bitmap[0x1000];
	u32 pale;

	printk("OSD STATUS: %08X\n", rDR(OSD_VALID));

	wDR(OSD_BUFFER_IDLE_START,rDR(OSD_BUFFER_START));

	osd_create_frame( &frame, 150, 150, 400, 400, 0x1f, 1 );

	rgb2crycb( 100, 0, 0, 3, &pale );
	palette[0] = pale;

	rgb2crycb( 0, 100, 0, 3, &pale );

	/* set palette */
	for(i=1;i<16;i++)
	{
		palette[i] = pale;
	}

	/* set bitmap */
	for(i=0;i<0x1000;i++)
	{
		bitmap[i] = 0x1111;
	}

	osd_show_frame( &frame, palette, bitmap, 0x1000 );

	printk("OSD: %08X\n",rDR(OSD_BUFFER_IDLE_START));
	printk("OSD: %08X\n",rDR(OSD_BUFFER_START));
	printk("OSD: %08X\n",rDR(OSD_BUFFER_END));
	printk("OSD: %08X\n",rDR(OSD_ODD_FIELD));
	printk("OSD: %08X\n",rDR(OSD_EVEN_FIELD));

	udelay(1000*100);

	printk("OSD STATUS: %08X\n", rDR(OSD_VALID));

	return 0;
}

/* ---------------------------------------------------------------------- */

#ifdef MODULE
MODULE_AUTHOR("Gillem <htoa@gmx.net>");
MODULE_DESCRIPTION("AVIA OSD driver");
MODULE_PARM(debug,"i");

int init_module(void)
{
	return init_avia_osd();
}

void cleanup_module(void)
{
	wDR(OSD_EVEN_FIELD, 0 );
	wDR(OSD_ODD_FIELD, 0 );

	return;
}
#endif

/* ---------------------------------------------------------------------- */
