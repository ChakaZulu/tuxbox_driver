/*
 *   avia_pcm.h - pcm driver for AViA (dbox-II-project)
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

#ifndef AVIA_PCM_H
#define AVIA_PCM_H

#define IM_AVIA_PCM_OPS "im_avia_pcm_ops"

typedef struct {

    char *name;
    int (*play_buffer)(void *buffer, unsigned int buffer_size, unsigned char block);
    int (*set_rate)(unsigned short rate);
    int (*set_width)(unsigned char width);
    int (*set_channels)(unsigned char channels);
    int (*set_signed)(unsigned char signed_samples);
    int (*set_endian)(unsigned char be);
    void (*set_mpeg_attenuation)(unsigned char left, unsigned char right);
    void (*set_pcm_attenuation)(unsigned char left, unsigned char right);
    void (*stop)(void);
	    
} sAviaPcmOps;
	    
#endif
	    