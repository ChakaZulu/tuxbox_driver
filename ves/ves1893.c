/* 
   $Id: ves1893.c,v 1.22 2002/04/20 18:23:16 obi Exp $

    VES1893A - Single Chip Satellite Channel Receiver driver module
               used on the the Siemens DVB-S cards

    Copyright (C) 1999 Convergence Integrated Media GmbH <ralph@convergence.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the

    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Log: ves1893.c,v $
    Revision 1.22  2002/04/20 18:23:16  obi
    added raw diseqc command

    Revision 1.21  2002/04/13 05:30:33  obi
    added FE_READ_UNCORRECTED_BLOCKS

    Revision 1.20  2002/04/09 23:50:12  kwon
    - mini-DiSEqC works correctly now

    Revision 1.19  2002/04/04 06:00:29  obi
    partially implemented FE_SEC_GET_STATUS

    Revision 1.18  2002/02/24 15:32:07  woglinde
    new tuner-api now in HEAD, not only in branch,
    to check out the old tuner-api should be easy using
    -r and date

    Revision 1.14.2.3  2002/01/22 23:44:56  fnbrd
    Id und Log reingemacht.

    
*/    

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <asm/io.h>
#include <linux/i2c.h>

#include <dbox/dvb_frontend.h>
#include <dbox/fp.h>
#include <ost/sec.h>

#ifdef MODULE
MODULE_DESCRIPTION("");
MODULE_AUTHOR("Ralph Metzler");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
MODULE_PARM(debug,"i");
#endif

static int debug = 0;
#define dprintk	if (debug) printk

static struct i2c_driver dvbt_driver;
static struct i2c_client client_template;

#define DBOX

static u8 Init1893Tab[] =
{
#ifndef DBOX
	0x01, 0xA4, 0x35, 0x81, 0x2A, 0x0d, 0x55, 0xC4,
	0x09, 0x69, 0x00, 0x86, 0x4c, 0x28, 0x7F, 0x00,
	0x00, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x80, 0x00, 0x31, 0xb0, 0x14, 0x00, 0xDC, 0x20,
	0x81, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x55, 0x00, 0x00, 0x7f, 0x00
#else
	0x01, 0x94, 0x00, 0x80, 0x80, 0x6a, 0x9b, 0xab,
	0x09, 0x69, 0x00, 0x86, 0x4c, 0x28, 0x7F, 0x00,
	0x00, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
	0x80, 0x00, 0x21, 0xb0, 0x14, 0x00, 0xDC, 0x00,
	0x81, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x55, 0x03, 0x00, 0x7f, 0x00
#endif
};

static u8 Init1893WTab[] =
{
	1,1,1,1,1,1,1,1, 1,1,0,0,1,1,0,0,
	0,1,0,0,0,0,0,0, 1,0,1,1,0,0,0,1,
	1,1,1,0,0,0,0,0, 0,0,1,1,0,0,0,0,
	1,1,1,0,1,1
};

struct ves1893 {
	u32 srate;
	u8 ctr;
	u8 fec;
	u8 inv;
	
	int power, tone;
	
	dvb_front_t frontend;
};

static int writereg(struct i2c_client *client, int reg, int data)
{
	int ret;
	unsigned char msg[] = {0x00, 0x1f, 0x00};
	
	msg[1]=reg; msg[2]=data;
	ret=i2c_master_send(client, msg, 3);
	if (ret!=3) 
		printk("writereg error\n");
	return ret;
}

static u8 readreg(struct i2c_client *client, u8 reg)
{
	struct i2c_adapter *adap=client->adapter;
	unsigned char mm1[] = {0x00, 0x1e};
	unsigned char mm2[] = {0x00};
	struct i2c_msg msgs[2];
	
	msgs[0].flags=0;
	msgs[1].flags=I2C_M_RD;
	msgs[0].addr=msgs[1].addr=client->addr;
	mm1[1]=reg;
	msgs[0].len=2; msgs[1].len=1;
	msgs[0].buf=mm1; msgs[1].buf=mm2;
	i2c_transfer(adap, msgs, 2);
	
	return mm2[0];
}

static int init(struct i2c_client *client)
{
	int i;
	struct ves1893 *ves=(struct ves1893 *) client->data;
	
	dprintk("VES1893: init chip\n");

	if (writereg(client, 0, 0)<0)
		printk("VES1893: send error\n");
	for (i=0; i<54; i++)
		if (Init1893WTab[i])
			writereg(client, i, Init1893Tab[i]);
	ves->ctr=Init1893Tab[0x1f];
	ves->srate=0;
	ves->fec=9;
	ves->inv=0;

	return 0;
}

