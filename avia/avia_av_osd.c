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
 *   $Log: avia_av_osd.c,v $
 *   Revision 1.6  2001/03/04 22:17:46  gillem
 *   - add read function ... avia500 multiframe not work
 *
 *   Revision 1.5  2001/03/04 20:04:41  gillem
 *   - show 16 frames ... avia600 only ????
 *
 *   Revision 1.4  2001/03/04 17:57:13  gillem
 *   - add more frames, not work !
 *
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
 *   $Revision: 1.6 $
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

/* ---------------------------------------------------------------------- */

typedef struct osd_header {

	u32 res01 	: 8   __attribute__((packed));
	u32 next  	: 22  __attribute__((packed));	// next frame pointer
	u32 res02 	: 2   __attribute__((packed));

	u32 res03 	: 8   __attribute__((packed));
	u32 bmsize1 : 18  __attribute__((packed));
	u32 n1 		: 1   __attribute__((packed)); 	// set to 1
	u32 res05 	: 5   __attribute__((packed));

	u32 res06 	: 14  __attribute__((packed));
	u32 n2 		: 1   __attribute__((packed)); 	// set to 1
	u32 res08 	: 3   __attribute__((packed));
	u32 n3 		: 1   __attribute__((packed)); 	// set to 1
	u32 res10 	: 2   __attribute__((packed));
	u32 bmsize2 : 9   __attribute__((packed)); 	// 8:0 bms
	u32 res11 	: 2   __attribute__((packed));

	u32 res12 	: 8   __attribute__((packed));
	u32 bmp 	: 22  __attribute__((packed));
	u32 res13 	: 2   __attribute__((packed));

	u32 res14 	: 23  __attribute__((packed));
	u32 peltyp	: 2   __attribute__((packed));
	u32 n4 		: 1   __attribute__((packed)); 	// set to 1
	u32 gbf 	: 5   __attribute__((packed));
	u32 n5 		: 1   __attribute__((packed)); 	// set to 1

	u32 res17 	: 12  __attribute__((packed));
	u32 colstart: 10  __attribute__((packed));
	u32 colstop : 10  __attribute__((packed));

	u32 res18 	: 12  __attribute__((packed));
	u32 rowstart: 10  __attribute__((packed));
	u32 rowstop : 10  __attribute__((packed));

	u32 res19 	: 8   __attribute__((packed));
	u32 palette : 22  __attribute__((packed));
	u32 res20 	: 2   __attribute__((packed));
} osd_header;

#define OSDH_SIZE sizeof(osd_header)

typedef struct osd_font {
	u16 width;
	u16 height;
	u8 *font;
} osd_font;

typedef struct osd_palette {
	u16 size;
	u32 *palette;
} osd_palette;

typedef struct osd_bitmap {
	u16 size;
	u32 *bitmap;
} osd_bitmap;

typedef struct osd_frame {
	int framenr;
	struct osd_header even;
	struct osd_header odd;
	struct osd_font *font;
	struct osd_bitmap *bitmap;
	struct osd_palette * palette;
} osd_frame;

typedef struct osd_frames {
	struct osd_frame frame[16];
} osd_frames;

struct osd_frames frames;

u32 osds,osde;

/* ---------------------------------------------------------------------- */

static void rgb2crycb( int r, int g, int b, int blend, u32 * pale )
{
	int cr,y,cb;

	if(!pale)
		return;

	y  = ((257*r  + 504*g + 98*b)/1000 + 16)&0x7f;
	cr = ((439*r  - 368*g - 71*b)/1000 + 128)&0x7f;
	cb = ((-148*r - 291*g + 439*b)/1000 + 128)&0x7f;

	*pale = (y<<16)|(cr<<9)|(cb<<2)|(blend&3);

	printk("OSD DATA: %d %d %d\n",cr,y,cb);
}

/* ---------------------------------------------------------------------- */

static void osd_set_font( unsigned char * font, int width, int height )
{
}

/* ---------------------------------------------------------------------- */

static int osd_show_frame( struct osd_frame * frame, u32 * palette, \
						u32* bitmap, int bmsize )
{
    int i;

	frame->odd.bmsize1 = frame->even.bmsize1 = (bmsize>>9);
	frame->odd.bmsize2 = frame->even.bmsize2 = (bmsize&0x1ff);

	frame->odd.palette = frame->even.palette = (osds+0x5000)>>2;
	frame->odd.bmp = frame->even.bmp = (osds+0x5100)>>2;

	/* copy palette */
	for(i=0;i<16;i++)
	{
		wDR( osds+0x5000+(i*4), palette[i] );
	}

	/* copy bitmap */
	for(i=0;i<bmsize;i++)
	{
		wDR( osds+0x5100+(i*4), bitmap[i] );
	}

	return 0;
}

