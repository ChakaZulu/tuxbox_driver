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
 * $Id: dvb.c,v 1.37 2001/06/24 18:29:02 gillem Exp $
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
#include <ost/osd.h>
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

/* dirty - gotta do that better... -Hunz */
#define EBUSOVERLOAD -6
//#define EINTERNAL		-5

typedef struct dvb_struct
{
	dmxdev_t						dmxdev;
	dvb_device_t					dvb_dev;
	struct frontend					front;
	struct demod_function_struct	*demod;
	qpsk_t							qpsk;
	struct videoStatus				videostate;
	int								num;
	dvb_net_t						dvb_net;
} dvb_struct_t;

static dvb_struct_t dvb;
																																// F R O N T E N D
static int tuner_setfreq(dvb_struct_t *dvb, unsigned int freq)
{
	if (dvb->demod->set_frequency)
		return dvb->demod->set_frequency(freq);
	
	printk("couldn't set tuner frequency because of missing/old driver\n");
	return -1;
}

static int frontend_init(dvb_struct_t *dvb)
{
	struct frontend fe;
	if (!dvb->demod)
		panic("demod not yet initialized");
	dvb->demod->init();
	dvb->demod->get_frontend(&fe);
	if (fe.type==FRONT_DVBS)
	{
		printk("using QPSK\n");
		// tuner init.
		fe.power=OST_POWER_ON;
		fe.AFC=1;
		fe.fec=0;
		fe.channel_flags=DVB_CHANNEL_FTA;
		fe.volt=SEC_VOLTAGE_13;
		fe.ttk=SEC_TONE_ON;
		fe.diseqc=0;
		fe.freq=fe.curfreq=(12666000-10600000)*1000;
		fe.srate=22000000;
		fe.video_pid=162;
		fe.audio_pid=96;
		fe.tt_pid=0x1012;
		fe.inv=0;
	} else if (fe.type==FRONT_DVBC)
	{
		printk("using QAM\n");
		fe.power=1; // <- that's false! in 2 ways even !! -Hunz
		fe.freq=394000000;
		fe.srate=6900000;
		fe.video_pid=0x262;
		fe.audio_pid=0x26c;
		fe.qam=2;
		fe.inv=0;
	}
	dvb->demod->set_frontend(&fe);
	
	dvb->front=fe;
	
	return 0;
}

int SetSec(powerState_t power,secVoltage voltage,secToneMode contone) {
	int volt, tone;

	// no bus overload error-handling here - we need this function for SEC-bus-reset after overload

	if (contone != SEC_TONE_ON)
		tone=0;
	else
		tone=1;
	
	switch(voltage) {
	case SEC_VOLTAGE_OFF:
		volt=0;
		break;
	case SEC_VOLTAGE_LT:
		volt=-1;
		break;
	case SEC_VOLTAGE_13:
		volt=1;
		break;
	case SEC_VOLTAGE_13_5:
		volt=2;
		break;
	case SEC_VOLTAGE_18:
		volt=3;
		break;
	case SEC_VOLTAGE_18_5:
		volt=4;
		break;
	default:
		return -EINVAL;
	}
	if (power != OST_POWER_ON)
		volt=0;

	if (!dvb.demod || !dvb.demod->set_sec)
		return -EINVAL; 
	return dvb.demod->set_sec(volt,tone);
}

int secSetTone(struct dvb_struct *dvb, secToneMode mode) {
	int res, status;
	secToneMode old;
	
	if (!dvb->demod || !dvb->demod->sec_status)
		return -EINVAL;

	// ERROR handling
	status=dvb->demod->sec_status();
	if (status == -1)
		return -EBUSOVERLOAD;
	else if (status == -2)
		return -EBUSY;
	else if (status < 0)
		return -EINTERNAL;

	old=dvb->front.ttk;
	dvb->front.ttk=mode;
	res=SetSec(dvb->front.power,dvb->front.volt,mode);
	if (res < 0)
		dvb->front.ttk=old;
	return res;
}

