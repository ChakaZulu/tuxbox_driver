/*
 * $Id: avia_av_napi.c,v 1.14 2003/01/11 22:45:16 obi Exp $
 *
 * AViA 500/600 DVB API driver (dbox-II-project)
 *
 * Homepage: http://dbox2.elxsi.de
 *
 * Copyright (C) 2002 Florian Schirmer (jolt@tuxbox.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <linux/init.h>

#include "../dvb-core/demux.h"
#include "../dvb-core/dvb_demux.h"
#include "../dvb-core/dvbdev.h"
#include <linux/dvb/video.h>
#include <linux/dvb/audio.h>

#include "avia_napi.h"
#include "avia_av.h"
#include "avia_av_napi.h"
#include "avia_gt_pcm.h"

static struct dvb_device *audio_dev;
static struct dvb_device *video_dev;
static struct audio_status audiostate;
static struct video_status videostate;
static u8 have_audio = 0;
static u8 have_video = 0;

int avia_av_napi_decoder_start(struct dvb_demux_feed *dvbdmxfeed)
{

	if ((dvbdmxfeed->type != DMX_TYPE_TS) && (dvbdmxfeed->type != DMX_TYPE_PES))
		return -EINVAL;

	if ((dvbdmxfeed->pes_type != DMX_TS_PES_AUDIO) && (dvbdmxfeed->pes_type != DMX_TS_PES_VIDEO))
		return -EINVAL;

	if (!(dvbdmxfeed->ts_type & TS_DECODER))
		return -EINVAL;

	if (dvbdmxfeed->pes_type == DMX_TS_PES_AUDIO) {

		avia_av_pid_set(AVIA_AV_TYPE_AUDIO, dvbdmxfeed->pid);

		if (dvbdmxfeed->ts_type & TS_PAYLOAD_ONLY)
			avia_av_stream_type_set(AVIA_AV_STREAM_TYPE_PES, AVIA_AV_STREAM_TYPE_PES);
		else
			avia_av_stream_type_set(AVIA_AV_STREAM_TYPE_SPTS, AVIA_AV_STREAM_TYPE_SPTS);

		if (audiostate.play_state == AUDIO_PLAYING)
			avia_av_play_state_set_audio(AVIA_AV_PLAY_STATE_PLAYING);

		have_audio = 1;

	} else {
			
		avia_av_pid_set(AVIA_AV_TYPE_VIDEO, dvbdmxfeed->pid);

		if (dvbdmxfeed->ts_type & TS_PAYLOAD_ONLY)
			avia_av_stream_type_set(AVIA_AV_STREAM_TYPE_PES, AVIA_AV_STREAM_TYPE_PES);
		else
			avia_av_stream_type_set(AVIA_AV_STREAM_TYPE_SPTS, AVIA_AV_STREAM_TYPE_SPTS);

		if (videostate.play_state == VIDEO_PLAYING)
			avia_av_play_state_set_video(AVIA_AV_PLAY_STATE_PLAYING);

		have_video = 1;
		
	}

	return 0;
		
}

int avia_av_napi_decoder_stop(struct dvb_demux_feed *dvbdmxfeed)
{

	if ((dvbdmxfeed->type != DMX_TYPE_TS) && (dvbdmxfeed->type != DMX_TYPE_PES))
		return -EINVAL;

	if ((dvbdmxfeed->pes_type != DMX_TS_PES_AUDIO) && (dvbdmxfeed->pes_type != DMX_TS_PES_VIDEO))
		return -EINVAL;

	if (!(dvbdmxfeed->ts_type & TS_DECODER))
		return -EINVAL;

	if (dvbdmxfeed->pes_type == DMX_TS_PES_AUDIO)
		have_audio = 0;
	else
		have_video = 0;

	return 0;
	
}

static int avia_av_napi_video_ioctl(struct inode *inode, struct file *file, unsigned int cmd, void *parg)
{
	
	unsigned long arg = (unsigned long)parg;
	int ret = 0;

	if (((file->f_flags & O_ACCMODE) == O_RDONLY) && (cmd != VIDEO_GET_STATUS))
		return -EPERM;

	switch (cmd) {

		case VIDEO_STOP:

			avia_av_play_state_set_video(AVIA_AV_PLAY_STATE_STOPPED);
			videostate.play_state = VIDEO_STOPPED;

			break;

		case VIDEO_PLAY:

			if (have_video)
				avia_av_play_state_set_video(AVIA_AV_PLAY_STATE_PLAYING);

			videostate.play_state = VIDEO_PLAYING;

			break;

		case VIDEO_FREEZE:

			avia_av_play_state_set_video(AVIA_AV_PLAY_STATE_PAUSED);
			videostate.play_state = VIDEO_FREEZED;

			break;

		case VIDEO_CONTINUE:

			avia_av_play_state_set_video(AVIA_AV_PLAY_STATE_PLAYING);
			videostate.play_state = VIDEO_PLAYING;

			break;

		case VIDEO_SELECT_SOURCE:
			{
				video_stream_source_t source = (video_stream_source_t)parg;

				if (videostate.play_state == VIDEO_STOPPED) {

					switch (source) {

						case VIDEO_SOURCE_DEMUX:

//FIXME							if (videostate.stream_source != VIDEO_SOURCE_DEMUX)
//FIXME								dvb_select_source(dvb, 0);

							break;

						case VIDEO_SOURCE_MEMORY:

//FIXME							if (videostate.stream_source != VIDEO_SOURCE_MEMORY)
//FIXME								dvb_select_source(dvb, 1);

							break;

						default:

							return -EINVAL;

					}

					videostate.stream_source = source;

				} else {

					ret = -EINVAL;

				}
			}

			break;

		case VIDEO_SET_BLANK:

			videostate.video_blank = (arg != 0);

			break;

		case VIDEO_GET_STATUS:

			memcpy(parg, &videostate, sizeof(struct video_status));

			break;

		case VIDEO_GET_EVENT:

			ret = -EOPNOTSUPP;

			break;

		case VIDEO_SET_DISPLAY_FORMAT:
			{

				video_displayformat_t format = (video_displayformat_t) parg;

				u16 val = 0;

				switch (format) {

					case VIDEO_PAN_SCAN:

						val = 1;

						break;

					case VIDEO_LETTER_BOX:

						val = 2;

						break;

					case VIDEO_CENTER_CUT_OUT:

						val = 0;

						break;

					default:

						return -EINVAL;
				}

				videostate.display_format = format;
				wDR(ASPECT_RATIO_MODE, val);

				break;

			}

		case VIDEO_SET_FORMAT:
			{
				video_format_t format = (video_format_t) parg;

				switch (format) {

					case VIDEO_FORMAT_4_3:

						wDR(FORCE_CODED_ASPECT_RATIO, 2);

						break;

					case VIDEO_FORMAT_16_9:

						wDR(FORCE_CODED_ASPECT_RATIO, 3);

						break;

					default:

						return -EINVAL;

				}

				videostate.video_format = format;

				break;
			}

		case VIDEO_STILLPICTURE:

			if (videostate.play_state == VIDEO_STOPPED)
				ret = -EOPNOTSUPP;
			else
				ret = -EINVAL;

			break;

		case VIDEO_FAST_FORWARD:

			ret = -EOPNOTSUPP;

			break;

		case VIDEO_SLOWMOTION:

			ret = -EOPNOTSUPP;

			break;

		case VIDEO_GET_CAPABILITIES:

			*(int *) parg = VIDEO_CAP_MPEG1 | VIDEO_CAP_MPEG2 | VIDEO_CAP_SYS | VIDEO_CAP_PROG;

			break;

		case VIDEO_CLEAR_BUFFER:
			break;

		default:

			ret = -ENOIOCTLCMD;

			break;

	}

	return ret;

}

static int avia_av_napi_audio_ioctl(struct inode *inode, struct file *file, unsigned int cmd, void *parg)
{

	unsigned long arg = (unsigned long)parg;
	int ret = 0;

	if (((file->f_flags & O_ACCMODE) == O_RDONLY) && (cmd != AUDIO_GET_STATUS))
		return -EPERM;

	switch (cmd) {

		case AUDIO_STOP:

			avia_av_play_state_set_audio(AVIA_AV_PLAY_STATE_STOPPED);

			audiostate.play_state = AUDIO_STOPPED;

			break;

		case AUDIO_PLAY:

			if (have_audio)
				avia_av_play_state_set_audio(AVIA_AV_PLAY_STATE_PLAYING);

			audiostate.play_state = AUDIO_PLAYING;
			
			break;

		case AUDIO_PAUSE:
		
			if (audiostate.play_state == AUDIO_PLAYING) {

				avia_av_play_state_set_audio(AVIA_AV_PLAY_STATE_PAUSED);
				audiostate.play_state = AUDIO_PAUSED;

			}
			
			break;

		case AUDIO_CONTINUE:

			if (audiostate.play_state == AUDIO_PAUSED) {

				avia_av_play_state_set_audio(AVIA_AV_PLAY_STATE_PLAYING);
				audiostate.play_state = AUDIO_PLAYING;

			}

			break;

		case AUDIO_SELECT_SOURCE:

			if (audiostate.play_state == AUDIO_STOPPED) {

				audio_stream_source_t source = (audio_stream_source_t) parg;
				
				audiostate.stream_source = source;

			} else {
				ret = -EINVAL;
			}
			break;

		case AUDIO_SET_MUTE:
		
			if (arg) {
				/*
				 * mute av spdif (2) and analog audio (4) 
				 */
				wDR(AUDIO_CONFIG, rDR(AUDIO_CONFIG) & ~6);
				/*
				 * mute gt mpeg 
				 */
