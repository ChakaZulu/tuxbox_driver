#ifndef __PCM_H_
#define __PCM_H_

/*
 *   pcm.h - pcm driver for gtx (dbox-II-project)
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
 *   $Log: pcm.h,v $
 *   Revision 1.5  2001/11/26 23:07:30  Terminar
 *   implemented more ioctls - pcm Termina'ted' ;)
 *
 *   Revision 1.4  2001/02/03 14:13:40  tmbinc
 *   Moved PCMx- and DPCR-stuff into gtx.h. Was in pcm.h.
 *
 *   Revision 1.3  2001/01/06 10:07:10  gillem
 *   cvs check
 *
 *   $Revision: 1.5 $
 *
 */



void pcm_reset(void);
void pcm_dump_reg(void);

void avia_audio_init(void);


struct pcm_state {
  /* soundcore stuff */
  int dev_audio;
  int dev_mixer;
};



struct pcm_data {
    int fmt; /* oss format */
    int rate;   /* rate in mhz */
    int channels; /* number of channels */
    int bits;   /* 8 or 16 bits */
    int sign;   /* signed or unsigned */
};


#define PCM_REG_ENABLED (1<<9)
#define PCM_REG_SIGNED (1<<11)
#define PCM_REG_BITS   (1<<13)
#define PCM_REG_STEREO (1<<12)

#define PCM_REG_RATE_44100  (3<<14)
#define PCM_REG_RATE_22050  (2<<14)
#define PCM_REG_RATE_11025  (1<<14)



#endif /* pcm.h */
