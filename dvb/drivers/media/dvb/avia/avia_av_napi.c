/*
 * $Id: avia_av_napi.c,v 1.24 2003/09/11 23:22:48 obi Exp $
 *
 * AViA 500/600 DVB API driver (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
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
#include <linux/bitops.h>
#include <asm/irq.h>
#include <asm/io.h>
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
#include "avia_gt_dmx.h"
#include "avia_gt_napi.h"
#include "avia_gt_pcm.h"

static struct dvb_device *audio_dev = NULL;
static struct dvb_device *video_dev = NULL;
static struct audio_status audiostate;
static struct video_status videostate;
static wait_queue_head_t audio_wait_queue;
static wait_queue_head_t video_wait_queue;
static u8 have_audio = 0;
static u8 have_video = 0;

/* used for playback from memory */
static int need_audio_pts = 0;

/*
 * MPEG1 PES & MPEG2 PES
 * SPTS and ES are undefined atm.
 */
#define AVIA_AV_VIDEO_CAPS	(VIDEO_CAP_MPEG1 | VIDEO_CAP_MPEG2)

/*
 * MPEG1 PES & MPEG2 PES
 * ES is undefined atm.
 */
#define AVIA_AV_AUDIO_CAPS	(AUDIO_CAP_MP1 | AUDIO_CAP_MP2 | AUDIO_CAP_AC3)


