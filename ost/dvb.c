/*
 * dvb.c
 *
 * Copyright (C) 2000-2002 tmbinc, gillem, obi
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * $Id: dvb.c,v 1.69 2002/05/08 13:28:42 obi Exp $
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <asm/uaccess.h>
#include <asm/system.h>

#include <linux/kmod.h>

#include <ost/audio.h>
#include <ost/ca.h>
#include <ost/demux.h>
#include <ost/dmx.h>
#include <ost/frontend.h>
#include <ost/sec.h>
#include <ost/video.h>
#include <ost/net.h>

#include <dbox/dvb.h>
#include <dbox/ves.h>
#include <dbox/avia.h>
#include <dbox/avia_gt_pcm.h>
#include <dbox/cam.h>

#include "dvbdev.h"
#include "dmxdev.h"
#include "dvb_net.h"
#include <dbox/dvb_frontend.h>

#define AUDIO_MIXER_MAX_VOLUME	64

typedef struct dvb_struct
{
	dmxdev_t		dmxdev;
	dvb_device_t		dvb_dev;
	dvb_front_t *		frontend;
	struct audioMixer	audiomixer;
	struct audioStatus	audiostate;
	struct videoStatus	videostate;
	FrontendPowerState	powerstate;
	struct secStatus	sec;
	int			num;
	dvb_net_t *		dvb_net;

} dvb_struct_t;

static dvb_struct_t dvb;

static int secSetTone (struct dvb_struct * dvb, secToneMode mode)
{
	if (!dvb->frontend)
	{
		printk("dvb: no frontend available\n");
		return -ENXIO;
	}

	dvb->sec.contTone = mode;
	dvb_frontend_sec_set_tone(dvb->frontend, mode);
	return 0;
}

static int secSetVoltage (struct dvb_struct * dvb, secVoltage voltage)
{
	if (!dvb->frontend)
	{
		printk("dvb: no frontend available\n");
		return -ENXIO;
	}

	dvb->sec.selVolt = voltage;
	dvb_frontend_sec_set_voltage(dvb->frontend, voltage);
	return 0;
}

static int secSendSequence (struct dvb_struct * dvb, struct secCmdSequence * seq)
{
	int i;
	int ret;
	struct secCommand scommands;

	if (!dvb->frontend)
	{
		printk("dvb: no frontend available\n");
		return -ENXIO;
	}

	switch (seq->miniCommand)
	{
		case SEC_MINI_NONE:
		case SEC_MINI_A:
		case SEC_MINI_B:
		{
			break;
		}
		default:
		{
			return -EINVAL;
		}
	}

	for (i = 0; i < seq->numCommands; i++)
	{
		if (copy_from_user(&scommands, &seq->commands[i], sizeof(struct secCommand)))
		{
			continue;
		}

		dvb_frontend_sec_command(dvb->frontend, &scommands);
	}

	if (seq->miniCommand != SEC_MINI_NONE)
	{
		dvb_frontend_sec_mini_command(dvb->frontend, seq->miniCommand);
	}

	ret = secSetVoltage(dvb, seq->voltage);

	if (ret < 0)
	{
		printk("dvb: secSetVoltage failed\n");
		return ret;
	}

	ret = secSetTone(dvb, seq->continuousTone);

	if (ret < 0)
	{
		printk("dvb: secSetTone failed\n");
		return ret;
	}

	return 0;
}

void tuning_complete_cb (void * priv)
{
	// FIXME
	// struct dvb_struct * dvb = (struct dvb_struct *) priv;
}

static int frontend_init (dvb_struct_t * dvb)
{
	FrontendParameters para;

	dvb->frontend->priv = (void *) dvb;
	dvb->frontend->complete_cb = tuning_complete_cb;
	dvb_frontend_init(dvb->frontend);

	dvb->powerstate = FE_POWER_ON;

	switch (dvb->frontend->type)
	{
	case DVB_S:
		para.Frequency = 12480000 - 10600000;
		para.u.qpsk.SymbolRate = 27500000;
		para.u.qpsk.FEC_inner = 0;
		secSetTone(dvb, SEC_TONE_ON);
		secSetVoltage(dvb, SEC_VOLTAGE_13);
		break;

	case DVB_C:
		para.Frequency = 394000000;
		para.u.qam.SymbolRate = 6900000;
		para.u.qam.FEC_inner = 0;
		para.u.qam.QAM = QAM_64;
		break;
	}

	return 0;
}

static int demod_command (struct dvb_struct * dvb, unsigned int cmd, void * arg)
{
	if (!dvb->frontend->demod)
	{
		printk("dvb: no demod available\n");
		return -ENXIO;
	}

	return dvb->frontend->demod->driver->command(dvb->frontend->demod, cmd, arg);
}

int dvb_open (struct dvb_device * dvbdev, int type, struct inode * inode, struct file * file)
{
	struct dvb_struct * dvb = (struct dvb_struct *) dvbdev->priv;

	switch (type)
	{
		case DVB_DEVICE_VIDEO:
		{
			return 0;
		}
		case DVB_DEVICE_AUDIO:
		{
			return 0;
		}
		case DVB_DEVICE_SEC:
		{
			if (!dvb->frontend)
			{
				printk("dvb.c: no frontend available\n");
				return -ENXIO;
			}

			if (file->f_flags&O_NONBLOCK)
			{
				return -EWOULDBLOCK;
			}

			return 0;
		}
		case DVB_DEVICE_FRONTEND:
		{
			if (!dvb->frontend)
			{
				printk("dvb.c: no frontend available\n");
				return -ENXIO;
			}

			return 0;
		}
		case DVB_DEVICE_DEMUX:
		{
			if (!dvb->dmxdev.demux)
			{
				printk("dvb.c: no demux available\n");
				return -ENXIO;
			}

			return DmxDevFilterAlloc(&dvb->dmxdev, file);
		}
		case DVB_DEVICE_DVR:
		{
			if (!dvb->dmxdev.demux)
			{
				printk("dvb.c: no demux available\n");
				return -ENXIO;
			}

			return DmxDevDVROpen(&dvb->dmxdev, file);
		}
		case DVB_DEVICE_CA:
		{
			return 0;
		}
		case DVB_DEVICE_NET:
		{
			return 0;
		}
		case DVB_DEVICE_OSD:
		{
			return 0;
		}
		default:
		{
			return -ENXIO;
		}
	}
}

int dvb_close (struct dvb_device * dvbdev, int type, struct inode * inode, struct file * file)
{
	struct dvb_struct * dvb = (struct dvb_struct *) dvbdev->priv;

	switch (type)
	{
		case DVB_DEVICE_DEMUX:
		{
			return DmxDevFilterFree(&dvb->dmxdev, file);
		}
		case DVB_DEVICE_SEC:
		{
			return 0;
		}
		case DVB_DEVICE_VIDEO:
		{
			return 0;
		}
		case DVB_DEVICE_AUDIO:
		{
			return 0;
		}
		case DVB_DEVICE_FRONTEND:
		{
			return 0;
		}
		case DVB_DEVICE_DVR:
		{
			return DmxDevDVRClose(&dvb->dmxdev, file);
		}
		case DVB_DEVICE_CA:
		{
			return 0;
		}
		case DVB_DEVICE_NET:
		{
			return 0;
		}
		case DVB_DEVICE_OSD:
		{
			return 0;
		}
		default:
		{
			return -EIO;
		}
	}
}

ssize_t dvb_read (struct dvb_device * dvbdev, int type, struct file * file, char * buf, size_t count, loff_t * ppos)
{
	struct dvb_struct * dvb = (struct dvb_struct *) dvbdev->priv;

	switch (type)
	{
		case DVB_DEVICE_DEMUX:
		{
			return DmxDevRead(&dvb->dmxdev, file, buf, count, ppos);
		}
		case DVB_DEVICE_DVR:
		{
			return DmxDevDVRRead(&dvb->dmxdev, file, buf, count, ppos);
		}
		default:
		{
			return -EINVAL;
		}
	}
}

ssize_t dvb_write (struct dvb_device * dvbdev, int type, struct file * file, const char * buf, size_t count, loff_t * ppos)
{
	struct dvb_struct * dvb = (struct dvb_struct *) dvbdev->priv;

	switch (type)
	{
		case DVB_DEVICE_VIDEO:
		{
			if (dvb->videostate.streamSource != VIDEO_SOURCE_MEMORY)
			{
				return -EPERM;
			}

			return -EINVAL;
		}
		case DVB_DEVICE_AUDIO:
		{
			if (dvb->audiostate.streamSource != AUDIO_SOURCE_MEMORY)
			{
				return -EPERM;
			}

			return -EINVAL;
		}
		case DVB_DEVICE_DVR:
		{
			return DmxDevDVRWrite(&dvb->dmxdev, file, buf, count, ppos);
		}
		default:
		{
			return -EINVAL;
		}
	}
}

int dvb_ioctl (struct dvb_device * dvbdev, int type, struct file * file, unsigned int cmd, unsigned long arg)
{
	struct dvb_struct * dvb = (struct dvb_struct *) dvbdev->priv;
	void * parg = (void *) arg;

	switch (type)
	{
		case DVB_DEVICE_VIDEO:
		{
			if (((file->f_flags & O_ACCMODE) == O_RDONLY) && (cmd != VIDEO_GET_STATUS))
			{
				return -EPERM;
			}

			switch (cmd)
			{
				case VIDEO_STOP:
				{
					dvb->videostate.playState = VIDEO_STOPPED;
					avia_command(NewChannel, 0, 0xFFFF, 0xFFFF);
					break;
				}
				case VIDEO_PLAY:
				{
					dvb->videostate.playState = VIDEO_PLAYING;
					avia_command(NewChannel, 0, 0xffff, 0xffff);
					avia_wait(avia_command(SelectStream, 0, 0));
					avia_wait(avia_command(SelectStream, 3, 0));
					avia_command(NewChannel, 0, 0xffff, 0xffff);
					avia_wait(avia_command(SelectStream, 0, 0));
					avia_wait(avia_command(SelectStream, 3, 0));
					break;
				}
				case VIDEO_FREEZE:
				{
					dvb->videostate.playState = VIDEO_FREEZED;
					avia_wait(avia_command(Freeze, 1));
					break;
				}
				case VIDEO_CONTINUE:
				{
					if (dvb->videostate.playState == VIDEO_FREEZED)
					{
						dvb->videostate.playState=VIDEO_PLAYING;
						avia_wait(avia_command(Resume));
					}
					break;
				}
				case VIDEO_SELECT_SOURCE:
				{
					if (dvb->videostate.playState == VIDEO_STOPPED)
					{
						dvb->videostate.streamSource = (videoStreamSource_t) arg;

						if (dvb->videostate.streamSource != VIDEO_SOURCE_DEMUX)
						{
							return -EINVAL;
						}

						avia_flush_pcr();

						if (dvb->dmxdev.demux)
						{
							dvb->dmxdev.demux->flush_pcr();
						}

					}
					else
					{
						return -EINVAL;
					}
					break;
				}
				case VIDEO_SET_BLANK:
				{
					dvb->videostate.videoBlank = (boolean) arg;
					break;
				}
				case VIDEO_GET_STATUS:
				{
					if (copy_to_user(parg, &dvb->videostate, sizeof(struct videoStatus)))
					{
						return -EFAULT;
					}
					break;
				}
				case VIDEO_GET_EVENT:
				{
					return -ENOTSUPP;
				}
				case VIDEO_SET_DISPLAY_FORMAT:
				{
					switch ((videoDisplayFormat_t) arg)
					{
						case VIDEO_PAN_SCAN:
						{
							dprintk("SWITCH PAN SCAN\n");
							dvb->videostate.displayFormat = VIDEO_PAN_SCAN;
							wDR(ASPECT_RATIO_MODE, 1);
							break;
						}
						case VIDEO_LETTER_BOX:
						{
							dprintk("SWITCH LETTER BOX\n");
							dvb->videostate.displayFormat = VIDEO_LETTER_BOX;
							wDR(ASPECT_RATIO_MODE, 2);
							break;
						}
						case VIDEO_CENTER_CUT_OUT:
						{
							dprintk("SWITCH CENTER CUT OUT\n");
							dvb->videostate.displayFormat = VIDEO_CENTER_CUT_OUT;
							wDR(ASPECT_RATIO_MODE, 0);
							break;
						}
						default:
						{
							return -ENOTSUPP;
						}
					}
					break;
				}
				case VIDEO_STILLPICTURE:
				{
					return -ENOTSUPP;
				}
				case VIDEO_FAST_FORWARD:
				{
					return -ENOTSUPP;
				}
				case VIDEO_SLOWMOTION:
				{
					return -ENOTSUPP;
				}
				case VIDEO_GET_CAPABILITIES:
				{
					return -ENOTSUPP;
				}
				case VIDEO_CLEAR_BUFFER:
				{
					return -ENOTSUPP;
				}
				case VIDEO_SET_ID:
				{
					return -ENOTSUPP;
				}
				case VIDEO_SET_STREAMTYPE:
				{
					return -ENOTSUPP;
				}
				case VIDEO_SET_FORMAT:
				{
					dvb->videostate.videoFormat = (videoFormat_t) arg;

					switch (dvb->videostate.videoFormat)
					{
						case VIDEO_FORMAT_AUTO:
						{
							dprintk("dvb: VIDEO_FORMAT_AUTO\n");
							wDR(FORCE_CODED_ASPECT_RATIO, 0);
							break;
						}
						case VIDEO_FORMAT_4_3:
						{
							dprintk("dvb: VIDEO_FORMAT_4_3\n");
							wDR(FORCE_CODED_ASPECT_RATIO, 2);
							break;
						}
						case VIDEO_FORMAT_16_9:
						{
							dprintk("dvb: VIDEO_FORMAT_16_9\n");
							wDR(FORCE_CODED_ASPECT_RATIO, 3);
							break;
						}
						case VIDEO_FORMAT_20_9:
						{
							dprintk("dvb: VIDEO_FORMAT_20_9\n");
							wDR(FORCE_CODED_ASPECT_RATIO, 4);
							break;
						}
						default:
						{
							return -EINVAL;
						}
					}
					break;
				}
				case VIDEO_SET_SYSTEM:
				{
					return -ENOTSUPP;
				}
				case VIDEO_SET_HIGHLIGHT:
				{
					return -ENOTSUPP;
				}
				case VIDEO_SET_SPU:
				{
					return -ENOTSUPP;
				}
				case VIDEO_SET_SPU_Palette:
				{
					return -ENOTSUPP;
				}
				case VIDEO_GET_NAVI:
				{
					return -ENOTSUPP;
				}
				case VIDEO_SET_ATTRIBUTES:
				{
					return -ENOTSUPP;
				}
				case VIDEO_DIGEST:
				{
					/* TODO: check values */
					avia_wait(avia_command(Digest,
							((struct videoDigest *) parg)->x,
							((struct videoDigest *) parg)->y,
							((struct videoDigest *) parg)->skip,
							((struct videoDigest *) parg)->decimation,
							((struct videoDigest *) parg)->threshold,
							((struct videoDigest *) parg)->pictureID));
					break;
				}
				default:
				{
					return -ENOIOCTLCMD;
				}
			}
			break;
		}
		case DVB_DEVICE_AUDIO:
		{
			if (((file->f_flags & O_ACCMODE) == O_RDONLY) && (cmd != AUDIO_GET_STATUS))
			{
				return -EPERM;
			}

			switch (cmd)
			{
				case AUDIO_STOP:
				{
					dvb->audiostate.playState = AUDIO_STOPPED;
					break;
				}
				case AUDIO_PLAY:
				{
					dvb->audiostate.playState = AUDIO_PLAYING;
					break;
				}
				case AUDIO_PAUSE:
				{
					dvb->audiostate.playState = AUDIO_PAUSED;
					break;
				}
				case AUDIO_CONTINUE:
				{
					if (dvb->audiostate.playState == AUDIO_PAUSED)
					{
						dvb->audiostate.playState = AUDIO_PLAYING;
					}
					break;
				}
				case AUDIO_SELECT_SOURCE:
				{
					dvb->audiostate.streamSource = (audioStreamSource_t) arg;
					break;
				}
				case AUDIO_SET_MUTE:
				{
					dvb->audiostate.muteState = (boolean) arg;

					if (dvb->audiostate.muteState)
					{
						/* mute spdif (2) and analog audio (4) */
						wDR(AUDIO_CONFIG, rDR(AUDIO_CONFIG) & ~6);
						/* mute pcm */
						avia_gt_pcm_set_pcm_attenuation(0x00, 0x00);
					}
					else
					{
						/* unmute spdif (2) and analog audio (4) */
						wDR(AUDIO_CONFIG, rDR(AUDIO_CONFIG) | 6);
						/* unmute pcm */
						avia_gt_pcm_set_pcm_attenuation(dvb->audiomixer.volume_left, dvb->audiomixer.volume_right);
					}

					wDR(NEW_AUDIO_CONFIG, 1);
					break;
				}
				case AUDIO_SET_AV_SYNC:
				{
					dvb->audiostate.AVSyncState = (boolean) arg;

					if (dvb->audiostate.AVSyncState)
					{
						wDR(AV_SYNC_MODE, 6);
					}
					else
					{
						wDR(AV_SYNC_MODE, 0);
					}
					break;
				}
				case AUDIO_SET_BYPASS_MODE:
				{
					dvb->audiostate.bypassMode = (boolean) arg;

					if (dvb->audiostate.bypassMode)
					{
						avia_command(SelectStream, 2, 0x1FFF);
						avia_command(SelectStream, 3, 0);
						wDR(AUDIO_CONFIG, rDR(AUDIO_CONFIG) | 1);
					}
					else
					{
						avia_command(SelectStream, 3, 0x1FFF);
						avia_command(SelectStream, 2, 0);
						wDR(AUDIO_CONFIG, rDR(AUDIO_CONFIG) & ~1);
					}

					wDR(NEW_AUDIO_CONFIG, 1);
					break;
				}
				case AUDIO_CHANNEL_SELECT:
				{
					dvb->audiostate.channelSelect = (audioChannelSelect_t) arg;

					switch (dvb->audiostate.channelSelect)
					{
						case AUDIO_STEREO:
						{
							wDR(AUDIO_DAC_MODE, rDR(AUDIO_DAC_MODE) & ~0x30);
							break;
						}
						case AUDIO_MONO_LEFT:
						{
							wDR(AUDIO_DAC_MODE, (rDR(AUDIO_DAC_MODE) & ~0x30) | 0x10);
							break;
						}
						case AUDIO_MONO_RIGHT:
						{
							wDR(AUDIO_DAC_MODE, (rDR(AUDIO_DAC_MODE) & ~0x30) | 0x20);
							break;
						}
						default:
						{
							return -EINVAL;
						}
					}

					wDR(NEW_AUDIO_CONFIG, 1);
					break;
				}
				case AUDIO_GET_STATUS:
				{
					if (copy_to_user(parg, &dvb->audiostate, sizeof(struct audioStatus)))
					{
						return -EFAULT;
					}
					break;
				}
				case AUDIO_GET_CAPABILITIES:
				{
					int32_t cap = AUDIO_CAP_LPCM | AUDIO_CAP_MP1 | AUDIO_CAP_MP2 | AUDIO_CAP_AC3;

					if (copy_to_user(parg, &cap, sizeof(cap)))
					{
						return -EFAULT;
					}
					break;
				}
				case AUDIO_CLEAR_BUFFER:
				{
					return -ENOTSUPP;
				}
				case AUDIO_SET_ID:
				{
					return -ENOTSUPP;
				}
				case AUDIO_SET_MIXER:
				{
					if (copy_from_user(&dvb->audiomixer, parg, sizeof(struct audioMixer)))
					{
						return -EFAULT;
					}

					/*
					 * the maximum value is not defined in the api,
					 * so now it is defined as 64, which fits for
					 * the avia chipset
					 */

					if (dvb->audiomixer.volume_left > AUDIO_MIXER_MAX_VOLUME)
					{
						dvb->audiomixer.volume_left = AUDIO_MIXER_MAX_VOLUME;
					}
					if (dvb->audiomixer.volume_right > AUDIO_MIXER_MAX_VOLUME)
					{
						dvb->audiomixer.volume_right = AUDIO_MIXER_MAX_VOLUME;
					}

					/* set mpeg volume */
					avia_gt_pcm_set_mpeg_attenuation(dvb->audiomixer.volume_left, dvb->audiomixer.volume_right);

					/* set pcm volume */
					avia_gt_pcm_set_pcm_attenuation(dvb->audiomixer.volume_left, dvb->audiomixer.volume_right);
					break;

				}
				case AUDIO_SET_STREAMTYPE:
				{
					return -ENOTSUPP;
				}
				case AUDIO_SET_EXT_ID:
				{
					return -ENOTSUPP;
				}
				case AUDIO_SET_ATTRIBUTES:
				{
					return -ENOTSUPP;
				}
				case AUDIO_SET_KARAOKE:
				{
					return -ENOTSUPP;
				}
				case AUDIO_SET_SPDIF_COPIES:
				{
					switch ((audioSpdifCopyState_t) arg)
					{
						case SCMS_COPIES_NONE:
						{
							wDR(IEC_958_CHANNEL_STATUS_BITS, (rDR(IEC_958_CHANNEL_STATUS_BITS) & ~1) | 4);
							break;
						}
						case SCMS_COPIES_ONE:
						{
							wDR(IEC_958_CHANNEL_STATUS_BITS, rDR(IEC_958_CHANNEL_STATUS_BITS) & ~5);
							break;
						}
						case SCMS_COPIES_UNLIMITED:
						{
							wDR(IEC_958_CHANNEL_STATUS_BITS, rDR(IEC_958_CHANNEL_STATUS_BITS) | 5);
							break;
						}
						default:
						{
							return -EINVAL;
						}
					}

					wDR(NEW_AUDIO_CONFIG, 1);
					break;
				}
				default:
				{
					return -ENOIOCTLCMD;
				}
			}
			break;
		}
		case DVB_DEVICE_SEC:
		{
			if (!dvb->frontend)
			{
				return -ENXIO;
			}

			if (file->f_flags & O_NONBLOCK)
			{
				return -EWOULDBLOCK;
			}

			if (((file->f_flags & O_ACCMODE) == O_RDONLY) && (cmd != SEC_GET_STATUS))
			{
				return -EPERM;
			}

			switch (cmd)
			{
				case SEC_GET_STATUS:
				{
					if (copy_to_user(parg, &dvb->sec, sizeof(dvb->sec)))
					{
						return -EFAULT;
					}
					break;
				}
				case SEC_RESET_OVERLOAD:
				{
					return -ENOTSUPP;
				}
				case SEC_SEND_SEQUENCE:
				{
					struct secCmdSequence seq;

					if (copy_from_user(&seq, parg, sizeof(seq)))
					{
						return -EFAULT;
					}

					dvb_frontend_stop(dvb->frontend);
					return secSendSequence(dvb, &seq);
				}
				case SEC_SET_TONE:
				{
					secToneMode mode = (secToneMode) arg;
					dvb_frontend_stop(dvb->frontend);
					return secSetTone(dvb, mode);
				}
				case SEC_SET_VOLTAGE:
				{
					secVoltage val = (secVoltage) arg;
					dvb_frontend_stop(dvb->frontend);
					return secSetVoltage(dvb, val);
				}
				default:
				{
					return -ENOIOCTLCMD;
				}
			}
			break;
		}
		case DVB_DEVICE_FRONTEND:
		{
			if (!dvb->frontend)
			{
				return -ENXIO;
			}

			switch (cmd)
			{
				case FE_SELFTEST:
				{
					return -ENOTSUPP;
				}
				case FE_SET_POWER_STATE:
				{
					dvb->powerstate = arg;
					secSetVoltage(dvb, (dvb->powerstate == FE_POWER_ON) ? dvb->sec.selVolt : SEC_VOLTAGE_LT);
					break;
				}
				case FE_POWER_SUSPEND:
				case FE_POWER_STANDBY:
				case FE_POWER_OFF:
				{
					dvb->powerstate = FE_POWER_OFF;
					secSetVoltage(dvb, SEC_VOLTAGE_LT);
					break;
				}
				case FE_GET_POWER_STATE:
				{
					if (copy_to_user(parg, &dvb->powerstate, sizeof(u32)))
					{
						return -EFAULT;
					}
					break;
				}
				case FE_READ_STATUS:
				{
					FrontendStatus stat = 0;

					demod_command(dvb, FE_READ_STATUS, &stat);

					if (dvb->powerstate == FE_POWER_ON)
					{
						stat |= FE_HAS_POWER;
					}

					if (copy_to_user(parg, &stat, sizeof(stat)))
					{
						return -EFAULT;
					}
					break;
				}
				case FE_READ_BER:
				{
					uint32_t ber;

					if (dvb->powerstate != FE_POWER_ON)
					{
						return -ENOSIGNAL;
					}

					demod_command(dvb, FE_READ_BER, &ber);

					if (copy_to_user(parg, &ber, sizeof(ber)))
					{
						return -EFAULT;
					}
					break;
				}
				case FE_READ_SIGNAL_STRENGTH:
				{
					int32_t signal;

					if (dvb->powerstate != FE_POWER_ON)
					{
						return -ENOSIGNAL;
					}

					demod_command(dvb, FE_READ_SIGNAL_STRENGTH, &signal);

					if (copy_to_user(parg, &signal, sizeof(signal)))
					{
						return -EFAULT;
					}
					break;
				}
				case FE_READ_SNR:
				{
					int32_t snr;

					if (dvb->powerstate != FE_POWER_ON)
					{
						return -ENOSIGNAL;
					}

					demod_command(dvb, FE_READ_SNR, &snr);

					if (copy_to_user(parg, &snr, sizeof(snr)))
					{
						return -EFAULT;
					}
					break;
				}
				case FE_READ_UNCORRECTED_BLOCKS:
				{
					u32 ublocks;

					if (dvb->powerstate != FE_POWER_ON)
					{
						return -ENOSIGNAL;
					}
					if (demod_command(dvb, FE_READ_UNCORRECTED_BLOCKS, &ublocks) < 0)
					{
						return -ENOSYS;
					}
					if (copy_to_user(parg, &ublocks, sizeof(ublocks)))
					{
						return -EFAULT;
					}
					break;
				}
				case FE_GET_NEXT_FREQUENCY:
				{
					uint32_t freq;

					if (copy_from_user(&freq, parg, sizeof(freq)))
					{
						return -EFAULT;
					}
					if (dvb->frontend->type == DVB_S)
					{
						// FIXME: how does one calculate this?
						freq += 1000; //FIXME: KHz like in QPSK_TUNE??
					}
					else
					{
						freq += 1000000;
					}
					if (copy_to_user(parg, &freq, sizeof(freq)))
					{
						return -EFAULT;
					}
					break;
				}
				case FE_GET_NEXT_SYMBOL_RATE:
				{
					uint32_t rate;

					if (copy_from_user(&rate, parg, sizeof(rate)))
					{
						return -EFAULT;
					}

					if (dvb->frontend->type == DVB_C)
					{
						if (rate < 1725000)
							rate = 1725000;
						else if (rate < 3450000)
							rate = 3450000;
						else if (rate < 5175000)
							rate = 5175000;
						else if (rate < 5500000)
							rate = 5500000;
						else if (rate < 6875000)
							rate = 6875000;
						else if (rate < 6900000)
							rate = 6900000;
						else
							return -EINVAL;
					}
					// FIXME: how does one really calculate this?
					else if (rate<5000000)
						rate+=500000;
					else if(rate<10000000)
						rate+=1000000;
					else if(rate<30000000)
						rate+=2000000;
					else
						return -EINVAL;

					if (copy_to_user(parg, &rate, sizeof(rate)))
					{
						return -EFAULT;
					}
					break;
				}
				case FE_GET_FRONTEND:
				{
					if (copy_to_user(parg, &dvb->frontend->param, sizeof(FrontendParameters)))
					{
						return -EFAULT;
					}
					break;
				}
				case FE_SET_FRONTEND:
				{
					FrontendParameters para;

					if ((file->f_flags & O_ACCMODE) == O_RDONLY)
					{
						return -EPERM;
					}
					if (copy_from_user(&para, parg, sizeof(para)))
					{
						return -EFAULT;
					}
					if ((dvb->frontend->type == DVB_S) && (para.u.qpsk.FEC_inner > FEC_NONE))
					{
						return -EINVAL;
					}
					else if ((dvb->frontend->type == DVB_C) && (para.u.qam.FEC_inner > FEC_NONE))
					{
						return -EINVAL;
					}

					if (para.Frequency > 20000000)		// if >20Ghz, divide by 1000
					{
						para.Frequency /= 1000;
					}

					dvb_frontend_tune(dvb->frontend, &para);
					break;
				}
				case FE_GET_EVENT:
				{
					FrontendEvent event;
					int ret;

					ret = dvb_frontend_get_event(dvb->frontend, &event, file->f_flags & O_NONBLOCK);

					if (ret < 0)
					{
						return ret;
					}

					if (copy_to_user(parg, &event, sizeof(event)))
					{
						return -EFAULT;
					}
					break;
				}
				case FE_GET_INFO:
				{
					FrontendInfo feinfo;

					switch (dvb->frontend->type)
					{
					case DVB_S:
						feinfo.type = FE_QPSK;
						feinfo.minFrequency = 500;  //KHz?
						feinfo.maxFrequency = 2700000;
						break;

					case DVB_C:
						feinfo.type = FE_QAM;
						feinfo.minFrequency = 40000000;
						feinfo.maxFrequency = 870000000;
						break;
					}

					feinfo.minSymbolRate = 500000;
					feinfo.maxSymbolRate = 30000000;
					feinfo.hwType = 0;    //??
					feinfo.hwVersion = 0;

					if (copy_to_user(parg, &feinfo, sizeof(feinfo)))
					{
						return -EFAULT;
					}
					break;
				}
				default:
				{
					return -ENOIOCTLCMD;
				}
			}
			break;

		}
		case DVB_DEVICE_DEMUX:
		{
			return DmxDevIoctl(&dvb->dmxdev, file, cmd, arg);
		}
		case DVB_DEVICE_CA:
		{
			switch (cmd)
			{
				ca_msg_t ca_msg;

				case CA_RESET:
				{
					return cam_reset();
				}
				case CA_GET_CAP:
				{
					return -ENOTSUPP;
				}
				case CA_GET_SLOT_INFO:
				{
					return -ENOTSUPP;
				}
				case CA_GET_DESCR_INFO:
				{
					return -ENOTSUPP;
				}
				case CA_GET_MSG:
				{
					ca_msg.index = 0;
					ca_msg.type  = 0;

					if (copy_from_user(&ca_msg, parg, sizeof(ca_msg_t)))
					{
						return -EFAULT;
					}

					((ca_msg_t*)parg)->length = cam_read_message(((ca_msg_t*)parg)->msg, ca_msg.length);
					break;
				}
				case CA_SEND_MSG:
				{
					if (copy_from_user(&ca_msg, parg, sizeof(ca_msg_t)))
					{
						return -EFAULT;
					}

					cam_write_message(ca_msg.msg, ca_msg.length);
					break;

				}
				case CA_SET_DESCR:
				{
					return -ENOTSUPP;
				}
				default:
				{
					return -ENOIOCTLCMD;
				}
			}
			break;
		}
		case DVB_DEVICE_NET:
		{
			if (!dvb->dvb_net)
			{
				return -ENXIO;
			}

			if (((file->f_flags & O_ACCMODE) == O_RDONLY))
			{
				return -EPERM;
			}

			switch (cmd)
			{
				case NET_ADD_IF:
				{
					struct dvb_net_if dvbnetif;
					int result;

					if (copy_from_user(&dvbnetif, parg, sizeof(dvbnetif)))
					{
						return -EFAULT;
					}

					result = dvb->dvb_net->dvb_net_add_if(dvb->dvb_net, dvbnetif.pid);

					if (result < 0)
					{
						return result;
					}

					dvbnetif.if_num = result;

					if (copy_to_user(parg, &dvbnetif, sizeof(dvbnetif)))
					{
						return -EFAULT;
					}
					break;
				}
				case NET_REMOVE_IF:
				{
					return dvb->dvb_net->dvb_net_remove_if(dvb->dvb_net, (int) arg);
				}
				default:
				{
					return -ENOIOCTLCMD;
				}
			}
			break;
		}
		case DVB_DEVICE_OSD:
		{
			return -ENOTSUPP;
		}
		default:
		{
			return -EIO;
		}
	}

	return 0;
}

