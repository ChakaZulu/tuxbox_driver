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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Id: dvb.c,v 1.7 2001/03/06 22:08:45 Hunz Exp $
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

#include <asm/uaccess.h>
#include <asm/system.h>

#include <linux/kmod.h>


#include <ost/audio.h>
#include <ost/demux.h>
#include <ost/dmx.h>
#include <ost/frontend.h>
#include <ost/sec.h>
#include <ost/video.h>

#include <dbox/dvb.h>
#include <dbox/ves.h>
#include <dbox/fp.h>
#include <dbox/gtx-dmx.h>
#include <dbox/avia.h>

#include "dvbdev.h"
#include "dmxdev.h"

typedef struct dvb_struct
{
  dmxdev_t              dmxdev;
  gtx_demux_t           hw_demux;
  char                  hw_demux_id[8];
  dvb_device_t          dvb_dev;
  struct frontend       front;
  qpsk_t                qpsk;
  struct videoStatus    videostate;
  int                   num;
} dvb_struct_t;

static dvb_struct_t dvb;
                                                                // F R O N T E N D
static int tuner_setfreq(dvb_struct_t *dvb, unsigned int freq)
{
//  printk("setting frequency to %d (it's %d)\n", freq, dvb->front.type);
  if (dvb->front.type==FRONT_DVBS)
  {
    int p, t, os, c, r, pe;
    unsigned long b;
    
    freq+=479500000; freq/=125000*4;
    t=0; os=0; c=1;                // doch doch
    r=3; pe=1; p=1;

    b=p<<24;
    b|=t<<23;
    b|=os<<22;
    b|=c<<21;
    b|=r<<18;
    b|=pe<<17;
    b|=freq;
    
    fp_set_tuner_dword(T_QPSK, b);
    fp_set_sec(1,1);
    return 0;
  } else if (dvb->front.type==FRONT_DVBC)
  {
    u8 buffer[4];
    freq+=36125000;
    freq/=62500;
    
    printk("div: %d\n", freq);
    
    buffer[0]=(freq>>8)&0x7F;
    buffer[1]=freq&0xFF;
    buffer[2]=0x80 | (((freq>>15)&3)<<6) | 5;
    buffer[3]=1;
    
    fp_set_tuner_dword(T_QAM, *((u32*)buffer));
    return 0;
  } else
  {
    printk("tuner_setfreq: AAARRRGGG\n");
    return -1;
  }
}

static int frontend_init(dvb_struct_t *dvb)
{
  struct frontend fe;
//  ves_init();
  ves_get_frontend(&fe);
  if (fe.type==FRONT_DVBS)
  {
    printk("using QPSK\n");
    // tuner init.
    fe.power=1;
    fe.AFC=1;
    fe.fec=8;
    fe.channel_flags=DVB_CHANNEL_FTA;
    fe.volt=0;
    fe.ttk=1;
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
    fe.power=1;
    fe.freq=394000000;
    fe.srate=6900000;
    fe.video_pid=0x262;
    fe.audio_pid=0x26c;
    fe.qam=2;
    fe.inv=0;
  }
  ves_set_frontend(&fe);
  
  dvb->front=fe;
  
  return 0;
}

int SetSec(int power,int volt,int tone) {
  volt++;
  if (power == 0)
    volt=0;
  return fp_set_sec(volt,tone);
}

int secSetTone(struct dvb_struct *dvb, secToneMode mode) {
  int val;
  
  switch(mode) {
  case SEC_TONE_ON:
    val=1;
    break;
  case SEC_TONE_OFF:
    val=0;
    break;
  default:
    return -EINVAL;
  }
  dvb->front.ttk=val;
  SetSec(dvb->front.power,dvb->front.volt,val);
  return 0;
}