/* ---------------------------------------------------------------------- */

static void osd_create_frame( struct osd_frame * frame, int x, int y, \
						int w, int h, int gbf, int pel )
{
	frame->odd.gbf = frame->even.gbf = gbf;

	frame->odd.peltyp = frame->even.peltyp = pel;

	frame->odd.colstart = frame->even.colstart = x;
	frame->odd.colstop  = frame->even.colstop  = x+w;

	/* pal rulez */
	frame->even.rowstart = 22+(y/2);
	frame->even.rowstop  = 22+((y+h)/2);

	frame->odd.rowstart = 335+(y-1)/2;
	frame->odd.rowstop  = 335+((y+h)-1)/2;
}

/* ---------------------------------------------------------------------- */

static void osd_read_frame( struct osd_frame * frame )
{
	u32 *odd,*even;
	u32 osdsp;
    int i;

	osdsp = osds+(OSDH_SIZE*2*frame->framenr);

	printk("OSD FP: %08X\n",osdsp);

	/* copy header */
	even = (u32*)&frame->even;
	odd  = (u32*)&frame->odd;

	for(i=0;i<OSDH_SIZE;i+=4,osdsp+=4,even++,odd++)
	{
		*even = rDR(osdsp);
		*odd  = rDR(osdsp+OSDH_SIZE);
	}
}

/* ---------------------------------------------------------------------- */

static void osd_write_frame( struct osd_frame * frame )
{
	u32 *odd,*even;
	u32 osdsp;
    int i;

	osdsp = osds+(OSDH_SIZE*2*frame->framenr);

	printk("OSD FP: %08X\n",osdsp);

	/* copy header */
	even = (u32*)&frame->even;
	odd  = (u32*)&frame->odd;

	for(i=0;i<OSDH_SIZE;i+=4,osdsp+=4,even++,odd++)
	{
		wDR(osdsp,*even);
		wDR(osdsp+OSDH_SIZE,*odd);
	}

	printk("OSD FNR: %d E: %08X O: %08X\n",frame->framenr,frame->even.next<<2,frame->odd.next<<2);
}

/* ---------------------------------------------------------------------- */

static void osd_init_frame( struct osd_frame * frame, int framenr )
{
	memset(frame,1,sizeof(osd_frame));

	frame->framenr = framenr;

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

	if ( framenr < 15 )
	{
		frame->even.next = (osds+(OSDH_SIZE*2*(framenr+1)))>>2;
		frame->odd.next  = (osds+(OSDH_SIZE*2*(framenr+1))+OSDH_SIZE)>>2;
	}
}

/* ---------------------------------------------------------------------- */

static void osd_init_frames( struct osd_frames * frames )
{
	int i;

	for(i=0;i<16;i++)
	{
		osd_init_frame(&frames->frame[i],i);
		osd_write_frame(&frames->frame[i]);
	}
}

/* ---------------------------------------------------------------------- */

static int init_avia_osd(void)
{
	u32 i;
	u32 palette[16];
	static u32 bitmap[0x1000];
	u32 pale;

	printk("OSD STATUS: %08X\n", rDR(OSD_VALID));

	osds = (rDR(OSD_BUFFER_START)&(~3));
	osde = rDR(OSD_BUFFER_END);

	if (osds<rDR(OSD_BUFFER_START))
	{
		osds+=4;
	}

	for(i=osds;i<osde;i+=4)
		wDR(i,0);

    osd_init_frames(&frames);

	rgb2crycb( 100, 0, 100, 3, &pale );
	palette[0] = pale;
	rgb2crycb( 1000, 100, 100, 3, &pale );

	/* set palette */
	for(i=1;i<16;i++)
	{
		palette[i] = pale;
	}

	/* set bitmap */
	for(i=0;i<0x1000;i++)
	{
		bitmap[i] = 0x11111111;
	}

	for(i=0;i<16;i++)
	{
		osd_create_frame( &frames.frame[i], 60+(i*22), 60+(i*22), 20, 20, 0x1f, 1 );
		osd_show_frame( &frames.frame[i], palette, bitmap, 20*20*4/8 );
		osd_write_frame( &frames.frame[i] );
	}

	/* enable osd */
	wDR(OSD_ODD_FIELD, osds+OSDH_SIZE );
	wDR(OSD_EVEN_FIELD, osds);

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