//FIXME				avia_gt_pcm_set_mpeg_attenuation(0x00, 0x00);
			} else {
				/*
				 * unmute av spdif (2) and analog audio (4) 
				 */
				wDR(AUDIO_CONFIG, rDR(AUDIO_CONFIG) | 6);
				/*
				 * unmute gt mpeg 
				 */
//FIXME				avia_gt_pcm_set_mpeg_attenuation((audiostate.mixer_state.volume_left + 1) >> 1,
//FIXME								(audiostate.mixer_state.volume_right + 1) >> 1);
			}
			
			wDR(NEW_AUDIO_CONFIG, 1);
			audiostate.mute_state = (arg != 0);
			
			break;

		case AUDIO_SET_AV_SYNC:
		
			if (arg)
				avia_av_sync_mode_set(AVIA_AV_SYNC_MODE_AV);
			else
				avia_av_sync_mode_set(AVIA_AV_SYNC_MODE_NONE);
				
			audiostate.AV_sync_state = arg;
			
			break;

		case AUDIO_SET_BYPASS_MODE:

			avia_av_bypass_mode_set(!arg);
			
			audiostate.bypass_mode = arg;
			
			break;

		case AUDIO_CHANNEL_SELECT:
			{
				audio_channel_select_t select = (audio_channel_select_t) parg;

				switch (select) {
					case AUDIO_STEREO:
						wDR(AUDIO_DAC_MODE, rDR(AUDIO_DAC_MODE) & ~0x30);
						wDR(NEW_AUDIO_CONFIG, 1);
						break;

					case AUDIO_MONO_LEFT:
						wDR(AUDIO_DAC_MODE, (rDR(AUDIO_DAC_MODE) & ~0x30) | 0x10);
						wDR(NEW_AUDIO_CONFIG, 1);
						break;

					case AUDIO_MONO_RIGHT:
						wDR(AUDIO_DAC_MODE, (rDR(AUDIO_DAC_MODE) & ~0x30) | 0x20);
						wDR(NEW_AUDIO_CONFIG, 1);
						break;

					default:
						return -EINVAL;
				}
				
				audiostate.channel_select = select;
				
				break;
			}

		case AUDIO_GET_STATUS:
			memcpy(parg, &audiostate, sizeof(struct audio_status));
			break;

		case AUDIO_GET_CAPABILITIES:
			*(int *) parg = AUDIO_CAP_LPCM | AUDIO_CAP_MP1 | AUDIO_CAP_MP2 | AUDIO_CAP_AC3;
			break;

		case AUDIO_CLEAR_BUFFER:
			break;

		case AUDIO_SET_ID:
			break;

		case AUDIO_SET_MIXER:
			memcpy(&audiostate.mixer_state, parg, sizeof(struct audio_mixer));

			if (audiostate.mixer_state.volume_left > 255)
				audiostate.mixer_state.volume_left = 255;
			if (audiostate.mixer_state.volume_right > 255)
				audiostate.mixer_state.volume_right = 255;
			
			avia_gt_pcm_set_mpeg_attenuation((audiostate.mixer_state.volume_left + 1) >> 1,
											(audiostate.mixer_state.volume_right + 1) >> 1);
			break;

