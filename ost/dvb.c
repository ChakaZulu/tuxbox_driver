/*
 * dvbdev.c
 *
 * Copyright (C) 2000 tmbinc & gillem (?)
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
 * $Id: dvb.c,v 1.54 2002/01/22 22:35:26 tmbinc Exp $
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
// #include <dbox/fp.h>
#include <dbox/avia.h>
#include <dbox/cam.h>
#include <dbox/avs_core.h>

#include <mtdriver/scartApi.h>
#include <mtdriver/ostErrors.h>

#include "dvbdev.h"
#include "dmxdev.h"
#include "dvb_net.h"
#include <dbox/dvb_frontend.h>

/* dirty - gotta do that better... -Hunz */
#define EBUSOVERLOAD -6
//#define EINTERNAL		-5

typedef struct dvb_struct
{
	dmxdev_t						dmxdev;
	dvb_device_t				dvb_dev;
	dvb_front_t					*frontend;
	struct videoStatus	videostate;
	FrontendPowerState	powerstate;
	struct secStatus		sec;
	int									num;
	dvb_net_t						*dvb_net;
} dvb_struct_t;

static dvb_struct_t dvb;

static int secSetTone(struct dvb_struct *dvb, secToneMode mode)
{
	if (dvb->frontend)
	{
		dvb->sec.contTone=mode;
		dvb_frontend_sec_set_tone(dvb->frontend, mode);
		return 0;
	}
	return -ENOENT;
}

static int secSetVoltage(struct dvb_struct *dvb, secVoltage voltage)
{
	if (dvb->frontend)
	{
		dvb->sec.selVolt=voltage;
		dvb_frontend_sec_set_voltage(dvb->frontend, voltage);
		return 0;
	} else
		return -ENOENT;
}

static int
secSendSequence(struct dvb_struct *dvb, struct secCmdSequence *seq)
{
	int i, ret;
	struct secCommand scommands;
	
	if (!dvb->frontend)
	{
		printk("no frontend.\n");
		return -ENOENT;
	}
	
	switch (seq->miniCommand)
	{
	case SEC_MINI_NONE:
	case SEC_MINI_A:
	case SEC_MINI_B:
		break;
	default:
		return -EINVAL;
	}
	
	for (i=0; i<seq->numCommands; i++) {
		if (copy_from_user(&scommands, &seq->commands[i],
				sizeof(struct secCommand)))
			continue;
		dvb_frontend_sec_command(dvb->frontend, &scommands);
	}
	
	if (seq->miniCommand!=SEC_MINI_NONE)
		dvb_frontend_sec_mini_command(dvb->frontend, seq->miniCommand);
	
	ret=secSetVoltage(dvb, seq->voltage);
	if (ret<0)
	{
		printk("setVoltage failed.\n");
		return ret;
	}
	ret=secSetTone(dvb, seq->continuousTone);
	if (ret<0)
	{
		printk("setTone failed.\n");
	}
	return ret;
}


void tuning_complete_cb(void *priv)
{
	struct dvb_struct *dvb=(struct dvb_struct *) priv;
}
        
static int frontend_init(dvb_struct_t *dvb)
{
	FrontendParameters para;

	dvb->frontend->priv=(void *)dvb;
	dvb->frontend->complete_cb=tuning_complete_cb;
	dvb_frontend_init(dvb->frontend);
	
	dvb->powerstate=FE_POWER_ON;
	
	switch (dvb->frontend->type) {
	case DVB_S:
		para.Frequency=12480000-10600000;
		para.u.qpsk.SymbolRate=27500000;
		para.u.qpsk.FEC_inner=0;
		secSetTone(dvb, SEC_TONE_ON);
		secSetVoltage(dvb, SEC_VOLTAGE_13);
		break;
	case DVB_C:
		para.Frequency=394000000;
		para.u.qam.SymbolRate=6900000;
		para.u.qam.FEC_inner=0;
		para.u.qam.QAM=QAM_64;
		break;
	case DVB_T:
		para.Frequency=730000000;
		para.u.ofdm.bandWidth=BANDWIDTH_8_MHZ;
		para.u.ofdm.HP_CodeRate=FEC_2_3;
		para.u.ofdm.LP_CodeRate=FEC_1_2;
		para.u.ofdm.Constellation=QAM_16;
		para.u.ofdm.TransmissionMode=TRANSMISSION_MODE_2K;
		para.u.ofdm.guardInterval=GUARD_INTERVAL_1_8;
		para.u.ofdm.HierarchyInformation=HIERARCHY_NONE;
		break;
	}
	return 0;
}

