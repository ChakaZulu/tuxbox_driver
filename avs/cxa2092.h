/*
 *   cxa2092.h - audio/video switch driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2001 Gillem htoa@gmx.net
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
 *   $Log: cxa2092.h,v $
 *   Revision 1.3  2001/01/15 17:02:32  gillem
 *   rewriten
 *
 *   Revision 1.2  2001/01/06 10:05:43  gillem
 *   cvs check
 *
 *   $Revision: 1.3 $
 *
 */

/* WRITE MODE */

/* #### DATA 1 #### */

/* Elektronik Volume Curse (8dB steps) */
#define AVS_EVC (7<<5)
/* Electronik Volume Fine (1dB steps) */
#define AVS_EVF (7<<2)
/* TV Audio mute (same Data1&5) */
#define AVS_MUTE (1<<1)
/* Zero Cross detector active */
#define AVS_ZCD 1

/* #### DATA 2 #### */

/* Video Fast Blanking control */
#define AVS_FBLK (3<<6)
/* Select the input video sources for Vout1,Vout2,Vout3,Vout4 */
#define AVS_VSW1 (7<<3)
/* Select one of 5 stereo inputs for RTV,LTV */
#define AVS_ASW1 (7)

/* #### DATA 3 #### */

/* Video function switch control */
#define AVS_FNC (3<<6)
/* Select the input video sources for Vout5,Vout6 */
#define AVS_VSW2 (7<<3)
/* Select one of 5 stereo inputs for Rout1,Lout1 */
#define AVS_ASW2 (7)

/* #### DATA 4 #### */

/* When Y/C Mix=1 Converts Y/C input to CVBS for output through Vout8 */

#define AVS_YCM (1<<7)
/* Select the input video sources for Vout7 */
#define AVS_VSW3 (7<<3)
/* Select one of 5 stereo inputs for Rout2,Lout2 */
#define AVS_ASW3 (7)

/* #### DATA 5 #### */

/* same as MUTE ! */
#define AVS_MUTE2 (1<<7)
/* Logic outputs (open collector) 1=himp. 0=current sink mode */
#define AVS_LOG (0x0F)


/* READ MODE */

/* monitors voltage of pin 8 VCR */
#define AVS_FVCR (3)
/* monitors voltage of pin 8 AUX */
#define AVS_FAUX (3<<2)
/* POR=1 when DIG_VCC volt. threshold higher than reset level (approx
5V) */
#define AVS_POR (1<<4)
/* ZC Status=1 indicate that zero cross condition has been achieved
after vol or mute */
#define AVS_ZCS (1<<5)

/* ### data defines ### */

/* VS1 - TV Output */
#define AVS_TVOUT_DE1 0
#define AVS_TVOUT_DE2 1
#define AVS_TVOUT_VCR 2
#define AVS_TVOUT_AUX 3
#define AVS_TVOUT_DE3 4
#define AVS_TVOUT_DE4 5
#define AVS_TVOUT_DE5 6
#define AVS_TVOUT_VM1 7

/* VS2 - VCR Output */
#define AVS_VCROUT_DE1 0
#define AVS_VCROUT_DE2 1
#define AVS_VCROUT_VCR 2
#define AVS_VCROUT_AUX 3
#define AVS_VCROUT_DE3 4
#define AVS_VCROUT_VM1 5
#define AVS_VCROUT_VM2 6
#define AVS_VCROUT_VM3 7

/* VS3 - AUX Output */
#define AVS_AUXOUT_DE1 0
#define AVS_AUXOUT_VM1 1
#define AVS_AUXOUT_VCR 2
#define AVS_AUXOUT_AUX 3
#define AVS_AUXOUT_DE2 4
#define AVS_AUXOUT_VM2 5
#define AVS_AUXOUT_VM3 6
#define AVS_AUXOUT_VM4 7

/* FBLK */
#define AVS_FBLKOUT_NULL 0
#define AVS_FBLKOUT_5V	 1
#define AVS_FBLKOUT_IN1  2
#define AVS_FBLKOUT_IN2  3

/* FNC */
#define AVS_FNCOUT_INTTV		0
#define AVS_FNCOUT_EXT169		1
#define AVS_FNCOUT_EXT43		2
#define AVS_FNCOUT_EXT43_1	3

/* AUDIO Output */
#define AVS_AUDOUT_V1 0
#define AVS_AUDOUT_V2 1
#define AVS_AUDOUT_V3 2
#define AVS_AUDOUT_V4 3
#define AVS_AUDOUT_V5 4
#define AVS_AUDOUT_M1 5
#define AVS_AUDOUT_M2 6
#define AVS_AUDOUT_M3 7

/* Volume (course) */
#define AVS_VOLOUT_C00 0
#define AVS_VOLOUT_C08 1
#define AVS_VOLOUT_C16 2
#define AVS_VOLOUT_C24 3
#define AVS_VOLOUT_C32 4
#define AVS_VOLOUT_C40 5
#define AVS_VOLOUT_C48 6
#define AVS_VOLOUT_C56 7

/* Volume (fine) */
#define AVS_VOLOUT_F0 0
#define AVS_VOLOUT_F1 1
#define AVS_VOLOUT_F2 2
#define AVS_VOLOUT_F3 3
#define AVS_VOLOUT_F4 4
#define AVS_VOLOUT_F5 5
#define AVS_VOLOUT_F6 6
#define AVS_VOLOUT_F7 7

/* Mute */
#define AVS_MUTE_IM		0
#define AVS_MUTE_ZC		1
#define AVS_UNMUTE_IM	2
#define AVS_UNMUTE_ZC	3

#define SET_EVC(arr,val)	arr[0] = (arr[0]&(~AVS_EVC)) | (val<<5);
#define SET_EVF(arr,val)	arr[0] = (arr[0]&(~AVS_EVF)) | (val<<2);

/* IOCTL */
#define AVSIOSVSW1 0
#define AVSIOSVSW2 1
#define AVSIOSVSW3 2
#define AVSIOSASW1 3
#define AVSIOSASW2 4
#define AVSIOSASW3 5
#define AVSIOSVOL  6

#define AVSIOGVSW1 7
#define AVSIOGVSW2 8
#define AVSIOGVSW3 9
#define AVSIOGASW1 10
#define AVSIOGASW2 11
#define AVSIOGASW3 12
#define AVSIOGVOL  13

#define AVSIOSMUTE 14
#define AVSIOGMUTE 15