unsigned int dvb_poll (struct dvb_device * dvbdev, int type, struct file * file, poll_table * wait)
{
	struct dvb_struct * dvb = (struct dvb_struct *) dvbdev->priv;

	switch (type)
	{
		case DVB_DEVICE_FRONTEND:
		{
			return dvb_frontend_poll(dvb->frontend, file, wait);
		}
		case DVB_DEVICE_DEMUX:
		{
			return DmxDevPoll(&dvb->dmxdev, file, wait);
		}
		default:
		{
			return -ENOTSUPP;
		}
	}
}

/* device register */

static int dvb_register (struct dvb_struct * dvb)
{
	int result;
	struct dvb_device *dvbd = &dvb->dvb_dev;

	dvb->num = 0;
	dvb->dmxdev.demux = 0;
	dvb->dvb_net = 0;

	dvbd->priv = (void *) dvb;
	dvbd->open = dvb_open;
	dvbd->close = dvb_close;
	dvbd->read = dvb_read;
	dvbd->write = dvb_write;
	dvbd->ioctl = dvb_ioctl;
	dvbd->poll = dvb_poll;

	dprintk("registering device.\n");

	result = dvb_register_device(dvbd);

	if (result < 0)
	{
		printk("dvb: yes we can't remove all this ...\n");
		return result;
	}

	return 0;
}

