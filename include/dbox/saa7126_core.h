/*
 *   saa7126_core.h - pal driver (dbox-II-project)
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
 *   $Log: saa7126_core.h,v $
 *   Revision 1.3  2001/02/11 12:31:29  gillem
 *   - add ioctl SAAIOGREG,SAAIOSINP,SAAIOSENC
 *   - change i2c stuff
 *   - change device to pal
 *   - change major to 240
 *   - add load option board (0=nokia,1=philips,2=sagem,3=no config)
 *
 *   Revision 1.2  2001/01/06 10:06:55  gillem
 *   cvs check
 *
 *   $Revision: 1.3 $
 *
 */

#define SAAIOGREG		1
#define SAAIOSINP		2
#define SAAIOSENC		3

#ifdef __KERNEL__

typedef struct s_saa_data {
	unsigned char version   : 3;
	unsigned char ccrdo     : 1;
	unsigned char ccrde     : 1;
	unsigned char res01     : 1;
	unsigned char fseq      : 1;
	unsigned char o_2       : 1;

	unsigned char res02[0x25-0x01];	// NULL

	unsigned char wss_7_0   : 8;

	unsigned char wsson     : 1;
	unsigned char res03     : 1;
	unsigned char wss_13_8  : 6;

	unsigned char deccol    : 1;
	unsigned char decfis    : 1;
	unsigned char bs        : 6;

	unsigned char sres      : 1;
	unsigned char res04     : 1;
	unsigned char be        : 6;

	unsigned char cg_7_0    : 8;

	unsigned char cg_15_8   : 8;

	unsigned char cgen      : 1;
	unsigned char res05     : 3;
	unsigned char cg_19_16  : 4;

	unsigned char vbsen     : 2;
	unsigned char cvbsen    : 1;
	unsigned char cen       : 1;
	unsigned char cvbstri   : 1;
	unsigned char rtri      : 1;
	unsigned char gtri      : 1;
	unsigned char btri      : 1;

	unsigned char res06[0x37-0x2e];	// NULL

	unsigned char res07     : 3;
	unsigned char gy        : 5;

	unsigned char res08     : 3;
	unsigned char gcd       : 5;

	unsigned char vbenb     : 1;
	unsigned char res09     : 2;
	unsigned char symp      : 1;
	unsigned char demoff    : 1;
	unsigned char csync     : 1;
	unsigned char mp2c      : 2;

	unsigned char vpsen     : 1;
	unsigned char ccirs     : 1;
	unsigned char res10     : 4;
	unsigned char edge      : 2;

	unsigned char vps5      : 8;
	unsigned char vps11     : 8;
	unsigned char vps12     : 8;
	unsigned char vps13     : 8;
	unsigned char vps14     : 8;

	unsigned char chps      : 8;
	unsigned char gainu_7_0 : 8;
	unsigned char gainv_7_0 : 8;

	unsigned char gainu_8   : 1;
	unsigned char decoe     : 1;
	unsigned char blckl     : 6;

	unsigned char gainv_8   : 1;
	unsigned char decph     : 1;
	unsigned char blnnl     : 6;

	unsigned char ccrs      : 2;
	unsigned char blnvb     : 6;

	unsigned char res11     : 8; // NULL

	unsigned char downb     : 1;
	unsigned char downa     : 1;
	unsigned char inpi      : 1;
	unsigned char ygs       : 1;
	unsigned char res12     : 1;
	unsigned char scbw      : 1;
	unsigned char apl       : 1;
	unsigned char fise      : 1;

	// 62h
	unsigned char rtce      : 1;
	unsigned char bsta      : 7;

	unsigned char fsc0      : 8;
	unsigned char fsc1      : 8;
	unsigned char fsc2      : 8;
	unsigned char fsc3      : 8;

	unsigned char l21o0     : 8;
	unsigned char l21o1     : 8;
	unsigned char l21e0     : 8;
	unsigned char l21e1     : 8;

	unsigned char srcv0     : 1;
	unsigned char srcv1     : 1;
	unsigned char trcv2     : 1;
	unsigned char orcv1     : 1;
	unsigned char prcv1     : 1;
	unsigned char cblf      : 1;
	unsigned char orcv2     : 1;
	unsigned char prcv2     : 1;

	// 6ch
	unsigned char htrig0    : 8;
	unsigned char htrig1    : 8;

	unsigned char sblbn     : 1;
	unsigned char blckon    : 1;
	unsigned char phres     : 2;
	unsigned char ldel      : 2;
	unsigned char flc       : 2;

	unsigned char ccen      : 2;
	unsigned char ttxen     : 1;
	unsigned char sccln     : 5;

	unsigned char rcv2s_lsb : 8;
	unsigned char rcv2e_lsb : 8;

	unsigned char res13     : 1;
	unsigned char rvce_mbs  : 3;
	unsigned char res14     : 1;
	unsigned char rvcs_mbs  : 3;

	unsigned char ttxhs     : 8;
	unsigned char ttxhl     : 4;
	unsigned char ttxhd     : 4;

	unsigned char csynca    : 5;
	unsigned char vss       : 3;

	unsigned char ttxovs    : 8;
	unsigned char ttxove    : 8;
	unsigned char ttxevs    : 8;
	unsigned char ttxeve    : 8;

	// 7ah
	unsigned char fal       : 8;
	unsigned char lal       : 8;

	unsigned char ttx60     : 1;
	unsigned char lal8      : 1;
	unsigned char ttx0      : 1;
	unsigned char fal8      : 1;
	unsigned char ttxeve8   : 1;
	unsigned char ttxove8   : 1;
	unsigned char ttxevs8   : 1;
	unsigned char ttxovs8   : 1;

	unsigned char res15     : 8;

	unsigned char ttxl12    : 1;
	unsigned char ttxl11    : 1;
	unsigned char ttxl10    : 1;
	unsigned char ttxl9     : 1;
	unsigned char ttxl8     : 1;
	unsigned char ttxl7     : 1;
	unsigned char ttxl6     : 1;
	unsigned char ttxl5     : 1;

	unsigned char ttxl20    : 1;
	unsigned char ttxl19    : 1;
	unsigned char ttxl18    : 1;
	unsigned char ttxl17    : 1;
	unsigned char ttxl16    : 1;
	unsigned char ttxl15    : 1;
	unsigned char ttxl14    : 1;
	unsigned char ttxl13    : 1;
} s_saa_data;

#define SAA_DATA_SIZE		sizeof(s_saa_data)

#define BOARD_NOKIA		0
#define BOARD_PHILIPS	1
#define BOARD_SAGEM		2
#define BOARD_NONE		3

#endif