static int demod_command(struct dvb_struct *dvb, unsigned int cmd, void *arg)
{
	if (!dvb->frontend->demod)
		return -1;
	return dvb->frontend->demod->driver->command(dvb->frontend->demod, cmd, arg);
}

int dvb_open(struct dvb_device *dvbdev, int type, struct inode *inode, struct file *file)
{
	struct dvb_struct *dvb=(struct dvb_struct *) dvbdev->priv;

	switch (type)
	{
		case DVB_DEVICE_DVBCLK:
		{
			break;
		}
		case DVB_DEVICE_DVBFE:
		{
			break;
		}
		case DVB_DEVICE_DEMUX:
		{
			if (!dvb->dmxdev.demux)
				return -ENOENT;
			return DmxDevFilterAlloc(&dvb->dmxdev, file);
		}
		case DVB_DEVICE_SEC:
		{
			if (file->f_flags&O_NONBLOCK)
				return -EWOULDBLOCK;
			break;
		}
		case DVB_DEVICE_CC:
		{
			break;
		}
		case DVB_DEVICE_SCART:
		{
			break;
		}
		case DVB_DEVICE_DVBTEST:
		{
			break;
		}
		case DVB_DEVICE_OPM:
		{
			break;
		}
		case DVB_DEVICE_SC:
		{
			break;
		}
		case DVB_DEVICE_VIDEO:
		{
			
			break;
		}
		case DVB_DEVICE_AUDIO:
		{	
			printk("open audio device\n");
			break;
		}
		case DVB_DEVICE_DSCR:
		{
			break;
		}
		case DVB_DEVICE_FPRTC:
		{
			break;
		}
		case DVB_DEVICE_DVBFLASH:
		{
			break;
		}
		case DVB_DEVICE_TTXT:
		{
			break;
		}
		case DVB_DEVICE_IRRC:
		{
			break;
		}
		case DVB_DEVICE_CI:
		{
			break;
		}
		case DVB_DEVICE_FPD:
		{
			break;
		}
		case DVB_DEVICE_OSTKBD:
		{
			break;
		}
		case DVB_DEVICE_DVBIO:
		{
			break;
		}
		case DVB_DEVICE_FRONTEND:
		{
			if (!dvb->frontend)
				return -ENOENT;
			break;
		}
		case DVB_DEVICE_DVR:
		{
			if (!dvb->dmxdev.demux)
				return -ENOENT;
			return DmxDevDVROpen(&dvb->dmxdev, file);
		}
		case DVB_DEVICE_CA:
		{
			break;
		}
		case DVB_DEVICE_NET:
		{
			break;
		}
		default:
		{
			return -EOPNOTSUPP;
		}
	}

	return 0;
}

int dvb_close(struct dvb_device *dvbdev, int type, struct inode *inode, struct file *file)
{
	struct dvb_struct *dvb=(struct dvb_struct *) dvbdev->priv;

	switch (type)
	{
		case DVB_DEVICE_DVBCLK:
		{
			break;
		}
		case DVB_DEVICE_DVBFE:
		{
			break;
		}
		case DVB_DEVICE_DEMUX:
		{
			return DmxDevFilterFree(&dvb->dmxdev, file);
		}
		case DVB_DEVICE_SEC:
		{
			break;
		}
		case DVB_DEVICE_CC:
		{
			break;
		}
		case DVB_DEVICE_SCART:
		{
			break;
		}
		case DVB_DEVICE_DVBTEST:
		{
			break;
		}
		case DVB_DEVICE_OPM:
		{
			break;
		}
		case DVB_DEVICE_SC:
		{
			break;
		}
		case DVB_DEVICE_VIDEO:
		{
			break;
		}
		case DVB_DEVICE_AUDIO:
		{
			break;
		}
		case DVB_DEVICE_DSCR:
		{
			break;
		}
		case DVB_DEVICE_FPRTC:
		{
			break;
		}
		case DVB_DEVICE_DVBFLASH:
		{
			break;
		}
		case DVB_DEVICE_TTXT:
		{
			break;
		}
		case DVB_DEVICE_IRRC:
		{
			break;
		}
		case DVB_DEVICE_CI:
		{
			break;
		}
		case DVB_DEVICE_FPD:
		{
			break;
		}
		case DVB_DEVICE_OSTKBD:
		{
			break;
		}
		case DVB_DEVICE_DVBIO:
		{
			break;
		}
		case DVB_DEVICE_FRONTEND:
		{
			break;
		}
		case DVB_DEVICE_DVR:
		{
			return DmxDevDVRClose(&dvb->dmxdev, file);
		}
		case DVB_DEVICE_CA:
		{
			break;
		}
		case DVB_DEVICE_NET:
		{
			break;
		}
		default:
		{
			return -EOPNOTSUPP;
		}
	}

	return 0;
}