int avia_av_napi_decoder_start(struct dvb_demux_feed *dvbdmxfeed)
{
	if ((dvbdmxfeed->type != DMX_TYPE_TS) && (dvbdmxfeed->type != DMX_TYPE_PES))
		return -EINVAL;

	if (!(dvbdmxfeed->ts_type & TS_DECODER))
		return -EINVAL;

	switch (dvbdmxfeed->pes_type) {
	case DMX_TS_PES_AUDIO:
		avia_av_pid_set(AVIA_AV_TYPE_AUDIO, dvbdmxfeed->pid);

		if (dvbdmxfeed->ts_type & TS_PAYLOAD_ONLY)
			avia_av_stream_type_set(AVIA_AV_STREAM_TYPE_PES, AVIA_AV_STREAM_TYPE_PES);
		else
			avia_av_stream_type_set(AVIA_AV_STREAM_TYPE_0, AVIA_AV_STREAM_TYPE_0);

		if (audiostate.play_state == AUDIO_PLAYING)
			avia_av_play_state_set_audio(AVIA_AV_PLAY_STATE_PLAYING);

		have_audio = 1;
		break;

	case DMX_TS_PES_VIDEO:
		avia_av_pid_set(AVIA_AV_TYPE_VIDEO, dvbdmxfeed->pid);

		if (dvbdmxfeed->ts_type & TS_PAYLOAD_ONLY)
			avia_av_stream_type_set(AVIA_AV_STREAM_TYPE_PES, AVIA_AV_STREAM_TYPE_PES);
		else
			avia_av_stream_type_set(AVIA_AV_STREAM_TYPE_0, AVIA_AV_STREAM_TYPE_0);

		if (videostate.play_state == VIDEO_PLAYING)
			avia_av_play_state_set_video(AVIA_AV_PLAY_STATE_PLAYING);

		have_video = 1;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

int avia_av_napi_decoder_stop(struct dvb_demux_feed *dvbdmxfeed)
{
	if ((dvbdmxfeed->type != DMX_TYPE_TS) && (dvbdmxfeed->type != DMX_TYPE_PES))
		return -EINVAL;

	if (!(dvbdmxfeed->ts_type & TS_DECODER))
		return -EINVAL;

	switch (dvbdmxfeed->pes_type) {
	case DMX_TS_PES_AUDIO:
		avia_av_play_state_set_audio(AVIA_AV_PLAY_STATE_STOPPED);
		have_audio = 0;
		break;

	case DMX_TS_PES_VIDEO:
		avia_av_play_state_set_video(AVIA_AV_PLAY_STATE_STOPPED);
		have_video = 0;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static
int avia_av_napi_video_ioctl(struct inode *inode, struct file *file, unsigned int cmd, void *parg)
{
	unsigned long arg = (unsigned long) parg;
	int err;

	if (((file->f_flags & O_ACCMODE) == O_RDONLY) &&
		(cmd != VIDEO_GET_STATUS) &&
		(cmd != VIDEO_GET_EVENT) &&
		(cmd != VIDEO_GET_SIZE))
		return -EPERM;

	switch (cmd) {
	case VIDEO_STOP:
		avia_av_play_state_set_video(AVIA_AV_PLAY_STATE_STOPPED);
		videostate.play_state = VIDEO_STOPPED;
		break;

	case VIDEO_PLAY:
		if ((have_video) || (videostate.stream_source == VIDEO_SOURCE_MEMORY))
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
		if (videostate.play_state != VIDEO_STOPPED)
			return -EINVAL;
		switch ((video_stream_source_t) arg) {
		case VIDEO_SOURCE_DEMUX:
			if ((err = avia_gt_dmx_disable_clip_mode(AVIA_GT_DMX_QUEUE_VIDEO)) < 0)
				return err;
			break;
		case VIDEO_SOURCE_MEMORY:
			if ((err = avia_gt_dmx_enable_clip_mode(AVIA_GT_DMX_QUEUE_VIDEO)) < 0)
				return err;
			break;
		default:
			return -EINVAL;
		}
		videostate.stream_source = (video_stream_source_t) arg;
		break;

	case VIDEO_SET_BLANK:
		videostate.video_blank = !!arg;
		break;

	case VIDEO_GET_STATUS:
		memcpy(parg, &videostate, sizeof(struct video_status));
		break;

	case VIDEO_GET_EVENT:
		return -EOPNOTSUPP;

	case VIDEO_SET_DISPLAY_FORMAT:
	{
		video_displayformat_t format = (video_displayformat_t) arg;

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
		avia_av_dram_write(ASPECT_RATIO_MODE, val);
		break;
	}

	case VIDEO_STILLPICTURE:
		if (videostate.play_state != VIDEO_STOPPED)
			return -EINVAL;
		return -EOPNOTSUPP;

	case VIDEO_FAST_FORWARD:
		return -EOPNOTSUPP;

	case VIDEO_SLOWMOTION:
		return -EOPNOTSUPP;

	case VIDEO_GET_CAPABILITIES:
		*((unsigned int *)parg) = AVIA_AV_VIDEO_CAPS;
		break;

	case VIDEO_CLEAR_BUFFER:
		return -EOPNOTSUPP;

	case VIDEO_SET_ID:
		return -EOPNOTSUPP;

	case VIDEO_SET_STREAMTYPE:
		if ((!(arg & AVIA_AV_VIDEO_CAPS)) || (hweight32(arg) != 1))
			return -EINVAL;
		switch (arg) {
		case VIDEO_CAP_MPEG1:
		case VIDEO_CAP_MPEG2:
			return avia_av_stream_type_set(AVIA_AV_STREAM_TYPE_PES, AVIA_AV_STREAM_TYPE_PES);
		default:
			break;
		}
		break;

	case VIDEO_SET_FORMAT:
	{
		video_format_t format = (video_format_t) arg;

		switch (format) {
		case VIDEO_FORMAT_4_3:
			avia_av_dram_write(FORCE_CODED_ASPECT_RATIO, 2);
			break;
		case VIDEO_FORMAT_16_9:
			avia_av_dram_write(FORCE_CODED_ASPECT_RATIO, 3);
			break;
		case VIDEO_FORMAT_221_1:
			avia_av_dram_write(FORCE_CODED_ASPECT_RATIO, 4);
			break;
		default:
			return -EINVAL;
		}

		videostate.video_format = format;
		break;
	}

	case VIDEO_SET_SYSTEM:
		return -EOPNOTSUPP;

	case VIDEO_SET_HIGHLIGHT:
		return -EOPNOTSUPP;

	case VIDEO_SET_SPU:
		return -EOPNOTSUPP;

	case VIDEO_SET_SPU_PALETTE:
		return -EOPNOTSUPP;

	case VIDEO_GET_NAVI:
		return -EOPNOTSUPP;

	case VIDEO_SET_ATTRIBUTES:
		return -EOPNOTSUPP;

	case VIDEO_GET_SIZE:
	{
		video_size_t *s = parg;

		s->w = avia_av_dram_read(H_SIZE) & 0xffff;
		s->h = avia_av_dram_read(V_SIZE) & 0xffff;

		switch (avia_av_dram_read(ASPECT_RATIO) & 0xffff) {
		case 2:
			s->aspect_ratio = VIDEO_FORMAT_4_3;
			break;
		case 3:
			s->aspect_ratio = VIDEO_FORMAT_16_9;
			break;
		case 4:
			s->aspect_ratio = VIDEO_FORMAT_221_1;
			break;
		default:
			s->aspect_ratio = VIDEO_FORMAT_4_3;
			printk(KERN_INFO "avia_av_napi: unknown aspect ratio: %x\n",
					avia_av_dram_read(ASPECT_RATIO) & 0xffff);
			break;
		}

		break;
	}

	default:
		return -ENOIOCTLCMD;
	}

	return 0;
}

static
int avia_av_napi_audio_ioctl(struct inode *inode, struct file *file, unsigned int cmd, void *parg)
{
	unsigned long arg = (unsigned long) parg;
	int err;

	if (((file->f_flags & O_ACCMODE) == O_RDONLY) &&
		(cmd != AUDIO_GET_STATUS))
		return -EPERM;

	switch (cmd) {
	case AUDIO_STOP:
		avia_av_play_state_set_audio(AVIA_AV_PLAY_STATE_STOPPED);
		audiostate.play_state = AUDIO_STOPPED;
		break;

	case AUDIO_PLAY:
		if ((have_audio) || (audiostate.stream_source == AUDIO_SOURCE_MEMORY))
			avia_av_play_state_set_audio(AVIA_AV_PLAY_STATE_PLAYING);
		need_audio_pts = 1;
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
		if (audiostate.play_state != AUDIO_STOPPED)
			return -EINVAL;
		switch ((audio_stream_source_t) arg) {
		case AUDIO_SOURCE_DEMUX:
			if ((err = avia_gt_dmx_disable_clip_mode(AVIA_GT_DMX_QUEUE_AUDIO)) < 0)
				return err;
			break;
		case AUDIO_SOURCE_MEMORY:
			if ((err = avia_gt_dmx_enable_clip_mode(AVIA_GT_DMX_QUEUE_AUDIO)) < 0)
				return err;
			break;
		default:
			return -EINVAL;
		}
		audiostate.stream_source = (audio_stream_source_t) arg;
		break;

	case AUDIO_SET_MUTE:
		if (arg) {
			/* mute av spdif (2) and analog audio (4) */
			avia_av_dram_write(AUDIO_CONFIG, avia_av_dram_read(AUDIO_CONFIG) & ~6);
		} else {
			/* unmute av spdif (2) and analog audio (4) */
			avia_av_dram_write(AUDIO_CONFIG, avia_av_dram_read(AUDIO_CONFIG) | 6);
		}
		avia_av_dram_write(NEW_AUDIO_CONFIG, 1);
		audiostate.mute_state = !!arg;
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
		audio_channel_select_t select = (audio_channel_select_t) arg;

		switch (select) {
		case AUDIO_STEREO:
			avia_av_dram_write(AUDIO_DAC_MODE, avia_av_dram_read(AUDIO_DAC_MODE) & ~0x30);
			avia_av_dram_write(NEW_AUDIO_CONFIG, 1);
			break;

		case AUDIO_MONO_LEFT:
			avia_av_dram_write(AUDIO_DAC_MODE, (avia_av_dram_read(AUDIO_DAC_MODE) & ~0x30) | 0x10);
			avia_av_dram_write(NEW_AUDIO_CONFIG, 1);
			break;

		case AUDIO_MONO_RIGHT:
			avia_av_dram_write(AUDIO_DAC_MODE, (avia_av_dram_read(AUDIO_DAC_MODE) & ~0x30) | 0x20);
			avia_av_dram_write(NEW_AUDIO_CONFIG, 1);
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
		*((unsigned int *)parg) = AVIA_AV_AUDIO_CAPS;
		break;

	case AUDIO_CLEAR_BUFFER:
		return -EOPNOTSUPP;

	case AUDIO_SET_ID:
		return -EOPNOTSUPP;

	case AUDIO_SET_MIXER:
		memcpy(&audiostate.mixer_state, parg, sizeof(struct audio_mixer));

		if (audiostate.mixer_state.volume_left > 255)
			audiostate.mixer_state.volume_left = 255;
		if (audiostate.mixer_state.volume_right > 255)
			audiostate.mixer_state.volume_right = 255;

		avia_av_set_audio_attenuation(((255 - max(audiostate.mixer_state.volume_left, audiostate.mixer_state.volume_right)) * 96) / 255);
		avia_gt_pcm_set_mpeg_attenuation((audiostate.mixer_state.volume_left + 1) >> 1, (audiostate.mixer_state.volume_right + 1) >> 1);
		break;

	case AUDIO_SET_STREAMTYPE:
		if ((!(arg & AVIA_AV_AUDIO_CAPS)) || (hweight32(arg) != 1))
			return -EINVAL;
		switch (arg) {
		case AUDIO_CAP_MP1:
		case AUDIO_CAP_MP2:
		case AUDIO_CAP_AC3:
			return avia_av_stream_type_set(AVIA_AV_STREAM_TYPE_PES, AVIA_AV_STREAM_TYPE_PES);
		default:
			break;
		}
		break;

	case AUDIO_SET_EXT_ID:
		return -EOPNOTSUPP;

	case AUDIO_SET_ATTRIBUTES:
		return -EOPNOTSUPP;

	case AUDIO_SET_KARAOKE:
		return -EOPNOTSUPP;

	default:
		return -ENOIOCTLCMD;
	}

	return 0;
}

static
ssize_t avia_av_napi_video_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	if (((file->f_flags & O_ACCMODE) == O_RDONLY) ||
		(videostate.stream_source != VIDEO_SOURCE_MEMORY))
			return -EPERM;

	return avia_gt_dmx_queue_write(AVIA_GT_DMX_QUEUE_VIDEO, buf, count, file->f_flags & O_NONBLOCK);
}

static
int avia_av_napi_video_open(struct inode *inode, struct file *file)
{
	return dvb_generic_open(inode, file);
}

static
int avia_av_napi_video_release(struct inode *inode, struct file *file)
{
	avia_av_napi_video_ioctl(inode, file, VIDEO_STOP, NULL);
	avia_av_napi_video_ioctl(inode, file, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_DEMUX);
	return dvb_generic_release(inode, file);
}

static
unsigned int avia_av_napi_video_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(file, &video_wait_queue, wait);

	if (videostate.play_state == VIDEO_PLAYING) {
		if (avia_gt_dmx_queue_nr_get_bytes_free(AVIA_GT_DMX_QUEUE_VIDEO) >= 188)
			mask = (POLLOUT | POLLWRNORM);
	}
	else {
		mask = (POLLOUT | POLLWRNORM);
	}

	return mask;
}

static
ssize_t avia_av_napi_audio_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	size_t i = 0;

	if (((file->f_flags & O_ACCMODE) == O_RDONLY) ||
		(audiostate.stream_source != AUDIO_SOURCE_MEMORY))
			return -EPERM;

	if (need_audio_pts) {
		while (i < count - sizeof(struct pes_header)) {
			if (avia_av_audio_pts_to_stc((struct pes_header *)&buf[i]) == 0) {
				need_audio_pts = 0;
				break;
			}
			i++;
		}
	}

	return avia_gt_dmx_queue_write(AVIA_GT_DMX_QUEUE_AUDIO, &buf[i], count - i, file->f_flags & O_NONBLOCK);
}

static
int avia_av_napi_audio_open(struct inode *inode, struct file *file)
{
	return dvb_generic_open(inode, file);
}

static
int avia_av_napi_audio_release(struct inode *inode, struct file *file)
{
	avia_av_napi_audio_ioctl(inode, file, AUDIO_STOP, NULL);
	avia_av_napi_audio_ioctl(inode, file, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_DEMUX);
	return dvb_generic_release(inode, file);
}

static
unsigned int avia_av_napi_audio_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(file, &audio_wait_queue, wait);

	if (audiostate.play_state == AUDIO_PLAYING) {
		if (avia_gt_dmx_queue_nr_get_bytes_free(AVIA_GT_DMX_QUEUE_AUDIO) >= 188)
			mask = (POLLOUT | POLLWRNORM);
	}
	else {
		mask = (POLLOUT | POLLWRNORM);
	}

	return mask;
}

static struct file_operations avia_av_napi_video_fops = {
	.owner = THIS_MODULE,
	.write = avia_av_napi_video_write,
	.ioctl = dvb_generic_ioctl,
	.open = avia_av_napi_video_open,
	.release = avia_av_napi_video_release,
	.poll = avia_av_napi_video_poll,
};

static struct dvb_device avia_av_napi_video_dev = {
	.priv = NULL,
	.users = ~0,
	.readers = ~0,
	.writers = 1,
	.fops = &avia_av_napi_video_fops,
	.kernel_ioctl = avia_av_napi_video_ioctl,
};

static struct file_operations avia_av_napi_audio_fops = {
	.owner = THIS_MODULE,
	.write = avia_av_napi_audio_write,
	.ioctl = dvb_generic_ioctl,
	.open = avia_av_napi_audio_open,
	.release = avia_av_napi_audio_release,
	.poll = avia_av_napi_audio_poll,
};

static struct dvb_device avia_av_napi_audio_dev = {
	.priv = NULL,
	.users = ~0,
	.readers = ~0,
	.writers = 1,
	.fops = &avia_av_napi_audio_fops,
	.kernel_ioctl = avia_av_napi_audio_ioctl,
};

int __init avia_av_napi_init(void)
{
	int result;

	printk(KERN_INFO "%s: $Id: avia_av_napi.c,v 1.24 2003/09/11 23:22:48 obi Exp $\n", __FILE__);

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

	init_waitqueue_head(&audio_wait_queue);
	init_waitqueue_head(&video_wait_queue);

	if ((result = dvb_register_device(avia_napi_get_adapter(), &video_dev, &avia_av_napi_video_dev, NULL, DVB_DEVICE_VIDEO)) < 0) {
		printk(KERN_ERR "%s: dvb_register_device (video) failed (errno = %d)\n", __FILE__, result);
		return result;
	}

	if ((result = dvb_register_device(avia_napi_get_adapter(), &audio_dev, &avia_av_napi_audio_dev, NULL, DVB_DEVICE_AUDIO)) < 0) {
		printk(KERN_ERR "%s: dvb_register_device (audio) failed (errno = %d)\n", __FILE__, result);
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