int secSetVoltage(struct dvb_struct *dvb, secVoltage voltage) {
	int res, status;
	secVoltage old;

	if (!dvb->demod || !dvb->demod->sec_status)
		return -EINVAL;

	// ERROR handling
	status=dvb->demod->sec_status();
	
	if (status == -1)
		return -EBUSOVERLOAD;
	else if (status == -2)
		return -EBUSY;
	else if (status < 0)
		return -EINTERNAL;
	
	old=dvb->front.volt;
	dvb->front.volt=voltage;
	res=SetSec(dvb->front.power,voltage,dvb->front.ttk);
	// error
	if (res < 0)
		dvb->front.volt=old;	 
	return res;
}

int secSendSequence(struct dvb_struct *dvb, struct secCmdSequence *seq)
{
	int i, ret, burst, len, status;
	secVoltage old_volt;
	secToneMode old_tone;
	u8 msg[16];
	
	if (!dvb->demod || !dvb->demod->sec_status || !dvb->demod->send_diseqc)
		return -EINVAL;

	// ERROR handling
	status=dvb->demod->sec_status();
	
	if (status == -1)
		return -EBUSOVERLOAD;
	else if (status == -2)
		return -EBUSY;
	else if (status < 0)
		return -EINTERNAL;

	switch (seq->miniCommand)
	{
	case SEC_MINI_NONE:
		burst=-1;
		break;
	case SEC_MINI_A:
		burst=0;
		break;
	case SEC_MINI_B:
		burst=1;
		break;
	default:
		return -EINVAL;
	}
	
	for (i=0; i<seq->numCommands; i++) {
		switch (seq->commands[i].type) {
		case SEC_CMDTYPE_DISEQC:
			len=seq->commands[i].u.diseqc.numParams;
			if (len>SEC_MAX_DISEQC_PARAMS)
				return -EINVAL;
			
			msg[0]=0xe0;
			msg[1]=seq->commands[i].u.diseqc.addr;
			msg[2]=seq->commands[i].u.diseqc.cmd;
			memcpy(msg+3, &seq->commands[i].u.diseqc.params, len);
			ret=dvb->demod->send_diseqc(msg,len+3);
			if (ret < 0)
				return -EINTERNAL;
			break;
		case SEC_CMDTYPE_PAUSE:
			// what to do here ??
			break;
		case SEC_CMDTYPE_VSEC:
		default:
			return -EINVAL;
		}
	}
				
	if (burst != -1) {
		if (burst==0)
			msg[0]=0;
		else if (burst == 1)
			msg[0]=0xFF;
		ret=dvb->demod->send_diseqc(msg,1);
		if (ret < 0)
			return -EINTERNAL;
	}

	old_volt=dvb->front.volt;
	old_tone=dvb->front.ttk;
	dvb->front.volt=seq->voltage;
	ret=secSetTone(dvb, seq->continuousTone);
	if (ret < 0) {
		dvb->front.volt=old_volt;
		dvb->front.ttk=old_tone;
	}
	return ret;
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
			if (!dvb->demod)
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
		case DVB_DEVICE_OSD:
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
		case DVB_DEVICE_OSD:
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
		case DVB_DEVICE_OSD:
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
		case DVB_DEVICE_OSD:
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
					printk("CHCH [DECODER] ABORT\n");
					avia_command(Abort, 0);
					break;
				}
				case VIDEO_PLAY:
				{
					printk("CHCH [DECODER] PLAY\n");
					avia_command(Play, 50, 0, 0);
					udelay(100*1000);
					avia_flush_pcr();
					if (dvb->dmxdev.demux)
						dvb->dmxdev.demux->flush_pcr();
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
						printk("CHCH [DECODER] SETSTREAMTYPE\n");
						avia_command(SetStreamType, 0xB);
						avia_command(SelectStream, 0, 0);
						avia_command(SelectStream, 2, 0);
						avia_command(SelectStream, 3, 0);
						wDR(0x468, 0xFFFF);	// new audio config
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
		case DVB_DEVICE_FRONTEND:
		{
			if (!dvb->demod)
				return -ENOSYS;

			switch (cmd)
			{
				case OST_SELFTEST:
				{
					if ((file->f_flags&O_ACCMODE)==O_RDONLY)
						return -EPERM;
					if(dvb->demod->sec_status)
						return dvb->demod->sec_status(); // anyone a better idea ?
					else
						return -ENOSYS;
					break;
				}
				case OST_SET_POWER_STATE:
				{
					uint32_t pwr,old;
					int res;
	
					if ((file->f_flags&O_ACCMODE)==O_RDONLY)
						return -EPERM;
					if (copy_from_user(&pwr, parg, sizeof(pwr))) //FIXME!! -Hunz
						return -EFAULT;
					if(dvb->demod->set_sec)
					{
						if (!dvb->demod->sec_status || (dvb->demod->sec_status() < 0))
							return -EINVAL;
						old=dvb->front.power;
						if(pwr == OST_POWER_OFF)
							dvb->front.power=OST_POWER_OFF;
						else
							dvb->front.power=OST_POWER_ON;
						res=SetSec(dvb->front.power,dvb->front.volt,dvb->front.ttk);
						if (res < 0)
							dvb->front.power=old;
						return res;
					}	else
						return -ENOSYS;
					break;
				}
				case OST_GET_POWER_STATE:
				{
					uint32_t pwr;
	
					if ((file->f_flags&O_ACCMODE)==O_WRONLY)
						return -EPERM;
					pwr=dvb->front.power;
					if(copy_to_user(parg, &pwr, sizeof(pwr)))
						return -EFAULT;
					break;	
				}
				case FE_READ_STATUS:
				{
					feStatus stat;

					stat=0;
					dvb->demod->get_frontend(&dvb->front);
					if (dvb->front.power)
						stat|=FE_HAS_POWER;
					if ((dvb->front.sync&0x1f)==0x1f)
						stat|=FE_HAS_SIGNAL;
					if (dvb->front.inv)
						stat|=QPSK_SPECTRUM_INV;

					if(copy_to_user(parg, &stat, sizeof(stat)))
						return -EFAULT;
					break;
				}
				case FE_READ_BER:
				{
					uint32_t ber;
			
					dvb->demod->get_frontend(&dvb->front);
					if (!dvb->front.power)
						return -ENOSIGNAL;
					ber=dvb->front.vber*10;
					if(copy_to_user(parg, &ber, sizeof(ber)))
						return -EFAULT;
					break;
				}
				case FE_READ_SIGNAL_STRENGTH:
				{
					int32_t signal;
					dvb->demod->get_frontend(&dvb->front);
					if (!dvb->front.power)
						return -ENOSIGNAL;
					signal=dvb->front.agc;
					if (copy_to_user(parg, &signal, sizeof(signal)))
						return -EFAULT;
					break;
				}
				case FE_READ_SNR:
				{
					return -ENOSYS;
				}
				case FE_READ_UNCORRECTED_BLOCKS:
				{
					uint32_t uncp;

					if ( dvb->demod->get_unc_packet(&uncp) < 0 )
					{
						return -ENOSYS;
					}
					else
					{
						if (copy_to_user(parg, &uncp, sizeof(uncp)))
							return -EFAULT;
					}

					break;
				}
				case FE_GET_NEXT_FREQUENCY:
				{
					uint32_t freq;

					if (copy_from_user(&freq, parg, sizeof(freq)))
						return -EFAULT;
					freq+=1000;
					if (copy_to_user(parg, &freq, sizeof(freq)))
						return -EFAULT;
					break;
				}
				case FE_GET_NEXT_SYMBOL_RATE:
				{
					uint32_t rate;

					if(copy_from_user(&rate, parg, sizeof(rate)))
						return -EFAULT;

					// FIXME: how does one really calculate this?
					if (rate<5000000)
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
				/* QPSK-Stuff */
				case QPSK_TUNE:
				{
					struct qpskParameters para;
					static const uint8_t fectab[8]={8,0,1,2,4,6,0,8};
					u32 val;
	
					if(copy_from_user(&para, parg, sizeof(para)))
						return -EFAULT;
					if ((file->f_flags&O_ACCMODE)==O_RDONLY)
						return -EPERM;
					if (para.FEC_inner>7)
						return -EINVAL;
					val=para.iFrequency*1000;
					if (dvb->front.freq!=val)
					{
//						printk ("frontend.c: fe.freq != val...\n");
						tuner_setfreq(dvb, val);
						dvb->front.freq=val;
					}

					val=para.SymbolRate;
					dvb->front.srate=val;
					dvb->front.fec=fectab[para.FEC_inner];
					dvb->demod->set_frontend(&dvb->front);

					// TODO: qpsk event				
					break;
				}
				case QPSK_GET_EVENT:
				{
					return -ENOTSUPP;
				}
				case QPSK_FE_INFO:
				{
					struct qpskFrontendInfo feinfo;
		
					feinfo.minFrequency=500;	//KHz?
					feinfo.maxFrequency=2100000;
					feinfo.minSymbolRate=500000;
					feinfo.maxSymbolRate=30000000;
					feinfo.hwType=0;		//??
					feinfo.hwVersion=0; //??

					if(copy_to_user(parg, &feinfo, sizeof(feinfo)))
						return -EFAULT;
					break;
				}
				case QPSK_WRITE_REGISTER:
				{
					return -ENOTSUPP;
				}
				case QPSK_READ_REGISTER:
				{
					return -ENOTSUPP;
				}
				case QPSK_GET_STATUS:
				{
					return -ENOTSUPP;
				}
				/* QAM-Stuff */
				case QAM_TUNE:
				{
					return -ENOTSUPP;
				}
				case QAM_GET_EVENT:
				{
					return -ENOTSUPP;
				}
				case QAM_FE_INFO:
				{
					return -ENOTSUPP;
				}
				case QAM_WRITE_REGISTER:
				{
					return -ENOTSUPP;
				}
				case QAM_READ_REGISTER:
				{
					return -ENOTSUPP;
				}
				case QAM_GET_STATUS:
				{
					return -ENOTSUPP;
				}
				default:
				{
					printk("ost_frontend_ioctl: UNEXPECTED cmd: %d.\n", cmd);
					return -EOPNOTSUPP;
				}
			}

			return 0;
		}
		case DVB_DEVICE_SEC:
		{
			if (!dvb->demod->set_sec)
				return -ENOENT;

			switch(cmd)
			{
				case SEC_GET_STATUS:
				{
					struct secStatus status;
					int ret;

					if ((file->f_flags&O_ACCMODE)==O_WRONLY)
						return -EPERM;
					if (!dvb->demod->sec_status)
						return -ENOSYS;

					ret=dvb->demod->sec_status();
					if (ret == 0)
						status.busMode=SEC_BUS_IDLE;
					else if (ret == -1)
						status.busMode=SEC_BUS_OVERLOAD;
					else if (ret == -2)
						status.busMode=SEC_BUS_BUSY;
					else if ((dvb->front.power == SEC_VOLTAGE_OFF) || (dvb->front.power == SEC_VOLTAGE_LT))
						status.busMode=SEC_BUS_OFF;
	
					status.selVolt=dvb->front.volt;
					status.contTone=dvb->front.ttk;
					if(copy_to_user(parg,&status, sizeof(status)))
						return -EFAULT;
					break;
				}
				case SEC_RESET_OVERLOAD:
				{
					if ((file->f_flags&O_ACCMODE)==O_RDONLY)
						return -EPERM;
					dvb->front.power=OST_POWER_ON;
					SetSec(dvb->front.power,dvb->front.volt,dvb->front.ttk);
					break;
				}
				case SEC_SEND_SEQUENCE:
				{
					struct secCmdSequence seq;

					if ((file->f_flags&O_ACCMODE)==O_RDONLY)
						return -EPERM;
					if(copy_from_user(&seq, parg, sizeof(seq)))
						return -EFAULT;
					return secSendSequence(dvb, &seq);
		 		}
				case SEC_SET_TONE:
				{
					secToneMode mode = (secToneMode) arg;

					if ((file->f_flags&O_ACCMODE)==O_RDONLY)
						return -EPERM;
					return secSetTone(dvb,mode);
				}
				case SEC_SET_VOLTAGE:
				{
					secVoltage val = (secVoltage) arg;

					if ((file->f_flags&O_ACCMODE)==O_RDONLY)
						return -EPERM;
					return secSetVoltage(dvb,val);
				}
				default:
					return -EOPNOTSUPP;
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
		case DVB_DEVICE_OSD:
		{
			switch (cmd)
			{
				case OSD_SEND_CMD:
				default:
					return -EOPNOTSUPP;
			}

			return 0;
		}
		case DVB_DEVICE_NET:
		{
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
					result=dvb_net_add_if(&dvb->dvb_net, dvbnetif.pid);
					if (result<0)
						return result;
					dvbnetif.if_num=result;
					if(copy_to_user(parg, &dvbnetif, sizeof(dvbnetif)))
						return -EFAULT;
					break;
				}
				case NET_REMOVE_IF:
				{
					return dvb_net_remove_if(&dvb->dvb_net, (int) arg);
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
#if 0
	if (dvb->qpsk.eventw!=dvb->qpsk.eventr)
		return (POLLIN | POLLRDNORM | POLLPRI);

	poll_wait(file, &dvb->qpsk.eventq, wait);
	if (dvb->qpsk.eventw!=dvb->qpsk.eventr)
		 return (POLLIN | POLLRDNORM | POLLPRI);
#endif
		return 0;
	case DVB_DEVICE_DEMUX:
		return DmxDevPoll(&dvb->dmxdev, file, wait);
//	case DVB_DEVICE_VIDEO:
//	case DVB_DEVICE_AUDIO:
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

static int dvb_register(struct dvb_struct *dvb)
{
//	int i;
	int result;
	struct dvb_device *dvbd=&dvb->dvb_dev;

	dvb->num=0;
	dvb->dmxdev.demux=0;

//	for (i=0; i<32; i++)
//		dvb->handle2filter[i]=NULL;

	init_waitqueue_head(&dvb->qpsk.eventq);
	spin_lock_init (&dvb->qpsk.eventlock);
	dvb->qpsk.eventw=dvb->qpsk.eventr=0;
	dvb->qpsk.overflow=0;
//	dvb->secbusy=0;

	// audiostate

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

int register_demod(struct demod_function_struct *demod)
{
	if (!dvb.demod)
	{
		dvb.demod=demod;
#ifdef MODULE
		MOD_INC_USE_COUNT;
#endif
		return frontend_init(&dvb);
	}
	return -EEXIST;
}

int unregister_demod(struct demod_function_struct *demod)
{
	if (dvb.demod==demod)
	{
		dvb.demod=0;
#ifdef MODULE
		MOD_DEC_USE_COUNT;
#endif
		return 0;
	}
	return -ENOENT;
}

int register_demux(struct dmx_demux_s *demux)
{
	int err;

	if (!dvb.dmxdev.demux)
	{
		dvb.dmxdev.filternum=32;
		dvb.dmxdev.demux=demux;
		dvb.dmxdev.capabilities=0;
#ifdef MODULE
		MOD_INC_USE_COUNT;
#endif
		err = DmxDevInit(&dvb.dmxdev);

		if(!err)
		{
			/* now register net-device with demux */
			dvb.dvb_net.card_num=dvb.num;
			err = dvb_net_init(&dvb.dvb_net, demux);
		}

		return err;
	}
	return -EEXIST;
}

int unregister_demux(struct dmx_demux_s *demux)
{
	if (dvb.dmxdev.demux==demux)
	{
		DmxDevRelease(&dvb.dmxdev);
		dvb.dmxdev.demux=0;
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
EXPORT_SYMBOL(register_demod);
EXPORT_SYMBOL(unregister_demod);
EXPORT_SYMBOL(register_demux);
EXPORT_SYMBOL(unregister_demux);