int secSetVoltage(struct dvb_struct *dvb, secVoltage voltage) {
  int power=1, volt=0;
  
  switch(voltage) {
  case SEC_VOLTAGE_LT: //WHAT's THIS FOR ??
    return -EOPNOTSUPP;
  case SEC_VOLTAGE_OFF:
    power=0;
    break;
  case SEC_VOLTAGE_13:
    volt=0;
    break;
  case SEC_VOLTAGE_18:
    volt=1;
    break;
  case SEC_VOLTAGE_13_5:
    volt=2;
    break;
  case SEC_VOLTAGE_18_5:
    volt=3;
    break;
  default:
    return -EINVAL;
  }
  dvb->front.power=power;
  dvb->front.volt=volt;
  SetSec(power,volt,dvb->front.ttk);
  return 0;
}

int dvb_open(struct dvb_device *dvbdev, int type, struct inode *inode, struct file *file)
{
  struct dvb_struct *dvb=(struct dvb_struct *) dvbdev->priv;
  switch (type)
  {
  case DVB_DEVICE_VIDEO:
    break;
//  case DVB_DEVICE_AUDIO:

  case DVB_DEVICE_SEC:
    if (file->f_flags&O_NONBLOCK)
      return -EWOULDBLOCK;
  case DVB_DEVICE_FRONTEND:
    break;
  case DVB_DEVICE_DEMUX:
    return DmxDevFilterAlloc(&dvb->dmxdev, file);
  case DVB_DEVICE_DVR:
    return DmxDevDVROpen(&dvb->dmxdev, file);
  default:
    return -EINVAL;
  }
  return 0;
}

int dvb_close(struct dvb_device *dvbdev, int type, struct inode *inode, struct file *file)
{
  struct dvb_struct *dvb=(struct dvb_struct *) dvbdev->priv;

  switch (type) {
  case DVB_DEVICE_VIDEO:
    avia_wait(avia_command(Reset));
    break;
 // case DVB_DEVICE_AUDIO:
 //   AV_Stop(dvb, RP_AUDIO);
 //   break;
 // case DVB_DEVICE_SEC:
 //   break;
  case DVB_DEVICE_FRONTEND:
    break;
  case DVB_DEVICE_DEMUX:
    return DmxDevFilterFree(&dvb->dmxdev, file);
  case DVB_DEVICE_DVR:
    return DmxDevDVRClose(&dvb->dmxdev, file);
  default:
    return -EINVAL;
  }
  return 0;
}

ssize_t dvb_read(struct dvb_device *dvbdev, int type, struct file *file, char *buf, size_t count, loff_t *ppos)
{
  struct dvb_struct *dvb=(struct dvb_struct *) dvbdev->priv;

  switch (type) {
  case DVB_DEVICE_VIDEO:
    break;
  case DVB_DEVICE_AUDIO:
    break;
  case DVB_DEVICE_DEMUX:
    return DmxDevRead(&dvb->dmxdev, file, buf, count, ppos);
  case DVB_DEVICE_DVR:
    return DmxDevDVRRead(&dvb->dmxdev, file, buf, count, ppos);
  default:
    return -EOPNOTSUPP;
  }

  return 0;
}

ssize_t dvb_write(struct dvb_device *dvbdev, int type, struct file *file, const char *buf, size_t count, loff_t *ppos)
{
  struct dvb_struct *dvb=(struct dvb_struct *) dvbdev->priv;

  switch (type) {
  case DVB_DEVICE_VIDEO:
    return -ENOSYS;
//        case DVB_DEVICE_AUDIO:
  case DVB_DEVICE_DVR:
    return DmxDevDVRWrite(&dvb->dmxdev, file, buf, count, ppos);
  default:
    return -EOPNOTSUPP;
  }
}

