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
 * $Id: dvb.c,v 1.62 2002/02/01 15:44:13 gillem Exp $
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

static int debug = 0;
#define dprintk if (debug) printk

typedef struct dvb_struct
{
	int				num;

	dmxdev_t			dmxdev;
	struct frontend			front;
	struct demod_function_struct	*demod;
	qpsk_t				qpsk;
	struct videoStatus		videostate;
//        int                     	display_ar;
        int                     	trickmode;
#define TRICK_NONE   0
#define TRICK_FAST   1
#define TRICK_SLOW   2
#define TRICK_FREEZE 3
        struct audioStatus      audiostate;

        /* Recording and playback flags */

        int                     rec_mode;
        int                     playing;
#define RP_NONE  0
#define RP_VIDEO 1
#define RP_AUDIO 2
#define RP_AV    3

        /* DVB device */

        struct dvb_device dvb_dev;
        dvb_net_t * dvb_net;

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
		fe.power=OST_POWER_ON;
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

/* osd stuff */
static int
OSD_DrawCommand(struct dvb_struct *dvb, osd_cmd_t *dc)
{
/*	switch (dc->cmd)
	{
		case OSD_Close:
				//DestroyOSDWindow(dvb, dvb->osdwin);
                return 0;
		case OSD_Open:
				dvb->osdbpp[dvb->osdwin]=(dc->color-1)&7;
				CreateOSDWindow(dvb, dvb->osdwin, bpp2bit[dvb->osdbpp[dvb->osdwin]],
								dc->x1-dc->x0+1, dc->y1-dc->y0+1);
				MoveWindowAbs(dvb, dvb->osdwin, dc->x0, dc->y0);
				SetColorBlend(dvb, dvb->osdwin);
				return 0;
		case OSD_Show:
				MoveWindowRel(dvb, dvb->osdwin, 0, 0);
				return 0;
		case OSD_Hide:
				HideWindow(dvb, dvb->osdwin);
				return 0;
		case OSD_Clear:
				DrawBlock(dvb, dvb->osdwin, 0, 0, 720, 576, 0);
				return 0;
		case OSD_Fill:
				DrawBlock(dvb, dvb->osdwin, 0, 0, 720, 576, dc->color);
				return 0;
		case OSD_SetColor:
				OSDSetColor(dvb, dc->color, dc->x0, dc->y0, dc->x1, dc->y1);
				return 0;
		case OSD_SetPalette:
		{
				int i, len=dc->x0-dc->color+1;
				u8 colors[len*4];

				if (copy_from_user(colors, dc->data, sizeof(colors)))
						return -EFAULT;
				for (i=0; i<len; i++)
					OSDSetColor(dvb, dc->color+i,
								colors[i*4]  , colors[i*4+1],
								colors[i*4+2], colors[i*4+3]);
				return 0;
		}
		case OSD_SetTrans:
				return 0;
		case OSD_SetPixel:
				DrawLine(dvb, dvb->osdwin,
						dc->x0, dc->y0, 0, 0,
						dc->color);
				return 0;
		case OSD_GetPixel:
				return 0;

		case OSD_SetRow:
				dc->y1=dc->y0;
				return 0;
		case OSD_SetBlock:
				OSDSetBlock(dvb, dc->x0, dc->y0, dc->x1, dc->y1, dc->color, dc->data);
				return 0;

		case OSD_FillRow:
				DrawBlock(dvb, dvb->osdwin, dc->x0, dc->y0,
						dc->x1-dc->x0+1, dc->y1,
						dc->color);
				return 0;
		case OSD_FillBlock:
				DrawBlock(dvb, dvb->osdwin, dc->x0, dc->y0,
						dc->x1-dc->x0+1, dc->y1-dc->y0+1,
						dc->color);
				return 0;
		case OSD_Line:
				DrawLine(dvb, dvb->osdwin,
						dc->x0, dc->y0, dc->x1-dc->x0, dc->y1-dc->y0,
						dc->color);
				return 0;
		case OSD_Query:
				return 0;
		case OSD_Test:
				return 0;
		case OSD_Text:
		{
				char textbuf[240];

				if (strncpy_from_user(textbuf, dc->data, 240)<0)
						return -EFAULT;
				textbuf[239]=0;
				if (dc->x1>3)
						dc->x1=3;
				SetFont(dvb, dvb->osdwin, dc->x1,
						(u16) (dc->color&0xffff), (u16) (dc->color>>16));
				FlushText(dvb);
				WriteText(dvb, dvb->osdwin, dc->x0, dc->y0, textbuf);
				return 0;
		}
		case OSD_SetWindow:
				if (dc->x0<1 || dc->x0>7)
					return -EINVAL;
				dvb->osdwin=dc->x0;
				return 0;
		case OSD_MoveWindow:
				MoveWindowAbs(dvb, dvb->osdwin, dc->x0, dc->y0);
				SetColorBlend(dvb, dvb->osdwin);
				return 0;
		default:
				return -EINVAL;
	}*/
	return 0;
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
			if ((file->f_flags&O_ACCMODE)!=O_RDWR)
				return -EINVAL;
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
	                return DmxDevDVRWrite(&dvb->dmxdev, file, buf, count, ppos);
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
	                if (((file->f_flags&O_ACCMODE)==O_RDONLY) &&
        	            (cmd!=VIDEO_GET_STATUS))
                	        return -EPERM;

			switch (cmd)
			{
				case VIDEO_STOP:
				{
					dprintk("DVB: VIDEO_STOP\n");

					dvb->videostate.playState=VIDEO_STOPPED;
					if (dvb->videostate.streamSource==VIDEO_SOURCE_MEMORY) {
                                		//AV_Stop(dvb, RP_VIDEO);
					} else {
						avia_command(NewChannel, 0, 0xFFFF, 0xFFFF);
						//avia_command(Abort, 0);
					}
					dvb->trickmode=TRICK_NONE;
                		        break;
				}
				case VIDEO_PLAY:
				{
					dprintk("DVB: VIDEO_PLAY\n");

					dvb->trickmode=TRICK_NONE;
					if (dvb->videostate.playState==VIDEO_FREEZED) {
						dvb->videostate.playState=VIDEO_PLAYING;
						avia_wait(avia_command(Resume));
					}

					if (dvb->videostate.streamSource==VIDEO_SOURCE_MEMORY) {
                                		if (dvb->playing==RP_AV) {
							dvb->playing&=~RP_VIDEO;
						}
						//AV_StartPlay(dvb,RP_VIDEO);
					} else {
						avia_command(NewChannel, 0, 0, 0);
						udelay(10*1000);
					}
					dvb->videostate.playState=VIDEO_PLAYING;
					break;
				}
				case VIDEO_FREEZE:
				{
					dprintk("DVB: VIDEO_FREEZE\n");

					dvb->videostate.playState=VIDEO_FREEZED;
                        		if (dvb->playing&RP_VIDEO) {
						//
					} else {
						avia_wait(avia_command(Freeze, 1));
					}
                        		dvb->trickmode=TRICK_FREEZE;
					break;
				}
				case VIDEO_CONTINUE:
				{
					dprintk("DVB: VIDEO_CONTINUE\n");

                        		if (dvb->playing&RP_VIDEO) {
						//
					}

					if (dvb->videostate.playState==VIDEO_FREEZED) {
						avia_wait(avia_command(Resume));
					}
					dvb->videostate.playState=VIDEO_PLAYING;
					dvb->trickmode=TRICK_NONE;
					break;
				}
				case VIDEO_SELECT_SOURCE:
				{
					dprintk("DVB: VIDEO_SELECT_SOURCE\n");
					if (((videoStreamSource_t) arg)!=VIDEO_SOURCE_DEMUX)
						return -EINVAL;

					dvb->videostate.streamSource=(videoStreamSource_t) arg;

					avia_command(SetStreamType, 0xB);
					avia_flush_pcr();
					
					if (dvb->dmxdev.demux)
					    dvb->dmxdev.demux->flush_pcr();
					break;
				}
				case VIDEO_SET_BLANK:
				{
					dvb->videostate.videoBlank=(boolean) arg;
					break;
				}
				case VIDEO_GET_STATUS:
				{
					if (copy_to_user(parg, &dvb->videostate,
						sizeof(struct videoStatus)))
						return -EFAULT;
					break;
				}
				case VIDEO_GET_EVENT:
				{
					//FIXME: write firmware support for this [*ggg*]
					return -EOPNOTSUPP;
				}
				case VIDEO_SET_DISPLAY_FORMAT:
				{
					videoDisplayFormat_t format=(videoDisplayFormat_t) arg;
					u16 val=0;

					switch (format)
					{
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
							return -ENOTSUPP;
					}

					dvb->videostate.displayFormat=format;
					//dvb->videostate.videoFormat=format; ??? ich glaube wohl nicht ...
					wDR(ASPECT_RATIO_MODE, val);

					break;
				}
				case VIDEO_SET_FORMAT:
				{
					videoFormat_t format=(videoFormat_t) arg;
					u16 val=0;

					switch (format)
					{
						case VIDEO_FORMAT_4_3:
							val=2;
							break;
						case VIDEO_FORMAT_16_9:
							val=3;
							break;
						case VIDEO_FORMAT_20_9:
							val=3;
							break;
						default:
							return -ENOTSUPP;
					}

					dvb->videostate.videoFormat = format;
					//dvb->display_ar=arg; ???
					wDR(FORCE_CODED_ASPECT_RATIO, val);

					break;
				}
				case VIDEO_STILLPICTURE:
				{
					struct videoDisplayStillPicture pic;

					if(copy_from_user(&pic, parg,
						sizeof(struct videoDisplayStillPicture))) {
						return -EFAULT;
					}

					//ring_buffer_flush(&dvb->avout);
					//play_iframe(dvb, pic.iFrame, pic.size,
					//	file->f_flags&O_NONBLOCK);
					return -ENOTSUPP;
				}
				case VIDEO_FAST_FORWARD:
				{
					/*//note: arg is ignored by firmware
					if (dvb->playing&RP_VIDEO)
						outcom(dvb, COMTYPE_REC_PLAY,
							__Scan_I, 2, AV_PES, 0);
					else
						vidcom(dvb, 0x16, arg);

					dvb->trickmode=TRICK_FAST;
					dvb->videostate.playState=VIDEO_PLAYING;
					break*/
					return -ENOTSUPP;
				}
				case VIDEO_SLOWMOTION:
				{
                        		/*if (dvb->playing&RP_VIDEO) {
                                		outcom(dvb, COMTYPE_REC_PLAY, __Slow, 2, 0, 0);
                                		vidcom(dvb, 0x22, arg);
                        		} else {
                                		vidcom(dvb, 0x0d, 0);
                                		vidcom(dvb, 0x0e, 0);
                                		vidcom(dvb, 0x22, arg);
                        		}
                        		dvb->trickmode=TRICK_SLOW;
                        		dvb->videostate.playState=VIDEO_PLAYING;
                        		break;*/
					return -ENOTSUPP;
				}
				case VIDEO_GET_CAPABILITIES:
				{
                        		/*int cap=VIDEO_CAP_MPEG1|
                                		VIDEO_CAP_MPEG2|
                                		VIDEO_CAP_SYS|
                                		VIDEO_CAP_PROG;

                        		if (copy_to_user(parg, &cap, sizeof(cap)))
                                		ret=-EFAULT;
                        		break;*/
					return -ENOTSUPP;
				}
				case VIDEO_CLEAR_BUFFER:
				{
                        		/*ring_buffer_flush(&dvb->avout);
                        		reset_ipack(&dvb->ipack[1]);

                        		if (dvb->playing==RP_AV) {
                                		outcom(dvb, COMTYPE_REC_PLAY,
                                      		 __Play, 2, AV_PES, 0);
                                		if (dvb->trickmode==TRICK_FAST)
		                                        outcom(dvb, COMTYPE_REC_PLAY,
                		                               __Scan_I, 2, AV_PES, 0);
		                                if (dvb->trickmode==TRICK_SLOW) {
		                                        outcom(dvb, COMTYPE_REC_PLAY, __Slow, 2, 0, 0);
                		                        vidcom(dvb, 0x22, arg);
	                	                }
		                                if (dvb->trickmode==TRICK_FREEZE)
        		                                vidcom(dvb, 0x000e, 1);
		                        }
		                        break;*/
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
	                if (((file->f_flags&O_ACCMODE)==O_RDONLY) &&
        	            (cmd!=AUDIO_GET_STATUS))
                	        return -EPERM;

			switch (cmd)
			{
				case AUDIO_STOP:
				{
					return -ENOTSUPP;
				}
				case AUDIO_PLAY:
				{
					return -ENOTSUPP;
				}
				case AUDIO_PAUSE:
				{
					return -ENOTSUPP;
				}
				case AUDIO_CONTINUE:
				{
					return -ENOTSUPP;
				}
				case AUDIO_SELECT_SOURCE:
				{
					return -ENOTSUPP;
				}
				case AUDIO_SET_MUTE:
				{
					return -ENOTSUPP;
				}
				case AUDIO_SET_AV_SYNC:
				{
					return -ENOTSUPP;
				}
			        case AUDIO_SET_BYPASS_MODE:
				{
					switch(arg)
					{
						case 1:	
							dprintk("[AUDIO] disable Bypass (compressed Bitstream on SPDIF off)\n");
							avia_command(SelectStream,2,0x1FFF);
							avia_command(SelectStream,3,0);
							wDR(AUDIO_CONFIG,(rDR(AUDIO_CONFIG)&~1)|1);
							wDR(0x468,0xFFFF);
							break;

						case 0:
							dprintk("[AUDIO] enable Bypass (compressed Bitstream on SPDIF on)\n");
							avia_command(SelectStream,3,0x1FFF);
							avia_command(SelectStream,2,0);
							wDR(AUDIO_CONFIG,(rDR(AUDIO_CONFIG)&~1));
							wDR(0x468,0xFFFF);
							break;

						default:
							return -ENOTSUPP;
					}
				}
				case AUDIO_CHANNEL_SELECT:
				{
					dvb->audiostate.channelSelect=(audioChannelSelect_t) arg;

					switch(dvb->audiostate.channelSelect) {
						case AUDIO_STEREO:
							break;

						case AUDIO_MONO_LEFT:
							break;

						case AUDIO_MONO_RIGHT:
							break;

						default:
							return -EINVAL;
					}
				}
				case AUDIO_GET_STATUS:
				{
					if(copy_to_user(parg, &dvb->audiostate,
						sizeof(struct audioStatus)))
						return -EFAULT;
					break;
				}
				case AUDIO_GET_CAPABILITIES:
				{
					return -ENOTSUPP;
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
					return -ENOTSUPP;
				}
				case AUDIO_SET_STREAMTYPE:
				{
					return -ENOTSUPP;
				}
				default:
				{
					return -ENOIOCTLCMD;
				}
                        }

			return 0;
		}
		case DVB_DEVICE_FRONTEND:
		{
			if (!dvb->demod)
				return -ENOSYS;

			switch (cmd)
			{
				case FE_SELFTEST:
				{
					if ((file->f_flags&O_ACCMODE)==O_RDONLY)
						return -EPERM;
					if(dvb->demod->sec_status)
						return dvb->demod->sec_status(); // anyone a better idea ?
					else
						return -ENOSYS;
					break;
				}
				case FE_SET_POWER_STATE:
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
					{
						if(pwr == OST_POWER_OFF)
							dvb->front.power=OST_POWER_OFF;
						else
							dvb->front.power=OST_POWER_ON;
						return 0;
					}
					break;
				}
				case FE_GET_POWER_STATE:
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
					if (dvb->front.power==OST_POWER_ON)
						stat|=FE_HAS_POWER;
					if ((dvb->front.sync&0x1f)==0x1f)
						stat|=FE_HAS_SIGNAL;
        		                if ((dvb->front.sync&1))
	                	                stat|=FE_HAS_LOCK;
					if (dvb->front.inv)
						stat|=FE_SPECTRUM_INV;

					if(copy_to_user(parg, &stat, sizeof(stat)))
						return -EFAULT;
					break;
				}
				case FE_READ_BER:
				{
					uint32_t ber;
			
					dvb->demod->get_frontend(&dvb->front);
					if (dvb->front.power!=OST_POWER_ON)
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
					if (dvb->front.power!=OST_POWER_ON)
						return -ENOSIGNAL;
					signal=dvb->front.agc;
					if (copy_to_user(parg, &signal, sizeof(signal)))
						return -EFAULT;
					break;
				}
				case FE_READ_SNR:
				{
					int32_t snr;
					dvb->demod->get_frontend(&dvb->front);
					if (dvb->front.power!=OST_POWER_ON)
						return -ENOSIGNAL;
					snr=dvb->front.nest;
					if (copy_to_user(parg, &snr, sizeof(snr)))
						return -EFAULT;
					break;
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
/*
				case FE_GET_FRONTEND:
                		{
                        		if(copy_to_user(parg, &dvb->frontend.param,
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
                        		return tune(dvb, &para);
                		}
                		case FE_GET_EVENT:
                		{
                        		FrontendEvent event;
                        		int ret;

                        		ret=dvb_frontend_get_event(&dvb->frontend, &event,
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

                        		switch (dvb->frontend.type) {
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
*/
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
				{
					osd_cmd_t doc;

					if(copy_from_user(&doc, parg, sizeof(osd_cmd_t)))
					{
						return -EFAULT;
					}

					return OSD_DrawCommand(dvb, &doc);
                }
                default:
				{
                        return -EINVAL;
                }
			}

			return 0;
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
	case DVB_DEVICE_DVR:
		return DmxDevDVRPoll(&dvb->dmxdev, file, wait);

//	case DVB_DEVICE_VIDEO:
//	case DVB_DEVICE_AUDIO:
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

/******************************************************************************
 * driver registration
 ******************************************************************************/

static int dvb_register(struct dvb_struct *dvb)
{
//	int i;
	int result;
	struct dvb_device *dvbd=&dvb->dvb_dev;

	dvb->num=0;
	dvb->dmxdev.demux=0;
	dvb->dvb_net=0;

//	for (i=0; i<32; i++)
//		dvb->handle2filter[i]=NULL;

	init_waitqueue_head(&dvb->qpsk.eventq);
	spin_lock_init (&dvb->qpsk.eventlock);
	dvb->qpsk.eventw=dvb->qpsk.eventr=0;
	dvb->qpsk.overflow=0;
//	dvb->secbusy=0;

	// audiostate

        // init and register dvb device structure
/* new
        dvbd->priv=(void *) dvb;
        dvbd->open=dvbdev_open;
        dvbd->close=dvbdev_close;
        dvbd->read=dvbdev_read;
        dvbd->write=dvbdev_write;
        dvbd->ioctl=dvbdev_ioctl;
        dvbd->poll=dvbdev_poll;
        dvbd->device_type=dvbdev_device_type;
*/
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

MODULE_PARM(debug,"i");

module_init ( dvb_init_module );
module_exit ( dvb_cleanup_module );
EXPORT_SYMBOL(register_demod);
EXPORT_SYMBOL(unregister_demod);
EXPORT_SYMBOL(register_demux);
EXPORT_SYMBOL(unregister_demux);
EXPORT_SYMBOL(register_dvbnet);
EXPORT_SYMBOL(unregister_dvbnet);