ssize_t dvb_read(struct dvb_device *dvbdev, int type, struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct dvb_struct *dvb=(struct dvb_struct *) dvbdev->priv;

	switch (type)
	{
		case DVB_DEVICE_DVBCLK:
		{
			break;
		}
		case DVB_DEVICE_DVBFE:
		{
			break;
		}
		case DVB_DEVICE_DEMUX:
		{
			return DmxDevRead(&dvb->dmxdev, file, buf, count, ppos);
		}
		case DVB_DEVICE_SEC:
		{
			break;
		}
		case DVB_DEVICE_CC:
		{
			break;
		}
		case DVB_DEVICE_SCART:
		{
			break;
		}
		case DVB_DEVICE_DVBTEST:
		{
			break;
		}
		case DVB_DEVICE_OPM:
		{
			break;
		}
		case DVB_DEVICE_SC:
		{
			break;
		}
		case DVB_DEVICE_VIDEO:
		{
			break;
		}
		case DVB_DEVICE_AUDIO:
		{
			break;
		}
		case DVB_DEVICE_DSCR:
		{
			break;
		}
		case DVB_DEVICE_FPRTC:
		{
			break;
		}
		case DVB_DEVICE_DVBFLASH:
		{
			break;
		}
		case DVB_DEVICE_TTXT:
		{
			break;
		}
		case DVB_DEVICE_IRRC:
		{
			break;
		}
		case DVB_DEVICE_CI:
		{
			break;
		}
		case DVB_DEVICE_FPD:
		{
			break;
		}
		case DVB_DEVICE_OSTKBD:
		{
			break;
		}
		case DVB_DEVICE_DVBIO:
		{
			break;
		}
		case DVB_DEVICE_FRONTEND:
		{
			break;
		}
		case DVB_DEVICE_DVR:
		{
			return DmxDevDVRRead(&dvb->dmxdev, file, buf, count, ppos);
		}
		case DVB_DEVICE_CA:
		{
			break;
		}
		case DVB_DEVICE_NET:
		{
			break;
		}
		default:
		{
			return -EOPNOTSUPP;
		}
	}

	return 0;
}

ssize_t dvb_write(struct dvb_device *dvbdev, int type, struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	struct dvb_struct *dvb=(struct dvb_struct *) dvbdev->priv;

	switch (type)
	{
		case DVB_DEVICE_DVBCLK:
		{
			break;
		}
		case DVB_DEVICE_DVBFE:
		{
			break;
		}
		case DVB_DEVICE_DEMUX:
		{
			return DmxDevDVRWrite(&dvb->dmxdev, file, buf, count, ppos);
		}
		case DVB_DEVICE_SEC:
		{
			break;
		}
		case DVB_DEVICE_CC:
		{
			break;
		}
		case DVB_DEVICE_SCART:
		{
			break;
		}
		case DVB_DEVICE_DVBTEST:
		{
			break;
		}
		case DVB_DEVICE_OPM:
		{
			break;
		}
		case DVB_DEVICE_SC:
		{
			break;
		}
		case DVB_DEVICE_VIDEO:
		{
			break;
		}
		case DVB_DEVICE_AUDIO:
		{
			break;
		}
		case DVB_DEVICE_DSCR:
		{
			break;
		}
		case DVB_DEVICE_FPRTC:
		{
			break;
		}
		case DVB_DEVICE_DVBFLASH:
		{
			break;
		}
		case DVB_DEVICE_TTXT:
		{
			break;
		}
		case DVB_DEVICE_IRRC:
		{
			break;
		}
		case DVB_DEVICE_CI:
		{
			break;
		}
		case DVB_DEVICE_FPD:
		{
			break;
		}
		case DVB_DEVICE_OSTKBD:
		{
			break;
		}
		case DVB_DEVICE_DVBIO:
		{
			break;
		}
		case DVB_DEVICE_FRONTEND:
		{
			break;
		}
		case DVB_DEVICE_DVR:
		{
			break;
		}
		case DVB_DEVICE_CA:
		{
			break;
		}
		case DVB_DEVICE_NET:
		{
			break;
		}
		default:
		{
			return -EOPNOTSUPP;
		}
	}

	return -ENOSYS;
}