void dvb_unregister (struct dvb_struct * dvb)
{
	return dvb_unregister_device(&dvb->dvb_dev);
}

int register_frontend (dvb_front_t * frontend)
{
	if (!dvb.frontend)
	{
		dvb.frontend = frontend;
		dprintk("registering frontend...\n");
#ifdef MODULE
		MOD_INC_USE_COUNT;
#endif
		return frontend_init(&dvb);
	}

	return -EEXIST;
}

int unregister_frontend (dvb_front_t * frontend)
{
	if (dvb.frontend == frontend)
	{
		dvb_frontend_exit(frontend);
		dvb.frontend = 0;
#ifdef MODULE
		MOD_DEC_USE_COUNT;
#endif
		return 0;
	}

	return -ENXIO;
}

int register_dvbnet (struct dvb_net_s * dvbnet)
{
	if (!dvb.dmxdev.demux)
	{
		return -ENXIO;
	}

	if (!dvb.dvb_net)
	{
		dvb.dvb_net = dvbnet;

#ifdef MODULE
		MOD_INC_USE_COUNT;
#endif
		/* now register net-device with demux */
		dvb.dvb_net->card_num = dvb.num;
		return dvb.dvb_net->dvb_net_init(dvb.dvb_net, dvb.dmxdev.demux);
	}

	return -EEXIST;
}

