/*
 *   avia_av_napi.c - AViA 500/600 NAPI driver (dbox-II-project)
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
 *
 *   $Log: avia_av_napi.c,v $
 *   Revision 1.2  2002/11/04 23:18:39  Jolt
 *   More decoder work
 *
 *   Revision 1.1  2002/11/04 23:04:02  Jolt
 *   Some decoder work
 *
 *
 *
 *
 *
 *   $Revision: 1.2 $
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

#include "../dvb-core/dvbdev.h"

#include "avia_av.h"
#include "avia_av_napi.h"

static struct dvb_device *audio_dev;
static u8 dev_registered = 0;
static struct dvb_device *video_dev;

static int avia_av_napi_video_ioctl(struct inode *inode, struct file *file, unsigned int cmd, void *parg)
{

#if 0
	struct dvb_struct *dvb= (struct dvb_struct *)dvbdev->priv;
	void *parg=(void *)arg;
	int ret=0;
	u32 new_mode;

	if (((file->f_flags&O_ACCMODE)==O_RDONLY) &&
	    (cmd!=VIDEO_GET_STATUS))
		return -EPERM;

	switch (cmd) {
	case VIDEO_STOP:
		dvb->videostate.playState=VIDEO_STOPPED;
		avia_command(SelectStream, 0x00, 0xFFFF);
		break;

	case VIDEO_PLAY:
		if ((dvb->audiostate.playState != AUDIO_PLAYING) && (dvb->videostate.playState != VIDEO_PLAYING)) {
			switch (dvb->videostate.streamSource) {
			case VIDEO_SOURCE_DEMUX:
				printk("avia: playing vpid 0x%X apid: 0x%X\n", dvb->video_pid, dvb->audio_pid);
#ifdef AVIA_SPTS
				if (dvb->video_stream_type != STREAM_TYPE_SPTS_ES) {
					if (!aviarev) {
						avia_command(SetStreamType, 0x10, dvb->audio_pid);
						avia_command(SetStreamType, 0x11, dvb->video_pid);
					}
					else {
						avia_command(Reset);
					}
					dvb->video_stream_type = STREAM_TYPE_SPTS_ES;
					dvb->audio_stream_type = STREAM_TYPE_SPTS_ES;
				}
#else
				if ( (dvb->video_stream_type != STREAM_TYPE_DPES_PES) ||
					 (dvb->audio_stream_type != STREAM_TYPE_DPES_PES) ) {
					avia_command(SetStreamType, 0x0B, 0x0000);
					dvb->video_stream_type = STREAM_TYPE_DPES_PES;
					dvb->audio_stream_type = STREAM_TYPE_DPES_PES;
				}
#endif
				avia_command(SelectStream, 0x00, dvb->video_pid);
				avia_command(SelectStream, (dvb->audiostate.bypassMode) ? 0x03 : 0x02, dvb->audio_pid);
				avia_command(Play, 0x00, dvb->video_pid, dvb->audio_pid);
				break;

			case VIDEO_SOURCE_MEMORY:
				dvb->video_stream_type = STREAM_TYPE_DPES_PES;
				dvb->audio_stream_type = STREAM_TYPE_DPES_PES;
				avia_command(SelectStream, 0x0B, 0x0000);
				avia_command(Play, 0x00, 0x0000, 0x0000);
				break;

			default:
				return -EINVAL;
			}
			dvb->audiostate.playState = AUDIO_PLAYING;
			dvb->videostate.playState = VIDEO_PLAYING;
		}
		break;

	case VIDEO_FREEZE:
		dvb->videostate.playState=VIDEO_FREEZED;
		avia_command(Freeze, 0x01);
		break;

	case VIDEO_CONTINUE:
		avia_command(Resume);
		dvb->videostate.playState=VIDEO_PLAYING;
		break;

	case VIDEO_SELECT_SOURCE:
		if ((dvb->audiostate.playState == AUDIO_STOPPED) && (dvb->videostate.playState == VIDEO_STOPPED)) {

			switch ((videoStreamSource_t) arg) {
			case VIDEO_SOURCE_DEMUX:
				if (dvb->videostate.streamSource != VIDEO_SOURCE_DEMUX)
					dvb_select_source(dvb, 0);
				break;

			case VIDEO_SOURCE_MEMORY:
				if (dvb->videostate.streamSource != VIDEO_SOURCE_MEMORY)
					dvb_select_source(dvb, 1);
				break;

			default:
				ret=-EINVAL;
				break;
			}
		}
		else {
			ret=-EINVAL;
		}
		break;

	case VIDEO_SET_BLANK:
		dvb->videostate.videoBlank=(boolean) arg;
		break;

	case VIDEO_GET_STATUS:
		if (copy_to_user(parg, &dvb->videostate, sizeof(struct videoStatus)))
			ret=-EFAULT;
		break;

	case VIDEO_GET_EVENT:
		ret=-EOPNOTSUPP;
		break;

	case VIDEO_SET_DISPLAY_FORMAT:
	{
		videoDisplayFormat_t format=(videoDisplayFormat_t) arg;
		u16 val=0;

		switch (format) {
		case VIDEO_PAN_SCAN:
			val=1;
			break;

		case VIDEO_LETTER_BOX:
			val=2;
			break;

		case VIDEO_CENTER_CUT_OUT:
			val=0;
			break;

		default:
			ret=-EINVAL;
			break;
		}
		if (ret<0)
			break;
		dvb->videostate.displayFormat=format;
		wDR(ASPECT_RATIO_MODE, val);
		break;
	}

	case VIDEO_SET_FORMAT:
		dvb->videostate.videoFormat = (videoFormat_t) arg;

		switch (dvb->videostate.videoFormat) {
		case VIDEO_FORMAT_AUTO:
			wDR(FORCE_CODED_ASPECT_RATIO, 0);
			break;

		case VIDEO_FORMAT_4_3:
			wDR(FORCE_CODED_ASPECT_RATIO, 2);
			break;

		case VIDEO_FORMAT_16_9:
			wDR(FORCE_CODED_ASPECT_RATIO, 3);
			break;

		case VIDEO_FORMAT_20_9:
			wDR(FORCE_CODED_ASPECT_RATIO, 4);
			break;

		default:
			ret=-EINVAL;
			break;
		}
		break;

	case VIDEO_STILLPICTURE:
		if (dvb->videostate.playState == VIDEO_STOPPED)
			ret=-EOPNOTSUPP;
		else
			ret=-EINVAL;
		break;

	case VIDEO_FAST_FORWARD:
		ret=-EOPNOTSUPP;
		break;

	case VIDEO_SLOWMOTION:
		ret=-EOPNOTSUPP;
		break;

	case VIDEO_GET_CAPABILITIES:
	{
		int cap=VIDEO_CAP_MPEG1|
			VIDEO_CAP_MPEG2|
			VIDEO_CAP_SYS|
			VIDEO_CAP_PROG;

		if (copy_to_user(parg, &cap, sizeof(cap)))
			ret=-EFAULT;
		break;
	}

	case VIDEO_CLEAR_BUFFER:
		break;

	case VIDEO_SET_STREAMTYPE:
		if ( (streamType_t) arg > STREAM_TYPE_DPES_PES ) {
			ret = -EINVAL;
			break;
		}

		if (dvb->video_stream_type == (streamType_t) arg)
			break;

		if (rDR(PROC_STATE) != PROC_STATE_IDLE)
			avia_command(Abort, 1);

		if ( (streamType_t) arg == STREAM_TYPE_SPTS_ES ) {
			if (!aviarev) {
				avia_command(SetStreamType, 0x10, dvb->audio_pid);
				avia_command(SetStreamType, 0x11, dvb->video_pid);
			}
			else {	// AVIA 500 doesn't support SetStreamType with type 0x10/0x11
				avia_command(Reset);
			}
			dvb->video_stream_type = STREAM_TYPE_SPTS_ES;
			dvb->audio_stream_type = STREAM_TYPE_SPTS_ES;
			break;
		}

		if ( (streamType_t) arg == STREAM_TYPE_DPES_PES) {
			new_mode = 0x09;
		}
		else
		{
			new_mode = 0x08;
		}

		if ( dvb->audio_stream_type == STREAM_TYPE_SPTS_ES )
		{
			dvb->audio_stream_type = (streamType_t) arg;
		}

		if (dvb->audio_stream_type == STREAM_TYPE_DPES_PES) {
			new_mode |= 0x02;
		}

		avia_command(SetStreamType, new_mode, 0x0000);

		dvb->video_stream_type = (streamType_t) arg;

		break;

	default:
		ret=-ENOIOCTLCMD;
		break;
	}
	return ret;
#endif

	return 0;
	
}

static int avia_av_napi_audio_ioctl(struct inode *inode, struct file *file, unsigned int cmd, void *parg)
{

#if 0

	struct dvb_struct *dvb=(struct dvb_struct *) dvbdev->priv;
	void *parg=(void *)arg;
	int ret=0;
	u32 new_mode;

	if (((file->f_flags&O_ACCMODE)==O_RDONLY) &&
	    (cmd!=AUDIO_GET_STATUS))
		return -EPERM;

	switch (cmd) {
	case AUDIO_STOP:
		avia_command(SelectStream, (dvb->audiostate.bypassMode) ? 0x03 : 0x02, 0xFFFF);
		dvb->audiostate.playState=AUDIO_STOPPED;
		break;

	case AUDIO_PLAY:
		if ( (dvb->audiostate.playState != AUDIO_PLAYING) || (dvb->audio_stream_type == STREAM_TYPE_SPTS_ES) ) {
			switch (dvb->audiostate.streamSource) {
			case AUDIO_SOURCE_DEMUX:
				printk("avia: playing apid: 0x%X\n", dvb->audio_pid);
#ifdef AVIA_SPTS
				if (dvb->audio_stream_type != STREAM_TYPE_SPTS_ES) {
					if (!aviarev) {
						avia_command(SetStreamType, 0x10, dvb->audio_pid);
						avia_command(SetStreamType, 0x11, dvb->video_pid);
					}
					else {
						avia_command(Reset);
					}
					dvb->audio_stream_type = STREAM_TYPE_SPTS_ES;
					dvb->video_stream_type = STREAM_TYPE_SPTS_ES;
				}
#else
				if (dvb->audio_stream_type != STREAM_TYPE_DPES_PES)
				{
					if (dvb->video_stream_type == STREAM_TYPE_DPES_PES) {
						avia_command(SetStreamType, 0x0B, 0x0000);
					}
					else {
						avia_command(SetStreamType, 0x0A, 0x0000);
					}
					dvb->audio_stream_type = STREAM_TYPE_DPES_PES;
				}
#endif
				avia_command(SelectStream, (dvb->audiostate.bypassMode) ? 0x03 : 0x02, dvb->audio_pid);
				if (dvb->audiostate.playState != AUDIO_PLAYING) {
					if (dvb->videostate.playState == VIDEO_PLAYING) {
						avia_command(Play, 0x00, dvb->video_pid, dvb->audio_pid);
					}
					else {
						avia_command(Play, 0x00, 0xFFFF, dvb->audio_pid);
					}
				}
				break;

			case AUDIO_SOURCE_MEMORY:
				avia_command(SelectStream, 0x0B, 0x0000);
				avia_command(Play, 0x00, 0xFFFF, 0x0000);
				break;

			default:
				return -EINVAL;
			}
			dvb->audiostate.playState = AUDIO_PLAYING;
		}
		break;

	case AUDIO_PAUSE:
		if (dvb->audiostate.playState==AUDIO_PLAYING) {
			//avia_command(Pause, 1, 1); // freeze video, pause audio
			avia_command(Pause, 1, 2); // pause audio (v2.0 silicon only)
			dvb->audiostate.playState=AUDIO_PAUSED;
		} else {
			ret=-EINVAL;
		}
		break;

	case AUDIO_CONTINUE:
		if (dvb->audiostate.playState==AUDIO_PAUSED) {
			dvb->audiostate.playState=AUDIO_PLAYING;
			avia_command(Resume);
		}
		break;

	case AUDIO_SELECT_SOURCE:
		if ((dvb->audiostate.playState == AUDIO_STOPPED) && (dvb->videostate.playState == VIDEO_STOPPED)) {
			switch ((audioStreamSource_t) arg) {
			case AUDIO_SOURCE_DEMUX:
				if (dvb->audiostate.streamSource != AUDIO_SOURCE_DEMUX)
					ret = dvb_select_source(dvb, 0);
				break;

			case AUDIO_SOURCE_MEMORY:
				if (dvb->audiostate.streamSource != AUDIO_SOURCE_MEMORY)
					ret = dvb_select_source(dvb, 1);
				break;

			default:
				ret=-EINVAL;
				break;
			}
		}
		else {
			ret=-EINVAL;
		}
		break;

	case AUDIO_SET_MUTE:
		if (arg) {
			/* mute av spdif (2) and analog audio (4) */
			wDR(AUDIO_CONFIG, rDR(AUDIO_CONFIG) & ~6);
			/* mute gt mpeg */
			avia_gt_pcm_set_mpeg_attenuation(0x00, 0x00);
		} else {
			/* unmute av spdif (2) and analog audio (4) */
			wDR(AUDIO_CONFIG, rDR(AUDIO_CONFIG) | 6);
			/* unmute gt mpeg */
			avia_gt_pcm_set_mpeg_attenuation((dvb->audiomixer.volume_left + 1) >> 1, (dvb->audiomixer.volume_right + 1) >> 1);
		}
		wDR(NEW_AUDIO_CONFIG, 1);
		dvb->audiostate.muteState=(boolean) arg;
		break;

	case AUDIO_SET_AV_SYNC:
		dvb->audiostate.AVSyncState=(boolean) arg;
		wDR(AV_SYNC_MODE, arg ? 0x06 : 0x00);
		break;

	case AUDIO_SET_BYPASS_MODE:
		if (arg) {
			avia_command(SelectStream, 0x02, 0xffff);
			avia_command(SelectStream, 0x03, dvb->audio_pid);
			wDR(AUDIO_CONFIG, rDR(AUDIO_CONFIG) | 1);
		} else {
			avia_command(SelectStream, 0x03, 0xffff);
			avia_command(SelectStream, 0x02, dvb->audio_pid);
			wDR(AUDIO_CONFIG, rDR(AUDIO_CONFIG) & ~1);
		}
		wDR(NEW_AUDIO_CONFIG, 1);
		dvb->audiostate.bypassMode=(boolean) arg;
		break;

	case AUDIO_CHANNEL_SELECT:
		dvb->audiostate.channelSelect=(audioChannelSelect_t) arg;

		switch (dvb->audiostate.channelSelect) {
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
			ret=-EINVAL;
			break;
		}
		break;

	case AUDIO_GET_STATUS:
		if (copy_to_user(parg, &dvb->audiostate, sizeof(struct audioStatus)))
			ret=-EFAULT;
		break;

	case AUDIO_GET_CAPABILITIES:
	{
		int cap=AUDIO_CAP_LPCM|
			AUDIO_CAP_MP1|
			AUDIO_CAP_MP2|
			AUDIO_CAP_AC3;

		if (copy_to_user(parg, &cap, sizeof(cap)))
			ret=-EFAULT;
		break;
	}

	case AUDIO_CLEAR_BUFFER:
		break;
	
	case AUDIO_SET_ID:
		break;

	case AUDIO_SET_MIXER:
		memcpy(&dvb->audiomixer, parg, sizeof(struct audioMixer));

		if (dvb->audiomixer.volume_left > AUDIO_MIXER_MAX_VOLUME)
			dvb->audiomixer.volume_left = AUDIO_MIXER_MAX_VOLUME;
		
		if (dvb->audiomixer.volume_right > AUDIO_MIXER_MAX_VOLUME)
			dvb->audiomixer.volume_right = AUDIO_MIXER_MAX_VOLUME;

		avia_gt_pcm_set_mpeg_attenuation((dvb->audiomixer.volume_left + 1) >> 1,
				(dvb->audiomixer.volume_right + 1) >> 1);
		break;

	case AUDIO_SET_STREAMTYPE:
		if ( (streamType_t) arg > STREAM_TYPE_DPES_PES ) {
			ret = -EINVAL;
			break;
		}

		if (dvb->audio_stream_type == (streamType_t)arg)
			break;

		if (rDR(PROC_STATE) != PROC_STATE_IDLE)
			avia_command(Abort, 1);

		if ( (streamType_t) arg == STREAM_TYPE_SPTS_ES) {
			if (!aviarev) {
				avia_command(SetStreamType, 0x10,dvb->audio_pid);
				avia_command(SetStreamType, 0x11,dvb->video_pid);
			}
			else { // AVIA 500 doesn't support SetStreamType with types 0x10, 0x11
				avia_command(Reset);
			}
			dvb->audio_stream_type = STREAM_TYPE_SPTS_ES;
			dvb->video_stream_type = STREAM_TYPE_SPTS_ES;
			break;
		}

		if ( (streamType_t) arg == STREAM_TYPE_DPES_PES) {
			new_mode = 0x0A;
		}
		else {
			new_mode = 0x08;
		}

		if ( dvb->video_stream_type == STREAM_TYPE_SPTS_ES )
		{
			dvb->video_stream_type = (streamType_t) arg;
		}

		if (dvb->video_stream_type == STREAM_TYPE_DPES_PES) {
			new_mode |= 0x01;
		}

		avia_command(SetStreamType, new_mode, 0x0000);

		dvb->audio_stream_type = (streamType_t) arg;

		break;

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

	default:
		ret=-ENOIOCTLCMD;
		break;
	}
	return ret;