static inline void ddelay(int i) 
{
	current->state=TASK_INTERRUPTIBLE;
	schedule_timeout((HZ*i)/100);
}

static void ClrBit1893(struct i2c_client *client)
{
	dprintk("clrbit1893\n");
	ddelay(2);
	writereg(client, 0, Init1893Tab[0] & 0xfe);
	writereg(client, 0, Init1893Tab[0]);
	writereg(client, 3, 0);
	writereg(client, 3, Init1893Tab[3]);
}

static int SetInversion(struct i2c_client *client, int inversion)
{
	struct ves1893 *ves=(struct ves1893 *) client->data;
	u8 val;

	if (inversion == ves->inv) 
		return 0;
	ves->inv=inversion;
	switch (inversion) {
	case INVERSION_OFF:
		val=0xc0;
		break;
	case INVERSION_ON:
		val=0x80;
		break;
	default:
	case INVERSION_AUTO:
		val=0x40;
		break;
	}
	writereg(client, 0x0c, 
		 (Init1893Tab[0x0c]&0x3f)|val);
	return 0;
}


static int SetFEC(struct i2c_client *client, u8 fec)
{
	struct ves1893 *ves=(struct ves1893 *) client->data;
	
	if (fec>=8) 
		fec=8;
	if (ves->fec==fec)
		return 0;
	ves->fec=fec;
	return writereg(client, 0x0d, ves->fec);
}

static int SetSymbolrate(struct i2c_client *client, u32 srate, int doclr)
{
	struct ves1893 *ves=(struct ves1893 *) client->data;
	u32 BDR;
	u32 ratio;
  	u8  ADCONF, FCONF, FNR;
	u32 BDRI;
	u32 tmp;

	if (ves->srate==srate) {
		if (doclr)
			ClrBit1893(client);
		return 0;
	}
	dprintk("setsymbolrate %d\n", srate);

#ifndef DBOX
#define XIN (90106000UL)
#else
#define XIN (91000000UL)
#endif

	if (srate>XIN/2)
		srate=XIN/2;
	if (srate<500000)
		srate=500000;
	ves->srate=srate;
	
#define MUL (1UL<<26)
#define FIN (XIN>>4)
	tmp=srate<<6;
	ratio=tmp/FIN;
	
	tmp=(tmp%FIN)<<8;
	ratio=(ratio<<8)+tmp/FIN;
	
	tmp=(tmp%FIN)<<8;
	ratio=(ratio<<8)+tmp/FIN;
	
	FNR = 0xFF;

	if (ratio < MUL/3)	   FNR = 0;
	if (ratio < (MUL*11)/50)     FNR = 1;
	if (ratio < MUL/6)	   FNR = 2;
	if (ratio < MUL/9)	   FNR = 3;
	if (ratio < MUL/12)	  FNR = 4;
	if (ratio < (MUL*11)/200)    FNR = 5;
	if (ratio < MUL/24)	  FNR = 6;
	if (ratio < (MUL*27)/1000)   FNR = 7;
	if (ratio < MUL/48)	  FNR = 8;
	if (ratio < (MUL*137)/10000) FNR = 9;

	if (FNR == 0xFF) {
		ADCONF = 0x89;
		FCONF  = 0x80;
		FNR	= 0;
	} else {
		ADCONF = 0x81;
		FCONF  = 0x88 | (FNR >> 1) | ((FNR & 0x01) << 5);
	}

	BDR = ((  (ratio<<(FNR>>1))  >>4)+1)>>1;
	BDRI = (  ((FIN<<8) / ((srate << (FNR>>1))>>2)  ) +1 ) >> 1;

	dprintk("FNR= %d\n", FNR);
	dprintk("ratio= %08x\n", ratio);
	dprintk("BDR= %08x\n", BDR);
	dprintk("BDRI= %02x\n", BDRI);

	if (BDRI > 0xFF)
		BDRI = 0xFF;

	writereg(client, 6, 0xff&BDR);
	writereg(client, 7, 0xff&(BDR>>8));
	writereg(client, 8, 0x0f&(BDR>>16));

	writereg(client, 9, BDRI);
	writereg(client, 0x20, ADCONF);
	writereg(client, 0x21, FCONF);

	if (srate<6000000) 
		writereg(client, 5, Init1893Tab[0x05] | 0x80);
	else
		writereg(client, 5, Init1893Tab[0x05] & 0x7f);

	writereg(client, 0, 0);
	writereg(client, 0, 1);

	if (doclr)
	  ClrBit1893(client);
	return 0;
}