int unregister_dvbnet (dvb_net_t * dvbnet)
{
	if (dvb.dvb_net == dvbnet)
	{
		dvb.dvb_net->dvb_net_release(dvb.dvb_net);
		dvb.dvb_net = 0;
#ifdef MODULE
		MOD_DEC_USE_COUNT;
#endif
		return 0;
	}

	return -ENXIO;
}

int register_demux (dmx_demux_t *demux)
{
	if (!dvb.dmxdev.demux)
	{
		dvb.dmxdev.filternum = 32;
		dvb.dmxdev.demux = demux;
		dvb.dmxdev.capabilities = 0;
#ifdef MODULE
		MOD_INC_USE_COUNT;
#endif
		return DmxDevInit(&dvb.dmxdev);
	}

	return -EEXIST;
}

int unregister_demux (struct dmx_demux_s * demux)
{
	if (dvb.dmxdev.demux == demux)
	{
		DmxDevRelease(&dvb.dmxdev);
		dvb.dmxdev.demux = 0;

		if (dvb.dvb_net)
		{
			unregister_dvbnet(dvb.dvb_net);
		}
#ifdef MODULE
		MOD_DEC_USE_COUNT;
#endif
		return 0;
	}

	return -ENXIO;
}

#ifdef MODULE
int __init dvb_init_module(void)
{
	return dvb_register(&dvb);
}

void __exit dvb_cleanup_module (void)
{
	return dvb_unregister(&dvb);
}

module_init(dvb_init_module);
module_exit(dvb_cleanup_module);
EXPORT_SYMBOL(register_frontend);
EXPORT_SYMBOL(unregister_frontend);
EXPORT_SYMBOL(register_demux);
EXPORT_SYMBOL(unregister_demux);
EXPORT_SYMBOL(register_dvbnet);
EXPORT_SYMBOL(unregister_dvbnet);
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */
#endif /* MODULE */