#endif

	return 0;

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

int avia_av_napi_register(struct dvb_adapter *adapter, void *priv)
{

	int result;

	avia_av_napi_unregister();

	if ((result = dvb_register_device(adapter, &video_dev, &avia_av_napi_video_dev, priv, DVB_DEVICE_VIDEO)) < 0) {

		printk("avia_av_napi: dvb_register_device (video) failed (errno = %d)\n", result);

		return result;
		
	}

	if ((result = dvb_register_device(adapter, &audio_dev, &avia_av_napi_audio_dev, priv, DVB_DEVICE_AUDIO)) < 0) {

		printk("avia_av_napi: dvb_register_device (audio) failed (errno = %d)\n", result);

		dvb_unregister_device(video_dev);
		
		return result;
		
	}

	dev_registered = 1;
	
	return 0;

}


void avia_av_napi_unregister(void)
{

	if (dev_registered) {

		dvb_unregister_device(audio_dev);
		dvb_unregister_device(video_dev);

		dev_registered = 0;
		
	}

}

int avia_av_napi_init(void)
{

	printk("avia_av_napi: $Id: avia_av_napi.c,v 1.2 2002/11/04 23:18:39 Jolt Exp $\n");

	return 0;

}

void avia_av_napi_exit(void)
{

	avia_av_napi_unregister();

}

EXPORT_SYMBOL(avia_av_napi_register);
EXPORT_SYMBOL(avia_av_napi_unregister);

#if defined(MODULE)
module_init(avia_av_napi_init);
module_exit(avia_av_napi_exit);
#endif
