/* 
 * video.h
 *
 * Copyright (C) 2000 Marcus Metzler <marcus@convergence.de>
 *                  & Ralph  Metzler <ralph@convergence.de>
                      for convergence integrated media GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
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

#ifndef _OST_VIDEO_H_
#define _OST_VIDEO_H_

#define boolean int
#define true 1
#define false 0

typedef enum {
	VIDEO_FORMAT_4_3, /* Select 4:3 format */ 
        VIDEO_FORMAT_16_9 /* Select 16:9 format. */ 
} videoFormat_t;


typedef enum {
        VIDEO_PAN_SCAN, 
	VIDEO_LETTER_BOX, 
	VIDEO_CENTER_CUT_OUT 
} videoDisplayFormat_t;


typedef enum {
        VIDEO_SOURCE_DEMUX, /* Select the demux as the main source */ 
	VIDEO_SOURCE_MEMORY /* If this source is selected, the stream 
			       comes from the user through the write 
			       system call */ 
} videoStreamSource_t;

typedef enum {
	VIDEO_STOPPED, /* Video is stopped */ 
        VIDEO_PLAYING, /* Video is currently playing */ 
	VIDEO_FREEZED  /* Video is freezed */ 
} videoPlayState_t; 


struct videoEvent { 
        int32_t type; 
        time_t timestamp;
 
        union { 
	        videoFormat_t videoFormat;
	} u; 
};


struct videoStatus { 
        boolean videoBlank; 
        videoPlayState_t playState; 
        videoStreamSource_t streamSource; 
        videoFormat_t videoFormat; 
        videoDisplayFormat_t displayFormat; 
};


struct videoDisplayStillPicture { 
        char *iFrame; 
        int32_t size; 
};

#define VIDEO_STOP                 _IOW('o', 21, boolean) 
#define VIDEO_PLAY                 _IOW('o', 22, void)
#define VIDEO_FREEZE               _IOW('o', 23, void)
#define VIDEO_CONTINUE             _IOW('o', 24, void)
#define VIDEO_SELECT_SOURCE        _IOW('o', 25, videoStreamSource_t)
#define VIDEO_SET_BLANK            _IOW('o', 26, boolean)
#define VIDEO_GET_STATUS           _IOR('o', 27, struct videoStatus *)
#define VIDEO_GET_EVENT            _IOR('o', 28, struct videoEvent *)
#define VIDEO_SET_DIPLAY_FORMAT    _IOW('o', 29, videoDisplayFormat_t)
#define VIDEO_STILLPICTURE         _IOW('o', 30, struct videoDisplayStillPicture *)
#define VIDEO_FAST_FORWARD         _IOW('o', 31, int)
#define VIDEO_SLOWMOTION           _IOW('o', 32, int)

#endif /*_OST_VIDEO_H_*/
