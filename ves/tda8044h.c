/* 
 *   $Id: tda8044h.c,v 1.11 2002/08/11 19:48:15 happydude Exp $
 *   
 *   tda8044h.c - Philips TDA8044H (d-box 2 project) 
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2001 Felix "tmbinc" Domke (tmbinc@gmx.net)
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
 *   $Log: tda8044h.c,v $
 *   Revision 1.11  2002/08/11 19:48:15  happydude
 *   disable debug messages
 *
 *   Revision 1.10  2002/05/18 21:01:48  derget
 *   fixx wegem lock problem von zapit (untested)
 *   sync wurde nie richtig gelesen !
 *   danke an TheTobe
 *
 *   Revision 1.9  2002/04/24 12:08:38  obi
 *   made framing byte hack nicer
 *
 *   Revision 1.8  2002/04/20 18:23:16  obi
 *   added raw diseqc command
 *
 *   Revision 1.7  2002/02/24 15:32:06  woglinde
 *   new tuner-api now in HEAD, not only in branch,
 *   to check out the old tuner-api should be easy using
 *   -r and date
 *
 *   Revision 1.4.2.3  2002/01/22 23:44:56  fnbrd
 *   Id und Log reingemacht.
 *
 *   Revision 1.4.2.2  2002/01/22 23:41:41  fnbrd
 *   changed to new tuning_api
 *
 *   Revision 1.5  2002/01/22 22:35:26  tmbinc
 *   added new tuning api (branch: new_tuning_api)
 *

 *   Revision 1.4  2001/11/02 02:33:57  TripleDES
 *   fixed diseqc(tm) problem
 *
 *   Revision 1.3  2001/11/01 02:17:00  TripleDES
 *   added DiSEqC(tm) support - no minidiseqc up to now
 *
 *   Revision 1.2  2001/07/07 23:39:38  tmbinc
 *   fixes
 *
 *   Revision 1.1  2001/04/19 22:48:10  tmbinc
 *   philips support (sat, tda8044h), ost/dvb.c fix to call demod->init() now.
 *
 *
 *   $Revision: 1.11 $
 *
 */

/* ---------------------------------------------------------------------- */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <asm/io.h>
#include <linux/i2c.h>

#include <dbox/dvb_frontend.h>
#include <dbox/fp.h>
#include <ost/sec.h>

#ifdef MODULE
MODULE_DESCRIPTION("");
MODULE_AUTHOR("Felix Domke");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
MODULE_PARM(debug,"i");
#endif

static DECLARE_MUTEX(i2c_mutex);

static int tda_set_sec(struct i2c_client *client, int power, int tone);
static int tda_set_freq(struct i2c_client *client, int freq);
static int tda_send_diseqc(struct i2c_client *client, u8 *cmd,unsigned int len);


static int debug = 0;
#define dprintk	if (debug) printk

static int instances=0;

static struct i2c_driver dvbt_driver;

static int attach_adapter(struct i2c_adapter *adap);
static int detach_client(struct i2c_client *client);
static int dvb_command(struct i2c_client *client, unsigned int cmd, void *arg);
static void inc_use (struct i2c_client *client);
static void dec_use (struct i2c_client *client);

static struct i2c_driver dvbt_driver = {
	"TDA8044H DVB DECODER",
	DVB_DEMOD_TDA8044,
	I2C_DF_NOTIFY,
	attach_adapter,
	detach_client,
	dvb_command,
	inc_use,
	dec_use,
};


static struct i2c_client client_template = {
	"TDA8044H",
	I2C_DRIVERID_TDA8044,
	0,
	(0xD0 >> 1),
	NULL,
	&dvbt_driver,
	NULL
};


static struct i2c_client tuner = {
	"TUNER",
	I2C_DRIVERID_TDA8044+1,
	0,
	(0xC6 >> 1),
	NULL,
	&dvbt_driver,
	NULL
};




#define TDA_INTERRUPT 14
static void tda_interrupt(int irq, void *vdev, struct pt_regs * regs);

static void tda_task(void*);

struct tq_struct tda_tasklet=
{
	routine: tda_task,
	data: 0
};

