/* 
 * audio.h
 *
 * Copyright (C) 2000 Ralph  Metzler <ralph@convergence.de>
 *                  & Marcus Metzler <marcus@convergence.de>
                      for convergence integrated media GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Lesser Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef _OST_AUDIO_H_
#define _OST_AUDIO_H_

#define boolean int
#define true 1
#define false 0

typedef enum {
        AUDIO_SOURCE_DEMUX, /* Select the demux as the main source */ 
	AUDIO_SOURCE_MEMORY /* Select internal memory as the main source */ 
} audioStreamSource_t;

typedef enum { 
	AUDIO_STOPPED, /* Device is stopped */ 
        AUDIO_PLAYING, /* Device is currently playing */ 
	AUDIO_PAUSED   /* Device is paused */ 
} audioPlayState_t;

typedef enum {
        AUDIO_STEREO, 
        AUDIO_MONO_LEFT, 
	AUDIO_MONO_RIGHT, 
} audioChannelSelect_t;

struct audioStatus { 
        boolean AVSyncState; 
        boolean muteState; 
        audioPlayState_t playState; 
        audioStreamSource_t streamSource;
        audioChannelSelect_t channelSelect; 
        boolean bypassMode;
};


#define AUDIO_STOP                 _IOW('o',1,void) 
#define AUDIO_PLAY                 _IOW('o',2,void)
#define AUDIO_PAUSE                _IOW('o',3,void)
#define AUDIO_CONTINUE             _IOW('o',4,void)
#define AUDIO_SELECT_SOURCE        _IOW('o',5,audioStreamSource_t)
#define AUDIO_SET_MUTE             _IOW('o',6,boolean)
#define AUDIO_SET_AV_SYNC          _IOW('o',7,boolean)
#define AUDIO_SET_BYPASS_MODE      _IOW('o',8,boolean)
#define AUDIO_CHANNEL_SELECT       _IOW('o',9,audioChannelSelect_t)
#define AUDIO_GET_STATUS           _IOR('o',10,struct audioStatus *)

#endif /* _OST_AUDIO_H_ */