#if 0
		case AUDIO_SET_SPDIF_COPIES:
			switch ((audioSpdifCopyState_t) arg) {
				case SCMS_COPIES_NONE:
					wDR(IEC_958_CHANNEL_STATUS_BITS, (rDR(IEC_958_CHANNEL_STATUS_BITS) & ~1) | 4);
					break;

				case SCMS_COPIES_ONE:
					wDR(IEC_958_CHANNEL_STATUS_BITS, rDR(IEC_958_CHANNEL_STATUS_BITS) & ~5);
					break;

				case SCMS_COPIES_UNLIMITED:
					wDR(IEC_958_CHANNEL_STATUS_BITS, rDR(IEC_958_CHANNEL_STATUS_BITS) | 5);
					break;

				default:
					return -EINVAL;
			}

			wDR(NEW_AUDIO_CONFIG, 1);
			break;
#endif
		default:
			ret = -ENOIOCTLCMD;
			break;
	}

	return ret;

}

static struct file_operations avia_av_napi_video_fops = {

	.owner = THIS_MODULE,
	//.write = avia_napi_video_write,
	.ioctl = dvb_generic_ioctl,
	//.open = avia_napi_video_open,
	.open = dvb_generic_open,
	//.release = avia_napi_video_release,
	.release = dvb_generic_release,
	//.poll = avia_napi_video_poll,

};