int dvb_ioctl(struct dvb_device *dvbdev, int type, struct file *file, unsigned int cmd, unsigned long arg)
{
  struct dvb_struct *dvb=(struct dvb_struct *) dvbdev->priv;
  void *parg=(void *)arg;
  switch (type) {
  case DVB_DEVICE_VIDEO:
  {
    if (((file->f_flags&O_ACCMODE)==O_RDONLY) && (cmd!=VIDEO_GET_STATUS))
      return -EPERM;
    switch (cmd)
    {
    case VIDEO_STOP:
      dvb->videostate.playState=VIDEO_STOPPED;
      avia_wait(avia_command(Pause, 3, 3));
      break;
    case VIDEO_PLAY:
      avia_wait(avia_command(Play, 0, 0, 0));
      dvb->videostate.playState=VIDEO_PLAYING;
      break;
    case VIDEO_FREEZE:
      dvb->videostate.playState=VIDEO_FREEZED;
      avia_wait(avia_command(Freeze, 1));
      break;
    case VIDEO_CONTINUE:
      if (dvb->videostate.playState==VIDEO_FREEZED)
      {
        dvb->videostate.playState=VIDEO_PLAYING;
        avia_wait(avia_command(Resume));
      }
      break;
    case VIDEO_SELECT_SOURCE:
    {
      if (dvb->videostate.playState==VIDEO_STOPPED)
      {
        dvb->videostate.streamSource=(videoStreamSource_t) arg;
        if (dvb->videostate.streamSource!=VIDEO_SOURCE_DEMUX)
          return -EINVAL;
        avia_command(SetStreamType, 0xB);
        avia_command(SelectStream, 0, 0);
        avia_command(SelectStream, 2, 0);
        avia_command(SelectStream, 3, 0);
      } else
        return -EINVAL;
      break;
    }
    case VIDEO_SET_BLANK:
      dvb->videostate.videoBlank=(boolean) arg;
      break;
    case VIDEO_GET_STATUS:
      if(copy_to_user(parg, &dvb->videostate, sizeof(struct videoStatus)))
        return -EFAULT;
      break;
    case VIDEO_GET_EVENT:
      return -ENOTSUPP;
    case VIDEO_SET_DIPLAY_FORMAT:
      return -ENOTSUPP;
    case VIDEO_STILLPICTURE:
      return -ENOTSUPP;
    case VIDEO_FAST_FORWARD:
      return -ENOTSUPP;
    case VIDEO_SLOWMOTION:
      return -ENOTSUPP;
    default:
      return -ENOIOCTLCMD;
    }
    return 0;
  }
//  case DVB_DEVICE_AUDIO:
  case DVB_DEVICE_FRONTEND:
  {
    switch (cmd)
    {
    case OST_SELFTEST:
      break;
    case OST_SET_POWER_STATE:
      return -ENOSYS;
      break;
    case OST_GET_POWER_STATE:
      return -ENOSYS;
      break;
    case FE_READ_STATUS:
    {
      feStatus stat;

      ves_get_frontend(&dvb->front);
      stat=0;
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

      ves_get_frontend(&dvb->front);
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
      ves_get_frontend(&dvb->front);
      if (!dvb->front.power)
        return -ENOSIGNAL;
      signal=dvb->front.agc;
      if (copy_to_user(parg, &signal, sizeof(signal)))
        return -EFAULT;
      break;
    }
    case FE_READ_SNR:
      return -ENOSYS;
    case FE_READ_UNCORRECTED_BLOCKS:
      return -ENOSYS;
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
        printk ("frontend.c: fe.freq != val...\n");
        tuner_setfreq(dvb, val);
        dvb->front.freq=val;
      }
      val=para.SymbolRate;
      if ((dvb->front.srate!=val) || (dvb->front.fec!=fectab[para.FEC_inner]))
      {
        dvb->front.srate=val;
        dvb->front.fec=fectab[para.FEC_inner];
        ves_set_frontend(&dvb->front);
        
      }
      printk(" TODO: adding QPSK event!\n");
      break;
    }
    case QPSK_GET_EVENT:
      return -ENOTSUPP;
    case QPSK_FE_INFO:
    {
      struct qpskFrontendInfo feinfo;
    
      feinfo.minFrequency=500;  //KHz?
      feinfo.maxFrequency=2100000;
      feinfo.minSymbolRate=500000;
      feinfo.maxSymbolRate=30000000;
      feinfo.hwType=0;    //??
      feinfo.hwVersion=0; //??
      if(copy_to_user(parg, &feinfo, sizeof(feinfo)))
        return -EFAULT;
      break;
    }
    case QPSK_WRITE_REGISTER:
      return -ENOTSUPP;
    case QPSK_READ_REGISTER:
      return -ENOTSUPP;
    default:
      printk("ost_frontend_ioctl: UNEXPECTED cmd: %d.\n", cmd);
    }
    return 0;
    break;
  }
  case DVB_DEVICE_SEC:
        switch(cmd) {
    case SEC_GET_STATUS:
      {
	struct secStatus status;

	status.busMode=SEC_BUS_IDLE;

	if (!dvb->front.power)
	  status.busMode=SEC_BUS_OFF;
	
	status.selVolt=dvb->front.volt;
	
	status.contTone=dvb->front.ttk;
	if(copy_to_user(parg,&status, sizeof(status)))
	  return -EFAULT;
      }
      break;
	case SEC_RESET_OVERLOAD:
	  {
	    if ((file->f_flags&O_ACCMODE)==O_RDONLY)
	      return -EPERM;
	    dvb->front.power=1;
	    SetSec(dvb->front.power,dvb->front.volt,dvb->front.ttk);
	    return 0;
	  }
	  break;
	case SEC_SEND_SEQUENCE:
	  {
	    if ((file->f_flags&O_ACCMODE)==O_RDONLY)
	      return -EPERM;
	    return -ENOSYS; // TODO
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
  case DVB_DEVICE_DEMUX:
    return DmxDevIoctl(&dvb->dmxdev, file, cmd, arg);
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
//  case DVB_DEVICE_VIDEO:
//  case DVB_DEVICE_AUDIO:
  default:
    return -EOPNOTSUPP;
  }
  return 0;
}