static int attach_adapter(struct i2c_adapter *adap)
{
	struct ves1893 *ves;
	struct i2c_client *client;
	
	client_template.adapter=adap;
	
/*	if (i2c_master_send(&client_template, NULL,0))
	{
		printk("send failed.\n");
		return -1;
	} */
	
	client_template.adapter=adap;
	
	if ((readreg(&client_template, 0x1e)&0xf0)!=0xd0)
		return -1;
	
	if (NULL == (client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
		return -ENOMEM;
	memcpy(client, &client_template, sizeof(struct i2c_client));
	
	client->data=ves=kmalloc(sizeof(struct ves1893),GFP_KERNEL);
	if (ves==NULL) {
		kfree(client);
		return -ENOMEM;
	}

	i2c_attach_client(client);
	init(client);

	printk("VES1893: attaching VES1893 at 0x%02x ", (client->addr)<<1);
	printk("to adapter %s\n", adap->name);
	
	ves->frontend.type=DVB_S;
	ves->frontend.capabilities=0;	// kann nix
	ves->frontend.i2cbus=adap;
	
	ves->frontend.demod=client;
	ves->frontend.demod_type=DVB_DEMOD_VES1893;
	
	register_frontend(&ves->frontend);
	return 0;
}

static int detach_client(struct i2c_client *client)
{
	printk("VES1893: detach_client\n");
	unregister_frontend(&((struct ves1893*)client->data)->frontend);
	i2c_detach_client(client);
	kfree(client->data);
	kfree(client);
	return 0;
}


static const uint8_t fectab[8]={8,0,1,2,4,6,0,8};
static const uint8_t fectab2[9]={1,2,3,-1,4,-1,5,-1,0};


static int dvb_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	struct ves1893 *ves=(struct ves1893 *) client->data;
		
	switch (cmd) 
	{
	case FE_READ_STATUS:
	{
		FrontendStatus *status=(FrontendStatus *) arg;
		int sync;

		*status=0;

		sync=readreg(client,0x0e);
		if (sync&1)
			*status|=FE_HAS_SIGNAL;
		if (sync&2)
			*status|=FE_HAS_CARRIER;
		if (sync&4)
			*status|=FE_HAS_VITERBI;
		if (sync&8)
			*status|=FE_HAS_SYNC;
		if ((sync&0x1f)==0x1f)
			*status|=FE_HAS_LOCK;

		sync=readreg(client,0x0f);
		if (sync&2)
			*status|=FE_SPECTRUM_INV;
		break;
	}
	case FE_READ_BER:
	{
		u32 *ber=(u32 *) arg;

		*ber = readreg(client,0x15);
		*ber|=(readreg(client,0x16)<<8);
		*ber|=(readreg(client,0x17)<<16);
		*ber*=10;
		break;
	}
	case FE_READ_SIGNAL_STRENGTH:
	{
		s32 *signal=(s32 *) arg;

		*signal=0xff-readreg(client,0x0b);
		break;
	}
	case FE_READ_SNR:
	{
		s32 *snr=(s32 *) arg;

		*snr=(readreg(client,0x1c)<<8);
		*snr=20000000+(10-(*snr>>8))*20000000/160;
		break;
	}
	case FE_READ_UNCORRECTED_BLOCKS: 
	{
		u32 *ublocks=(u32 *) arg;
		*ublocks = readreg(client,0x18) & 0x7f;
		writereg(client, 0x18, 0x00); // toggle bit 7 to clear count
		writereg(client, 0x18, 0x80);
		break;
	}
	case FE_READ_AFC:
	{
		s32 *afc=(s32 *) arg;
		
		*afc=((int)((char)(readreg(client,0x0a)<<1)))/2;
		*afc=(*afc*(int)(ves->srate/8))/16;
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

		SetInversion(client, param->Inversion);
		SetFEC(client, fectab[param->u.qpsk.FEC_inner]);
		SetSymbolrate(client, param->u.qpsk.SymbolRate, 1);
		break;
	}
	case FE_RESET:
	{
		ClrBit1893(client);
		break;
	}
	case FE_SEC_SET_TONE:
	{
		secToneMode mode=(secToneMode)arg;
		ves->tone=(mode==SEC_TONE_ON)?1:0;
		return fp_set_sec(ves->power, ves->tone);
	}
	case FE_SEC_SET_VOLTAGE:
	{
		secVoltage volt=(secVoltage)arg;
		switch (volt)
		{
		case SEC_VOLTAGE_OFF:
			ves->power=0;
			break;
		case SEC_VOLTAGE_LT:
			ves->power=-2;
			break;
		case SEC_VOLTAGE_13:
			ves->power=1;
			break;
		case SEC_VOLTAGE_13_5:
			ves->power=2;
			break;
		case SEC_VOLTAGE_18:
			ves->power=3;
			break;
		case SEC_VOLTAGE_18_5:
			ves->power=4;
			break;
		default:
			printk("invalid voltage\n");
		}
		return fp_set_sec(ves->power, ves->tone);
	}
	case FE_SEC_MINI_COMMAND:
	{
		secMiniCmd minicmd = (secMiniCmd) arg;

		switch (minicmd) {
		case SEC_MINI_A:
			printk ("minidiseqc: A\n");
			return fp_send_diseqc (1, "\x00\x00\x00\x00", 4);

		case SEC_MINI_B:
			printk ("minidiseqc: B\n");
			return fp_send_diseqc (1, "\xff", 1);

		default:
			break;
		}
		break;
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
			return fp_send_diseqc(1, msg, command->u.diseqc.numParams+3);
		}
		case SEC_CMDTYPE_DISEQC_RAW:
		{
			unsigned char msg[SEC_MAX_DISEQC_PARAMS+3];
			msg[0]=command->u.diseqc_raw.cmdtype;
			msg[1]=command->u.diseqc_raw.addr;
			msg[2]=command->u.diseqc_raw.cmd;
			memcpy(msg+3, command->u.diseqc_raw.params, command->u.diseqc_raw.numParams);
			return fp_send_diseqc(1, msg, command->u.diseqc_raw.numParams+3);
		}
		default:
			return -EINVAL;
		}
		break;
	}
	case FE_SEC_GET_STATUS:
	{
		struct secStatus status;

		/* todo: implement */
		status.busMode=SEC_BUS_IDLE;

		switch(ves->power)
		{
		case -2:
			status.selVolt=SEC_VOLTAGE_LT;
			break;
		case 0:
			status.selVolt=SEC_VOLTAGE_OFF;
			break;
		case 1:
			status.selVolt=SEC_VOLTAGE_13;
			break;
		case 2:
			status.selVolt=SEC_VOLTAGE_13_5;
			break;
		case 3:
			status.selVolt=SEC_VOLTAGE_18;
			break;
		case 4:
			status.selVolt=SEC_VOLTAGE_18_5;
			break;
		default:
			return -EINVAL;
		}

		status.contTone=(ves->tone ? SEC_TONE_ON : SEC_TONE_OFF);

		if (copy_to_user(arg, &status, sizeof(status)))
			return -EFAULT;

		break;
	}
	case FE_SETFREQ:
	{
		u32 freq=*(u32*)arg;
		int p, t, os, c, r, pe;
		unsigned long b;

		freq+=479500; freq/=125*4;
		t=0; os=0; c=1;								// doch doch
		r=3; pe=1; p=1;

		b=p<<24; b|=t<<23; b|=os<<22; b|=c<<21;
		b|=r<<18; b|=pe<<17; b|=freq;
		
		fp_set_tuner_dword(T_QPSK, b);
		break;
	}
	default:
		return -1;
	}
	
	return 0;
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

static struct i2c_driver dvbt_driver = {
	"VES1893 DVB demodulator",
	I2C_DRIVERID_VES1893,
	I2C_DF_NOTIFY,
	attach_adapter,
	detach_client,
	dvb_command,
	inc_use,
	dec_use,
};

static struct i2c_client client_template = {
	name:    "VES1893",
	id:      I2C_DRIVERID_VES1893,
	flags:   0,
	addr:    (0x10 >> 1),
	adapter: NULL,
	driver:  &dvbt_driver,
};


static int __init 
init_ves1893(void) {
	int res;
	
	if ((res = i2c_add_driver(&dvbt_driver))) 
	{
		printk("VES1893: i2c driver registration failed\n");
		return res;
	}
	
	dprintk("VES1893: init done\n");
	return 0;
}

static void __exit
exit_ves1893(void)
{
	int res;
	
	if ((res = i2c_del_driver(&dvbt_driver))) 
	{
		printk("dvb-tuner: Driver deregistration failed, "
		       "module not removed.\n");
	}
	dprintk("VES1893: cleanup\n");
}

module_init(init_ves1893);
module_exit(exit_ves1893);