static struct dvb_device avia_av_napi_video_dev = {

	.priv = 0,
	.users = 1,
	.writers = 1,
	.fops = &avia_av_napi_video_fops,
	.kernel_ioctl = avia_av_napi_video_ioctl,

};

static struct file_operations avia_av_napi_audio_fops = {

	.owner = THIS_MODULE,
	//.write = avia_av_napi_audio_write,
	.ioctl = dvb_generic_ioctl,
	//.open = avia_av_napi_audio_open,
	.open = dvb_generic_open,
	//.release = avia_av_napi_audio_release,
	.release = dvb_generic_release,
	//.poll = avia_av_napi_audio_poll,

};

static struct dvb_device avia_av_napi_audio_dev = {

	.priv = 0,
	.users = 1,
	.writers = 1,
	.fops = &avia_av_napi_audio_fops,
	.kernel_ioctl = avia_av_napi_audio_ioctl,

};

int __init avia_av_napi_init(void)
{

	int result;

	printk("avia_av_napi: $Id: avia_av_napi.c,v 1.14 2003/01/11 22:45:16 obi Exp $\n");

	audiostate.AV_sync_state = 0;
	audiostate.mute_state = 0;
	audiostate.play_state = AUDIO_STOPPED;
	audiostate.stream_source = AUDIO_SOURCE_DEMUX;
	audiostate.channel_select = AUDIO_STEREO;
	audiostate.bypass_mode = 1;
	audiostate.mixer_state.volume_left = 0;
	audiostate.mixer_state.volume_right = 0;

	videostate.video_blank = 0;
	videostate.play_state = VIDEO_STOPPED;
	videostate.stream_source = VIDEO_SOURCE_DEMUX;
	videostate.video_format = VIDEO_FORMAT_4_3;
	videostate.display_format = VIDEO_CENTER_CUT_OUT;

	if ((result = dvb_register_device(avia_napi_get_adapter(), &video_dev, &avia_av_napi_video_dev, NULL, DVB_DEVICE_VIDEO)) < 0) {

		printk("avia_av_napi: dvb_register_device (video) failed (errno = %d)\n", result);

		return result;

	}

	if ((result = dvb_register_device(avia_napi_get_adapter(), &audio_dev, &avia_av_napi_audio_dev, NULL, DVB_DEVICE_AUDIO)) < 0) {

		printk("avia_av_napi: dvb_register_device (audio) failed (errno = %d)\n", result);

		dvb_unregister_device(video_dev);

		return result;

	}

	return 0;

}

void __exit avia_av_napi_exit(void)
{

	dvb_unregister_device(audio_dev);
	dvb_unregister_device(video_dev);

}

module_init(avia_av_napi_init);
module_exit(avia_av_napi_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>, Andreas Oberritter <obi@tuxbox.org>");
MODULE_DESCRIPTION("AViA 500/600 dvb api driver");
MODULE_LICENSE("GPL");