static int dvb_register(struct dvb_struct *dvb)
{
//  int i;
  int result;
  struct dvb_device *dvbd=&dvb->dvb_dev;

  dvb->num=0;
  dvb->dmxdev.filternum=32;
  dvb->dmxdev.demux=&dvb->hw_demux.dmx;
  dvb->dmxdev.capabilities=0;
  printk("DmxDevInit.\n");
  DmxDevInit(&dvb->dmxdev);
  
//  for (i=0; i<32; i++)
//    dvb->handle2filter[i]=NULL;

  init_waitqueue_head(&dvb->qpsk.eventq);
  spin_lock_init (&dvb->qpsk.eventlock);
  dvb->qpsk.eventw=dvb->qpsk.eventr=0;
  dvb->qpsk.overflow=0;
//  dvb->secbusy=0;

  // audiostate

  // videostate
  printk("frontend init.\n");
  frontend_init(dvb);
  
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
    return result;

  memcpy(dvb->hw_demux_id, "demux0", 7);

  dvb->hw_demux_id[5]=dvb->num+0x30;

  printk("gtx/dmx init\n");
  GtxDmxInit(&dvb->hw_demux, (void*)dvb, dvb->hw_demux_id, "C-Cube", "Avia GTX");

  return 0;
}

void dvb_unregister(struct dvb_struct *dvb)
{
	GtxDmxCleanup(&dvb->hw_demux, (void*)dvb, dvb->hw_demux_id );

	return dvb_unregister_device(&dvb->dvb_dev);
}

int __init dvb_init_module(void)
{
  return dvb_register(&dvb);
}


void __exit dvb_cleanup_module ()
{
  return dvb_unregister(&dvb);
}

module_init ( dvb_init_module );
module_exit ( dvb_cleanup_module );