u8 Init8044Tab[] =
{
	0x06, 	//	output active, no PD reset, no AFC1 reset, no Preset logic, INTready, standby, no i2c reset
	0x00, 0x6f, 0xb5, 0x86, 0x20, 
	0x00, 	// Sigma Delta
	0xea,		// possible puncturing rates
	0x30, 0x42, 0x98, 0x68, 0x70, 0x42, 0x99, 0x58,
	0x95, 0x10, 0xf5, 0xe7, 0x93, 0x0b, 0x15, 0x68,
	0x9a, 0x90, 0x61, 0x80, 0x00, 0xe0, 0x40, 0x00,
	0x0f, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00
};

struct tda8044
{
	int inversion, srate, sync, fec;
	int loopopen;
	
	int power, tone;
	dvb_front_t frontend;
};

/* ---------------------------------------------------------------------- */

int writereg(struct i2c_client *client, u8 reg, u8 data)
{
	int ret;
	u8 msg[] = {0x1E, 0x00};

	if (down_interruptible(&i2c_mutex))
		return -EAGAIN;

	msg[0]=reg; msg[1]=data;
	ret=i2c_master_send(client, msg, 2);
	if (ret!=2)
		dprintk("tda8044h.o: writereg error\n");
	mdelay(10);
	up(&i2c_mutex);
	return ret;
}

/* ---------------------------------------------------------------------- */

u8 readreg(struct i2c_client *client, u8 reg)
{
	struct i2c_adapter *adap=client->adapter;
	unsigned char mm1[] = {0x1e};
	unsigned char mm2[] = {0x00};
	struct i2c_msg msgs[2];

	if (down_interruptible(&i2c_mutex))
		return -EAGAIN;

	msgs[0].flags=0;
	msgs[1].flags=I2C_M_RD;
	msgs[0].addr=msgs[1].addr=client->addr;
	mm1[0]=reg;
	msgs[0].len=1; msgs[1].len=1;
	msgs[0].buf=mm1; msgs[1].buf=mm2;
	i2c_transfer(adap, msgs, 2);
	up(&i2c_mutex);

	return mm2[0];
}

/* ---------------------------------------------------------------------- */

int init(struct i2c_client *client)
{
	struct tda8044 *tda=(struct tda8044*)client->data;
	int i;
	
	for (i=0; i<sizeof(Init8044Tab); i++)
		if (writereg(client, i, Init8044Tab[i])<0)
			return -1;
	
	udelay(10000);
	writereg(client, 0xF, 0x50);
	writereg(client, 0x20, 0x8F);
	writereg(client, 0x20, 0xBF);
	writereg(client, 0, 4);
	writereg(client, 0, 0xC);
	udelay(10000);
	Init8044Tab[0]=4;
	Init8044Tab[0xF]=0x50;
	Init8044Tab[0x1F]=0x7F;		// interrupt selection

	for (i=0; i<sizeof(Init8044Tab); i++)
		if (writereg(client, i, Init8044Tab[i])<0)
			return -1;
	
	tda->inversion=0;
	tda->srate=0;
	return 0;
}

static void SetSymbolrate(struct i2c_client *client, u32 Symbolrate, int FEC)
{
	struct tda8044 *tda=(struct tda8044*)client->data;
	__u8 srate[16];
	int i;
//	long long int ratio=0x2EE0000000;
		
	i2c_master_send(client, "\x00\x04", 3);
	srate[0]=1;
	srate[1]=0;			// no differential encoding, spectral inversion unknown, QPSK encoding

//	ratio/=Symbolrate;
	
//	srate[2]=(ratio>>16)&0xFF;
//	srate[3]=(ratio>>8)&0xFF;
//	srate[4]=(ratio)&0xFF;
//	srate[5]=0x22;

	if (Symbolrate==21997000)
		Symbolrate=22000000;
	
	// super-billig-calc:
	if (Symbolrate==27500000)
		memcpy(srate+2, "\x6F\xB5\x87\x22", 4);
	else if (Symbolrate==22000000)
		memcpy(srate+2, "\x8b\xa2\xe9\x23", 4);
	else
		dprintk("unsupported symbolrate %d! please fix driver!\n", Symbolrate);
	
	srate[6]=0;									// sigma delta
	if (FEC)
		dprintk("fec: %02x\n", FEC);
	switch (FEC)
	{
	case FEC_AUTO:
	case FEC_NONE:
		srate[7]=0xFF;
		break;
	case FEC_1_2:
		srate[7]=0x80;
		break;
	case FEC_2_3:
		srate[7]=0x40;
		break;
	case FEC_3_4:
		srate[7]=0x20;
		break;
	case FEC_5_6:
		srate[7]=0x08;
		break;
	case FEC_7_8:
		srate[7]=0x02;
		break;
	}
	srate[8]=0x30;							// carrier lock threshold
	srate[9]=0x42;							// ??
	srate[10]=0x98;							// ??
	srate[11]=0x28;							// PD loop opened
	srate[12]=0x30;							// ??
	srate[13]=0x42;
	srate[14]=0x99;
	srate[15]=0x50;
	if (i2c_master_send(client, srate, 16)!=16)
		printk("tda8044h.o: writeregs failed!\n");

	for (i=0; i<16; i++)
		dprintk("%02x ", srate[i]);
	dprintk("\n");
	i2c_master_send(client, "\x17\x68\x9a", 3);
	dprintk("17 68 9a\n");
	i2c_master_send(client, "\x22\xf9", 2);				// SCPC
	dprintk("22 f9\n");
	tda->loopopen=1;
}