int dvb_ioctl(struct dvb_device *dvbdev, int type, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct dvb_struct *dvb=(struct dvb_struct *) dvbdev->priv;
	void *parg=(void *)arg;

	switch (type)
	{
		case DVB_DEVICE_VIDEO:
		{
			if (((file->f_flags&O_ACCMODE)==O_RDONLY) && (cmd!=VIDEO_GET_STATUS))
				return -EPERM;

			switch (cmd)
			{
				case VIDEO_STOP:
				{
					dvb->videostate.playState=VIDEO_STOPPED;
					//printk("CHCH [DECODER] ABORT\n");
					avia_command(NewChannel, 0, 0xFFFF, 0xFFFF);
					//avia_command(Abort, 0);
					break;
				}
				case VIDEO_PLAY:
				{
					//printk("CHCH [DECODER] PLAY\n");
					avia_command(NewChannel, 0, 0, 0);
					//avia_command(Play, 0, 0, 0);
					udelay(10*1000);
					//avia_flush_pcr();
					
					//if (dvb->dmxdev.demux)
					//	dvb->dmxdev.demux->flush_pcr();
					dvb->videostate.playState=VIDEO_PLAYING;
					break;
				}
				case VIDEO_FREEZE:
				{
					dvb->videostate.playState=VIDEO_FREEZED;
					avia_wait(avia_command(Freeze, 1));
					break;
				}
				case VIDEO_CONTINUE:
				{
					if (dvb->videostate.playState==VIDEO_FREEZED)
					{
						dvb->videostate.playState=VIDEO_PLAYING;
						avia_wait(avia_command(Resume));
					}
					break;
				}
				case VIDEO_SELECT_SOURCE:
				{
					if (dvb->videostate.playState==VIDEO_STOPPED)
					{
						dvb->videostate.streamSource=(videoStreamSource_t) arg;
						if (dvb->videostate.streamSource!=VIDEO_SOURCE_DEMUX)
							return -EINVAL;
						// printk("CHCH [DECODER] SETSTREAMTYPE\n");
						// avia_command(SetStreamType, 0xB);
						
						avia_flush_pcr();
					
						if (dvb->dmxdev.demux)
						    dvb->dmxdev.demux->flush_pcr();
						
						//avia_command(SelectStream, 0, 0);
						//avia_command(SelectStream, 2, 0);
						//avia_command(SelectStream, 3, 0);
						
						//udelay(1000*10);
						//avia_command(NewChannel,0,0,0);
						//wDR(0x468, 0xFFFF);	// new audio config

					} else
						return -EINVAL;
					break;
				}
				case VIDEO_SET_BLANK:
				{
					dvb->videostate.videoBlank=(boolean) arg;
					break;
				}
				case VIDEO_GET_STATUS:
				{
					if(copy_to_user(parg, &dvb->videostate, sizeof(struct videoStatus)))
						return -EFAULT;
					break;
				}
				case VIDEO_GET_EVENT:
				{
					return -ENOTSUPP;
				}
				case VIDEO_SET_DISPLAY_FORMAT:
				{
					switch ((videoDisplayFormat_t)arg)
					{
						case VIDEO_PAN_SCAN:
							printk("SWITCH PAN SCAN\n");
							dvb->videostate.displayFormat = VIDEO_PAN_SCAN;
							wDR(ASPECT_RATIO_MODE, 1);
							return 0;
						case VIDEO_LETTER_BOX:
							printk("SWITCH LETTER BOX\n");
							dvb->videostate.displayFormat = VIDEO_LETTER_BOX;
							wDR(ASPECT_RATIO_MODE, 2);
							return 0;
						case VIDEO_CENTER_CUT_OUT:
							printk("SWITCH CENTER CUT OUT\n");
							dvb->videostate.displayFormat = VIDEO_CENTER_CUT_OUT;
							wDR(ASPECT_RATIO_MODE, 0);
							return 0;
						default:
							return -ENOTSUPP;
					}

					return -ENOTSUPP;
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
					switch ((videoFormat_t)arg)
					{
						case VIDEO_FORMAT_4_3:
							printk("SWITCH 4:3\n");
							dvb->videostate.videoFormat = VIDEO_FORMAT_4_3;
							wDR(FORCE_CODED_ASPECT_RATIO, 2);
							return 0;
						case VIDEO_FORMAT_16_9:
							printk("SWITCH 16:9\n");
							dvb->videostate.videoFormat = VIDEO_FORMAT_16_9;
							wDR(FORCE_CODED_ASPECT_RATIO, 3);
							return 0;
						case VIDEO_FORMAT_20_9:
							printk("SWITCH 20:9\n");
							dvb->videostate.videoFormat = VIDEO_FORMAT_20_9;
							wDR(FORCE_CODED_ASPECT_RATIO, 4);
							return 0;
						default:
							return -ENOTSUPP;
					}

					return -ENOTSUPP;
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
					avia_wait(avia_command(Digest,\
							((struct videoDigest*)parg)->x,
							((struct videoDigest*)parg)->y,
							((struct videoDigest*)parg)->skip,
							((struct videoDigest*)parg)->decimation,
							((struct videoDigest*)parg)->threshold,
							((struct videoDigest*)parg)->pictureID));
					break;
				}
				default:
					return -ENOIOCTLCMD;
			}

			return 0;
		}
		case DVB_DEVICE_AUDIO:
		{
			switch (cmd)
			{
			        case AUDIO_SET_BYPASS_MODE:
				    
				    switch(arg)
				    {
					case 1:	
					    printk("[AUDIO] disable Bypass (compressed Bitstream on SPDIF off)\n");
					    avia_command(SelectStream,2,0x1FFF);
					    avia_command(SelectStream,3,0);
					    wDR(AUDIO_CONFIG,(rDR(AUDIO_CONFIG)&~1)|1);
					    wDR(0x468,0xFFFF);
					    
					    break;
					case 0:
					    printk("[AUDIO] enable Bypass (compressed Bitstream on SPDIF on)\n");
					    avia_command(SelectStream,3,0x1FFF);
					    avia_command(SelectStream,2,0);
					    wDR(AUDIO_CONFIG,(rDR(AUDIO_CONFIG)&~1));
					    wDR(0x468,0xFFFF);
					    
					    break;
				    }
				    break;	
				default:
					return -ENOIOCTLCMD;
			}
			return 0;
		}
		case DVB_DEVICE_FRONTEND:
		{
			if (!dvb->frontend)
				return -ENOSYS;

			switch (cmd)
			{
			case FE_SELFTEST:
				break;
			case FE_SET_POWER_STATE:
				dvb->powerstate=arg;
				secSetVoltage(dvb, (dvb->powerstate==FE_POWER_ON)?dvb->sec.selVolt:SEC_VOLTAGE_LT);
				return 0;
			case FE_POWER_SUSPEND:
			case FE_POWER_STANDBY:
			case FE_POWER_OFF:
				dvb->powerstate=FE_POWER_OFF;
				secSetVoltage(dvb, SEC_VOLTAGE_LT);
				return 0;
			case FE_GET_POWER_STATE:
				if(copy_to_user(parg, &dvb->powerstate, sizeof(u32)))
					return -EFAULT;
				break;
			case FE_READ_STATUS:
			{
				FrontendStatus stat=0;
				demod_command(dvb, FE_READ_STATUS, &stat);
				if (dvb->powerstate==FE_POWER_ON)
					stat|=FE_HAS_POWER;
				if(copy_to_user(parg, &stat, sizeof(stat)))
					return -EFAULT;
				break;
			}
			case FE_READ_BER:
			{
				uint32_t ber;
				if (dvb->powerstate!=FE_POWER_ON)
					return -ENOSIGNAL;
				demod_command(dvb, FE_READ_BER, &ber);
				if(copy_to_user(parg, &ber, sizeof(ber)))
					return -EFAULT;
				break;
			}
			case FE_READ_SIGNAL_STRENGTH:
			{
				int32_t signal;
				if (dvb->powerstate!=FE_POWER_ON)
					return -ENOSIGNAL;
				demod_command(dvb, FE_READ_SIGNAL_STRENGTH, &signal);
				if(copy_to_user(parg, &signal, sizeof(signal)))
					return -EFAULT;
				break;
			}
			case FE_READ_SNR:
			{
				int32_t snr;
				if (dvb->powerstate!=FE_POWER_ON)
					return -ENOSIGNAL;
				demod_command(dvb, FE_READ_SNR, &snr);
				if(copy_to_user(parg, &snr, sizeof(snr)))
					return -EFAULT;
				break;
			}
			case FE_READ_UNCORRECTED_BLOCKS:
			{
				u32 ublocks;
				if (dvb->powerstate!=FE_POWER_ON)
					return -ENOSIGNAL;
				if (demod_command(dvb, FE_READ_UNCORRECTED_BLOCKS,
						&ublocks)<0)
					return -ENOSYS;
				if(copy_to_user(parg, &ublocks, sizeof(ublocks)))
					return -EFAULT;
				break;
			}
			case FE_GET_NEXT_FREQUENCY:
			{
				uint32_t freq;
				if (copy_from_user(&freq, parg, sizeof(freq)))
					return -EFAULT;
				if (dvb->frontend->type==DVB_S)
					// FIXME: how does one calculate this?
					freq+=1000; //FIXME: KHz like in QPSK_TUNE??
				else
					freq+=1000000;
				
				if (copy_to_user(parg, &freq, sizeof(freq)))
					return -EFAULT;
				break;
			}
			case FE_GET_NEXT_SYMBOL_RATE:
			{
				uint32_t rate;
				
				if(copy_from_user(&rate, parg, sizeof(rate)))
					return -EFAULT;
					
				if (dvb->frontend->type==DVB_C) {
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
				
				if(copy_to_user(parg, &rate, sizeof(rate)))
					return -EFAULT;
				break;
			}
			case FE_GET_FRONTEND:
			{
				if(copy_to_user(parg, &dvb->frontend->param,
						sizeof(FrontendParameters)))
					return -EFAULT;
				break;
			}
			case FE_SET_FRONTEND:
			{
				FrontendParameters para;
				if ((file->f_flags&O_ACCMODE)==O_RDONLY)
					return -EPERM;
				if(copy_from_user(&para, parg, sizeof(para)))
					return -EFAULT;
				if (dvb->frontend->type==DVB_S && para.u.qpsk.FEC_inner>FEC_NONE)
					return -EINVAL;
				if (dvb->frontend->type==DVB_C && para.u.qam.FEC_inner>FEC_NONE)
					return -EINVAL;
				else if (dvb->frontend->type==DVB_T && (para.u.ofdm.Constellation!=QPSK &&
						para.u.ofdm.Constellation!=QAM_16 && para.u.ofdm.Constellation!=QAM_64))
					return -EINVAL;
				
				if (para.Frequency>20000000)		// if >20Ghz, divide by 1000
					para.Frequency/=1000;

				dvb_frontend_tune(dvb->frontend, &para);
				return 0;
			}
			case FE_GET_EVENT:
			{
				FrontendEvent event;
				int ret;
				ret=dvb_frontend_get_event(dvb->frontend, &event,
					file->f_flags&O_NONBLOCK);
				if (ret<0)
					return ret;
				if(copy_to_user(parg, &event, sizeof(event)))
					return -EFAULT;
				break;
			}
			case FE_GET_INFO:
			{
				FrontendInfo feinfo;
				switch (dvb->frontend->type) {
				case DVB_S:
					feinfo.type=FE_QPSK;
					feinfo.minFrequency=500;  //KHz?
					feinfo.maxFrequency=2700000;
					break;
				case DVB_C:
					feinfo.type=FE_QAM;
					feinfo.minFrequency=40000000;
					feinfo.maxFrequency=870000000;
					break;
				case DVB_T:
					feinfo.type=FE_OFDM;
					feinfo.minFrequency=470000000;
					feinfo.maxFrequency=860000000;
					break;
				}
				feinfo.minSymbolRate=500000;
				feinfo.maxSymbolRate=30000000;
				feinfo.hwType=0;    //??
				feinfo.hwVersion=0;
				if(copy_to_user(parg, &feinfo, sizeof(feinfo)))
					return -EFAULT;
				break;
			}
			default:
				return -ENOIOCTLCMD;
			}
			return 0;
		}
		case DVB_DEVICE_SEC:
		{
			if (!dvb->frontend)
			{
				printk("nix frontend.\n");
				return -ENOENT;
			}

			if (file->f_flags&O_NONBLOCK)
				return -EWOULDBLOCK;
			
			switch(cmd) {
			case SEC_GET_STATUS:
			{
				if(copy_to_user(parg, &dvb->sec, sizeof(dvb->sec)))
					return -EFAULT;
				break;
			}
			case SEC_RESET_OVERLOAD:
			{
				if ((file->f_flags&O_ACCMODE)==O_RDONLY)
					return -EPERM;
				break;
			}
			case SEC_SEND_SEQUENCE:
			{
				struct secCmdSequence seq;
				if(copy_from_user(&seq, parg, sizeof(seq)))
					return -EFAULT;
				
				if ((file->f_flags&O_ACCMODE)==O_RDONLY)
					return -EPERM;
				
				dvb_frontend_stop(dvb->frontend);
				return secSendSequence(dvb, &seq);
			}
			case SEC_SET_TONE:
			{
				secToneMode mode = (secToneMode) arg;

				if ((file->f_flags&O_ACCMODE)==O_RDONLY)
					return -EPERM;
				
				dvb_frontend_stop(dvb->frontend);
				return secSetTone(dvb, mode);
			}
			case SEC_SET_VOLTAGE:
			{
				secVoltage val = (secVoltage) arg;
				
				if ((file->f_flags&O_ACCMODE)==O_RDONLY)
					return -EPERM;

				dvb_frontend_stop(dvb->frontend);
				return secSetVoltage(dvb, val);
			}
			default:
				return -EINVAL;
			}
			return 0;
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
					return -EOPNOTSUPP;
				}
				case CA_GET_SLOT_INFO:
				{
					return -EOPNOTSUPP;
				}
				case CA_GET_DESCR_INFO:
				{
					return -EOPNOTSUPP;
				}
				case CA_GET_MSG:
				{
					ca_msg.index = 0;
					ca_msg.type  = 0;

					if(copy_from_user(&ca_msg, parg, sizeof(ca_msg_t)))
					{
						return -EFAULT;
					}

					((ca_msg_t*)parg)->length = cam_read_message( ((ca_msg_t*)parg)->msg, ca_msg.length );
					/*
					if(copy_to_user(parg, &ca_msg, sizeof(ca_msg_t)))
					{
						return -EFAULT;
					}
					*/
					return 0;
				}
				case CA_SEND_MSG:
				{
					if(copy_from_user(&ca_msg, parg, sizeof(ca_msg_t)))
					{
						return -EFAULT;
					}

					cam_write_message( ca_msg.msg, ca_msg.length );

					return 0;
				}
				case CA_SET_DESCR:
				{
					return -EOPNOTSUPP;
				}
				default:
				{
					return -EOPNOTSUPP;
				}
			}

			return 0;
		}
		case DVB_DEVICE_SCART:
		{
			switch (cmd)
			{
				case SCART_VOLUME_SET:
				case SCART_VOLUME_GET:
				case SCART_MUTE_SET:
				case SCART_MUTE_GET:
				case SCART_AUD_FORMAT_SET:
				case SCART_AUD_FORMAT_GET:
				case SCART_VID_FORMAT_SET:
				case SCART_VID_FORMAT_GET:
				case SCART_VID_FORMAT_INPUT_GET:
				case SCART_SLOW_SWITCH_SET:
				case SCART_SLOW_SWITCH_GET:
				case SCART_RGB_LEVEL_SET:
				case SCART_RGB_LEVEL_GET:
				case SCART_RGB_SWITCH_SET:
				case SCART_RGB_SWITCH_GET:
				case SCART_BYPASS_SET:
				case SCART_BYPASS_GET:
					return scart_command( cmd, parg );
				default:
					return -EOPNOTSUPP;
			}

			return 0;
		}
		case DVB_DEVICE_OSTKBD:
		{
		}
		case DVB_DEVICE_NET:
		{
			if (!dvb->dvb_net)
				return -EINVAL; // ???

			if (((file->f_flags&O_ACCMODE)==O_RDONLY))
				return -EPERM;

			switch (cmd)
			{
				case NET_ADD_IF:
				{
					struct dvb_net_if dvbnetif;
					int result;

					if(copy_from_user(&dvbnetif, parg, sizeof(dvbnetif)))
						return -EFAULT;
					result=dvb->dvb_net->dvb_net_add_if(dvb->dvb_net, dvbnetif.pid);
					if (result<0)
						return result;
					dvbnetif.if_num=result;
					if(copy_to_user(parg, &dvbnetif, sizeof(dvbnetif)))
						return -EFAULT;
					break;
				}
				case NET_REMOVE_IF:
				{
					return dvb->dvb_net->dvb_net_remove_if(dvb->dvb_net, (int) arg);
				}
				default:
					return -EINVAL;
			}

			return 0;
		}
		default:
			return -EOPNOTSUPP;
	}

	return 0;
}

