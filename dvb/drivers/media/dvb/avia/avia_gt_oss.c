/*
 * $Id: avia_gt_oss.c,v 1.22 2003/08/01 17:31:22 obi Exp $
 *
 * AViA eNX/GTX oss driver (dbox-II-project)
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/sound.h>
#include <linux/soundcard.h>

#include "avia_av.h"
#include "avia_gt_pcm.h"

static int avia_oss_dsp_dev = 0;
static int avia_oss_mixer_dev = 0;

static
int avia_oss_dsp_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int val = 0, retval = 0;

	switch (cmd) {
	case OSS_GETVERSION:
		dprintk("avia_oss: IOCTL: OSS_GETVERSION\n");
		return put_user(SOUND_VERSION, (int *)arg);

	case SNDCTL_DSP_CHANNELS:
		if (get_user(val, (int *)arg))
			return -EFAULT;

		dprintk("avia_oss: IOCTL: SNDCTL_DSP_CHANNELS (arg=%d)\n", val);
		return avia_gt_pcm_set_channels(val);

	case SNDCTL_DSP_GETBLKSIZE:
		dprintk("avia_oss: IOCTL: SNDCTL_DSP_GETBLKSIZE\n");
		val = avia_gt_pcm_get_block_size();
		return put_user(val, (int *)arg);

	case SNDCTL_DSP_GETFMTS:
		dprintk("avia_oss: IOCTL: SNDCTL_DSP_GETFMTS\n");
		return put_user(AFMT_U8 | AFMT_S8 | AFMT_S16_BE | AFMT_S16_LE | AFMT_U16_BE | AFMT_U16_LE, (int *)arg);

	case SNDCTL_DSP_MAPINBUF:
		dprintk("avia_oss: IOCTL: SNDCTL_DSP_MAPINBUF\n");
		return -1;

	case SNDCTL_DSP_NONBLOCK:
		dprintk("avia_oss: IOCTL: SNDCTL_DSP_NONBLOCK\n");
		file->f_flags |= O_NONBLOCK;
		break;

	case SNDCTL_DSP_RESET:
		dprintk("avia_oss: IOCTL: SNDCTL_DSP_RESET\n");
		break;

	case SNDCTL_SEQ_GETTIME:
		printk("avia_oss: IOCTL: SNDCTL_SEQ_GETTIME\n");
		return -1;

	case SNDCTL_DSP_SETFMT:
		if (get_user(val, (int *)arg))
			return -EFAULT;

		dprintk("avia_oss: IOCTL: SNDCTL_DSP_SETFMT (arg=%d)\n", val);

		switch (val) {
		case AFMT_U8:
			if ((retval = avia_gt_pcm_set_width(8)) < 0)
				return retval;
			return avia_gt_pcm_set_signed(0);

		case AFMT_S8:
			if ((retval = avia_gt_pcm_set_width(8)) < 0)
				return retval;
			return avia_gt_pcm_set_signed(1);

		case AFMT_S16_BE:
			avia_gt_pcm_set_width(16);
			avia_gt_pcm_set_signed(1);
			avia_gt_pcm_set_endian(1);
			break;

		case AFMT_S16_LE:
			avia_gt_pcm_set_width(16);
			avia_gt_pcm_set_signed(1);
			avia_gt_pcm_set_endian(0);
			break;

		case AFMT_U16_BE:
			avia_gt_pcm_set_width(16);
			avia_gt_pcm_set_signed(0);
			avia_gt_pcm_set_endian(1);
			break;

		case AFMT_U16_LE:
			avia_gt_pcm_set_width(16);
			avia_gt_pcm_set_signed(0);
			avia_gt_pcm_set_endian(0);
			break;

		default:
			dprintk("avia_oss: IOCTL: SNDCTL_DSP_SETFMT unkown fmt (arg=%d)\n", val);
			return -EINVAL;
		}

		break;

	case SNDCTL_DSP_SPEED:
		if (get_user(val, (int *)arg))
			return -EFAULT;

		printk("avia_oss: IOCTL: SNDCTL_DSP_SPEED (arg=%d)\n", val);
		
		if ((val != 48000) && (val != 24000) && (val != 12000) && (avia_av_get_sample_rate() != 44100)) {
			avia_av_standby(1);
			avia_av_standby(0);
		}

		return avia_gt_pcm_set_rate(val);

	case SNDCTL_DSP_STEREO:
		if (get_user(val, (int *)arg))
			return -EFAULT;

		dprintk("avia_oss: IOCTL: SNDCTL_DSP_STEREO (arg=%d)\n", val);
		return avia_gt_pcm_set_channels(val + 1);

	case SNDCTL_DSP_SYNC:
		dprintk("avia_oss: IOCTL: SNDCTL_DSP_SYNC\n");
		break;

	case SOUND_PCM_WRITE_FILTER:
		printk("IOC: 1\n");
		return -1;

	case SNDCTL_DSP_POST:
		printk("IOC: 2\n");
		return -1;

	case SNDCTL_DSP_SUBDIVIDE:
		printk("IOC: 3\n");
		return -1;

	case SNDCTL_DSP_SETFRAGMENT:
		printk("IOC: 7\n");
		return -1;

	case SNDCTL_DSP_GETOSPACE:
		printk("IOC: 8\n");
		return -1;

	case SNDCTL_DSP_GETISPACE:
		printk("IOC: 9\n");
		return -1;

	case SNDCTL_DSP_GETCAPS:
		printk("IOC: 10\n");
		return -1;

	case SNDCTL_DSP_GETTRIGGER:
		printk("IOC: 11\n");
		return -1;

	case SNDCTL_DSP_SETTRIGGER:
		printk("IOC: 12\n");
		return -1;

	case SNDCTL_DSP_GETIPTR:
		printk("IOC: 13\n");
		return -1;

	case SNDCTL_DSP_GETOPTR:
		printk("IOC: 14\n");
		return -1;

	case SNDCTL_DSP_MAPOUTBUF:
		printk("IOC: 15\n");
		return -1;

	case SNDCTL_DSP_SETSYNCRO:
		printk("IOC: 16\n");
		return -1;

	case SNDCTL_DSP_SETDUPLEX:
		printk("IOC: 17\n");
		return -1;

	case SNDCTL_DSP_GETODELAY:
		printk("IOC: 18\n");
		return -1;

	case SNDCTL_DSP_GETCHANNELMASK:
		printk("IOC: 19\n");
		return -1;

	case SNDCTL_DSP_BIND_CHANNEL:
		printk("IOC: 20\n");
		return -1;

	case SNDCTL_DSP_PROFILE:
		printk("IOC: 21\n");
		return -1;

	default:
		printk("avia_oss: IOCTL: unknown (cmd=%d)\n", cmd);
		return -EINVAL;
	}

	return 0;
}

static
ssize_t avia_oss_dsp_write(struct file *file, const char *buf, size_t count, loff_t *offset)
{
	int result;

	dprintk("avia_oss: dsp write (buffer=0x%X, count=%d)\n", (unsigned int)buf, count);

	result = avia_gt_pcm_play_buffer((void *)buf, count, !(file->f_flags & O_NONBLOCK));

	if ((result > 0) && (offset))
		*offset += result;

	return result;
}

static
unsigned int avia_oss_dsp_poll(struct file *file, struct poll_table_struct *wait)
{
	return avia_gt_pcm_poll(file, wait);
}

static struct file_operations dsp_fops = {
	.ioctl = avia_oss_dsp_ioctl,
	.owner = THIS_MODULE,
	.poll = avia_oss_dsp_poll,
	.write = avia_oss_dsp_write,
};

static struct file_operations mixer_fops = {
	.owner = THIS_MODULE,
//	.ioctl = avia_oss_mixer_ioctl,
};

static
int __init avia_oss_init(void)
{
	printk(KERN_INFO "avia_oss: $Id: avia_gt_oss.c,v 1.22 2003/08/01 17:31:22 obi Exp $\n");

	avia_gt_pcm_set_pcm_attenuation(0x70, 0x70);

	avia_gt_pcm_set_rate(44100);
	avia_gt_pcm_set_width(16);
	avia_gt_pcm_set_channels(2);
	avia_gt_pcm_set_signed(1);
	avia_gt_pcm_set_endian(0);

	avia_oss_dsp_dev = register_sound_dsp(&dsp_fops, -1);
	avia_oss_mixer_dev = register_sound_mixer(&mixer_fops, -1);

	return 0;
}

static
void __exit avia_oss_cleanup(void)
{
	unregister_sound_mixer(avia_oss_mixer_dev);
	unregister_sound_dsp(avia_oss_dsp_dev);

	avia_gt_pcm_set_pcm_attenuation(0x00, 0x00);
	avia_gt_pcm_stop();
}

module_init(avia_oss_init);
module_exit(avia_oss_cleanup);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("AViA eNX/GTX OSS driver");
MODULE_LICENSE("GPL");