int attach_adapter(struct i2c_adapter *adap)
{
	struct tda8044 *tda;
	struct i2c_client *client;
	
	client_template.adapter=adap;
	
	if (writereg(&client_template, 0x89, 00)<0)
		return -EINVAL;

	if (readreg(&client_template, 0)!=4)
	{
		printk("no TDA8044H found!\n!");
		return -EINVAL;
	}

	if (!(client=kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
		return -ENOMEM;
	
	memcpy(client, &client_template, sizeof(struct i2c_client));
	
	client->data=tda=kmalloc(sizeof(struct tda8044), GFP_KERNEL);
	if (!tda)
	{
		kfree(client);
		return -ENOMEM;
	}
	
	tda->frontend.type=DVB_S;
	tda->frontend.capabilities=-1; // kann alles
	tda->frontend.i2cbus=adap;
	
	tda->frontend.demod=client;
	tda->frontend.demod_type=DVB_DEMOD_TDA8044;
	
	printk("tda8044h.o: attaching TDA8044H at 0x%02x\n", (client->addr)<<1);
	i2c_attach_client(client);
	tuner.adapter=adap;
	if (register_frontend(&tda->frontend))
		printk("tda8044h.o: can't register demod.\n");

	tda_tasklet.data = (void*)client;

	if (request_8xxirq(TDA_INTERRUPT, tda_interrupt, SA_ONESHOT, "tda8044h", NULL) != 0)
	{
		i2c_del_driver(&dvbt_driver);
		dprintk("tda8044h.o: can't request interrupt\n");
		return -EBUSY;
	}

	instances++; 
	return 0;
}

int detach_client(struct i2c_client *client)
{
	instances--;
	printk("tda8044h.o: detach_client\n");
	unregister_frontend(&((struct tda8044*)client->data)->frontend);
	i2c_detach_client(client);
	kfree(client->data);
	kfree(client);
	free_irq(TDA_INTERRUPT, NULL);
	return 0;
}

static int dvb_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	struct tda8044 *tda=(struct tda8044*)client->data;
	
	switch (cmd)
	{
	case FE_READ_STATUS:
	{
		FrontendStatus *status=(FrontendStatus*)arg;
		
		int sync=readreg(client, 2);
		
		*status=0;
		
		if (sync&1)
			*status|=FE_HAS_SIGNAL;
		if (sync&2)
			*status|=FE_HAS_CARRIER;
		if (sync&4)
			*status|=FE_HAS_VITERBI;
		if (sync&16)
			*status|=FE_HAS_SYNC;
		if ((sync&31)==31)
			*status|=FE_HAS_LOCK;
			
		if (tda->inversion)
			*status|=FE_SPECTRUM_INV;
		break;
	}
	case FE_READ_BER:
	{
		u32 *ber=(u32 *) arg;
		*ber=readreg(client, 0xb)<<16;
		*ber|=readreg(client, 0xc)<<8;
		*ber|=readreg(client, 0xd);
		break;
	}
	case FE_READ_SIGNAL_STRENGTH:
	{
		s32 *signal=(s32 *) arg;

		*signal=0xFF-readreg(client, 1);
		break;
	}
	case FE_READ_SNR:
	{
		s32 *snr=(s32 *) arg;
		*snr=readreg(client, 8)<<8;
		*snr=20000000+(10-(*snr>>8))*20000000/160;
		break;
	}
	case FE_READ_UNCORRECTED_BLOCKS:
	{
		u32 *ublocks=(u32 *) arg;
		*ublocks=readreg(client, 0xF);
		break;
	}
	case  FE_READ_AFC:
	{
		s32 *afc=(s32 *) arg;
		*afc=readreg(client, 5)<<16;
		*afc|=readreg(client, 6)<<8;
		*afc|=readreg(client, 7);
		break;
	}
	case FE_GET_INFO:
	{
		FrontendInfo *feinfo=(FrontendInfo *) arg;

		feinfo->type=FE_QPSK;
		feinfo->minFrequency=500; 
		feinfo->maxFrequency=2700000;
		feinfo->minSymbolRate=500000;
		feinfo->maxSymbolRate=30000000;
		feinfo->hwType=0;    
		feinfo->hwVersion=0;
		break;
	}
	case FE_WRITEREG:
	{
		u8 *msg = (u8 *) arg;
		writereg(client, msg[0], msg[1]);
		break;
	}
	case FE_READREG:
	{
		u8 *msg = (u8 *) arg;
		msg[1]=readreg(client, msg[0]);

		break;
	}
	case FE_INIT:
	{
		init(client);
		break;
	}
	case FE_SET_FRONTEND:
	{
		FrontendParameters *param = (FrontendParameters *) arg;

		// param->Inversion);
		tda->srate=param->u.qpsk.SymbolRate;
		dprintk("setting symbolrate: %d\n", tda->srate);
		tda->fec=param->u.qpsk.FEC_inner;
		SetSymbolrate(client, tda->srate, tda->fec);
		dprintk("0b 68 70\n");
		i2c_master_send(client, "\x0b\x68\x70", 3);		// stop sweep, close loop
		break;
	}
	case FE_RESET:
	{
		break;
	}
	case FE_SEC_SET_TONE:
	{
		secToneMode mode=(secToneMode)arg;
		tda->tone=(mode==SEC_TONE_ON)?1:0;
		tda_set_sec(client, tda->power, tda->tone);
		break;
	}
	case FE_SEC_SET_VOLTAGE:
	{
		secVoltage volt=(secVoltage)arg;
		switch (volt)
		{
		case SEC_VOLTAGE_OFF:
			tda->power=0;
			break;
		case SEC_VOLTAGE_LT:
			tda->power=-2;
			break;
		case SEC_VOLTAGE_13:
			tda->power=1;
			break;
		case SEC_VOLTAGE_13_5:
			tda->power=2;
			break;
		case SEC_VOLTAGE_18:
			tda->power=3;
			break;
		case SEC_VOLTAGE_18_5:
			tda->power=4;
			break;
		default:
			printk("invalid voltage\n");
		}
		tda_set_sec(client, tda->power, tda->tone);
		break;
	}
	case FE_SEC_MINI_COMMAND:
	{
		printk("warning, minidiseqc nyi\n");
		return 0;
	}
	case FE_SEC_COMMAND:
	{
		struct secCommand *command=(struct secCommand*)arg;
		switch (command->type) {
		case SEC_CMDTYPE_DISEQC:
		{
			unsigned char msg[SEC_MAX_DISEQC_PARAMS+3];
			msg[0]=0xE0;
			msg[1]=command->u.diseqc.addr;
			msg[2]=command->u.diseqc.cmd;
			memcpy(msg+3, command->u.diseqc.params, command->u.diseqc.numParams);
			tda_send_diseqc(client, msg, command->u.diseqc.numParams+3);
			break;
		}
                case SEC_CMDTYPE_DISEQC_RAW:
		{
			unsigned char msg[SEC_MAX_DISEQC_PARAMS+3];
			msg[0]=command->u.diseqc.cmdtype;
			msg[1]=command->u.diseqc.addr;
			msg[2]=command->u.diseqc.cmd;
			memcpy(msg+3, command->u.diseqc.params, command->u.diseqc.numParams);
			tda_send_diseqc(client, msg, command->u.diseqc.numParams+3);
			break;
		}
		default:
			return -EINVAL;
		}
		break;
		return 0;
	}
	case FE_SEC_GET_STATUS:
	{
		struct secStatus *status=(struct secStatus*)arg;
		break;
	}
	case FE_SETFREQ:
	{
		u32 freq=*(u32*)arg;
		tda_set_freq(client, freq);
		break;
	}
	default:
		return -1;
	}
	
	return 0;
}

static int tda_set_freq(struct i2c_client *client, int freq)
{
	int tries=10;
	u8 msg[4]={0, 0, 0x81, 0x60};
	
	dprintk("setting freq %d\n", freq);
		// semaphore, irgendwie
	writereg(client, 0x1C, 0x80);
	
	msg[0]=(freq/1000)>>8;
	msg[1]=(freq/1000)&0xFF;

	if (i2c_master_send(&tuner, msg, 4)!=4)
		dprintk("tda8044h.o: writereg error\n");

	writereg(client, 0x1C, 0x0);
	
	while (tries--)
	{
		udelay(10*1000);
		writereg(client, 0x1C, 0x80);
	
		if (i2c_master_recv(&tuner, msg, 1)!=1)
			dprintk("tda8044h.o: read tuner error\n");
	
		writereg(client, 0x1C, 0x0);
		if (msg[0]&0x40)		// in-lock flag
		{
			dprintk("tuner locked\n");
			return 0;
		}
	}
	printk("tuner: couldn't acquire lock.\n");
	return -1;
}

static int tda_set_sec(struct i2c_client *client, int power, int tone)
{
	int powerv;
	dprintk("tda8044: setting SEC, power: %d, tone: %d\n", power, tone);
	switch (power)
	{
	case 0:
		powerv=0x0F; break;
	case 1:		// 13V
	case 2:		// 14V
		powerv=0x3F; break;
	case 3:		// 18V
	case 4:		// 19V
		powerv=0xBF; break;
	default:
		powerv=0; break;
	}
	writereg(client, 0x20, powerv);
	writereg(client, 0x29, tone?0x80:0);
	return 0;
}

static int tda_send_diseqc(struct i2c_client *client, u8 *cmd,unsigned int len)
{
	int i,a;

	if(len>6 || len<3)
	{
		printk("wrong message size.\n");
		return 0;
	}
	for(a=0;a<2;a++)
	{
		writereg(client,0x29,8+(len-3));
		for(i=0;i<len;i++) writereg(client,0x23+i,cmd[i]);
		writereg(client,0x29,0x0C+(len-3));
	}
	return 0;
}

static int tda_sec_status(void)
{
	return 0;
}

static void tda_interrupt(int irq, void *vdev, struct pt_regs * regs)
{
	schedule_task(&tda_tasklet);
}

static void tda_task(void *data)
{
	struct i2c_client *client=(struct i2c_client *)data;
	struct tda8044 *tda=(struct tda8044*)client->data;
	int sync;
	static int lastsync;
	writereg(client, 0, 4);
	enable_irq(TDA_INTERRUPT);

	sync=readreg(client, 2);
	
	if (tda->loopopen && (sync&2) && (!(tda->sync&2)))
	{
		dprintk("loop open, lock acquired, closing loop.\n");
		// i2c_master_send(client, "\x0b\x68\x70", 3);		// stop sweep, close loop
		tda->loopopen=0;
	}
	
	if (sync != lastsync);
		dprintk("sync: %02x\n", sync);
	lastsync=sync;
	
	if ((sync == 0x1F) && (tda->sync != 0x1F))
	{
		int val=readreg(client, 0xE);
		tda->inversion=!!(val&0x80);
		tda->fec=val&7;
		dprintk("acquired full sync, found FEC: %d/%d, %sspectral inversion\n", tda->fec, tda->fec+1, tda->inversion?"":"no ");
	}

	if (sync&0x20)
		tda->sync=0;
	else
		tda->sync=sync&0x1F;
}

static void inc_use (struct i2c_client *client)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void dec_use (struct i2c_client *client)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}


#ifdef MODULE
int init_module(void) {
	int res;
	
	if ((res = i2c_add_driver(&dvbt_driver))) 
	{
		printk("TDA8044H: Driver registration failed, module not inserted.\n");
		return res;
	}
	if (!instances)
	{
		printk("TDA8044H: not found.\n");
		i2c_del_driver(&dvbt_driver);
		return -EBUSY;
	}
	return 0;
}

void cleanup_module(void)
{
	int res;
	
	if ((res = i2c_del_driver(&dvbt_driver))) 
	{
		printk("TDA8044H: Driver deregistration failed, "
					 "module not removed.\n");
	}
	dprintk("TDA8044H: cleanup\n");
}
#endif