unsigned int dvb_poll(struct dvb_device *dvbdev, int type, struct file *file, poll_table * wait)
{
	struct dvb_struct *dvb=(struct dvb_struct *) dvbdev->priv;

	switch (type) {
	case DVB_DEVICE_FRONTEND:
		return dvb_frontend_poll(dvb->frontend, file, wait);
	case DVB_DEVICE_DEMUX:
		return DmxDevPoll(&dvb->dmxdev, file, wait);
//	case DVB_DEVICE_VIDEO:
//	case DVB_DEVICE_AUDIO:
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

/* device register */

static int dvb_register(struct dvb_struct *dvb)
{
	int result;
	struct dvb_device *dvbd=&dvb->dvb_dev;

	dvb->num=0;
	dvb->dmxdev.demux=0;
	dvb->dvb_net=0;

	dvbd->priv=(void *)dvb;
	dvbd->open=dvb_open;
	dvbd->close=dvb_close;
	dvbd->read=dvb_read;
	dvbd->write=dvb_write;
	dvbd->ioctl=dvb_ioctl;
	dvbd->poll=dvb_poll;

	printk("registering device.\n");

	result=dvb_register_device(dvbd);

	if (result<0)
	{
		printk("yes we can't remove all this ...\n");
		return result;
	}

	return 0;
}

void dvb_unregister(struct dvb_struct *dvb)
{
	return dvb_unregister_device(&dvb->dvb_dev);
}

int register_frontend(dvb_front_t *frontend)
{
	if (!dvb.frontend)
	{
		dvb.frontend=frontend;
		printk("registering frontend...\n");
#ifdef MODULE
		MOD_INC_USE_COUNT;
#endif
		return frontend_init(&dvb);
	}
	return -EEXIST;
}

int unregister_frontend(dvb_front_t *frontend)
{
	if (dvb.frontend==frontend)
	{
		dvb_frontend_exit(frontend);
		dvb.frontend=0;
#ifdef MODULE
		MOD_DEC_USE_COUNT;
#endif
		return 0;
	}
	return -ENOENT;
}

int register_dvbnet(struct dvb_net_s *dvbnet)
{
	if(!dvb.dmxdev.demux)
	{
		return -ENOENT;
	}

	if (!dvb.dvb_net)
	{
		dvb.dvb_net=dvbnet;

#ifdef MODULE
		MOD_INC_USE_COUNT;
#endif
		/* now register net-device with demux */
		dvb.dvb_net->card_num=dvb.num;
		return dvb.dvb_net->dvb_net_init(dvb.dvb_net, dvb.dmxdev.demux);
	}
	return -EEXIST;
}

int unregister_dvbnet(dvb_net_t *dvbnet)
{
	if (dvb.dvb_net==dvbnet)
	{
		dvb.dvb_net->dvb_net_release(dvb.dvb_net);
		dvb.dvb_net=0;
#ifdef MODULE
		MOD_DEC_USE_COUNT;
#endif
		return 0;
	}
	return -ENOENT;
}

int register_demux(dmx_demux_t *demux)
{
	if (!dvb.dmxdev.demux)
	{
		dvb.dmxdev.filternum=32;
		dvb.dmxdev.demux=demux;
		dvb.dmxdev.capabilities=0;
#ifdef MODULE
		MOD_INC_USE_COUNT;
#endif
		return DmxDevInit(&dvb.dmxdev);
	}
	return -EEXIST;
}

int unregister_demux(struct dmx_demux_s *demux)
{
	if (dvb.dmxdev.demux==demux)
	{
		DmxDevRelease(&dvb.dmxdev);
		dvb.dmxdev.demux=0;

		if (dvb.dvb_net)
		{
			unregister_dvbnet(dvb.dvb_net);
		}
#ifdef MODULE
		MOD_DEC_USE_COUNT;
#endif
		return 0;
	}
	return -ENOENT;
}

int __init dvb_init_module(void)
{
	return dvb_register(&dvb);
}

void __exit dvb_cleanup_module (void)
{
	return dvb_unregister(&dvb);
}

module_init ( dvb_init_module );
module_exit ( dvb_cleanup_module );
EXPORT_SYMBOL(register_frontend);
EXPORT_SYMBOL(unregister_frontend);
EXPORT_SYMBOL(register_demux);
EXPORT_SYMBOL(unregister_demux);
EXPORT_SYMBOL(register_dvbnet);
EXPORT_SYMBOL(unregister_dvbnet);
