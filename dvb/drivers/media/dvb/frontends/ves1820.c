/* 
    VES1820  - Single Chip Cable Channel Receiver driver module
               used on the the Siemens DVB-C cards

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

*/    

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/i2c.h>

#include <dbox/dvb_frontend.h>
#include <dbox/fp.h>

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

u8 Init1820PTab[] =
{
  // changed by tmbinc, according to sniffed i2c-stuff. not validated at all.
  // does NO spectral inversion.
  0x49, 0x6A, 0x13, 0x0A, 0x15, 0x46, 0x26, 0x1A,
  0x43, 0x6A, 0x1A, 0x61, 0x19, 0xA1, 0x63, 0x00,
  0xB8, 0x00, 0xE1, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x32, 0xc8, 0x00, 0x00
};

struct ves1820 {
	int inversion;
	u32 srate;
	u8 pwm;
	u8 reg0;
	
	dvb_front_t frontend;
};


int writereg(struct i2c_client *client, u8 reg, u8 data)
{
	int ret;
	u8 msg[] = {0x00, 0x1f, 0x00};
	
	msg[1]=reg; msg[2]=data;
	ret=i2c_master_send(client, msg, 3);
	if (ret!=3) 
		printk("writereg error\n");
	mdelay(10);
	return ret;
}

u8 readreg(struct i2c_client *client, u8 reg)
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

int init(struct i2c_client *client)
{
	struct ves1820 *ves=(struct ves1820 *) client->data;
	struct i2c_adapter *adap=client->adapter;
	struct i2c_msg msgs[2];
	unsigned char mm1[] = {0xff};
	unsigned char mm2[] = {0x00};
	int i;
	
	dprintk("VES1820: init chip\n");

	msgs[0].flags=0;
	msgs[1].flags=1;
	msgs[0].addr=msgs[1].addr=(0x28<<1);
	mm1[0]=0xff;
	msgs[0].len=1; msgs[1].len=1;
	msgs[0].buf=mm1; msgs[1].buf=mm2;
	i2c_transfer(adap, msgs, 2);
	ves->pwm=*mm2;
	dprintk("VES1820: pwm=%02x\n", ves->pwm);
	if (ves->pwm == 0xff)
		ves->pwm=0x48;
       
	if (writereg(client, 0, 0)<0)
		printk("VES1820: send error\n");
	for (i=0; i<53; i++)
		writereg(client, i, Init1820PTab[i]);

	ves->inversion=0;
	ves->srate=0;
	ves->reg0=Init1820PTab[0];

	writereg(client, 0x34, ves->pwm);
	return 0;
}


void ClrBit1820(struct i2c_client *client)
{
	struct ves1820 *ves=(struct ves1820 *) client->data;
	u8 val;
	
	val=ves->reg0;
	if (ves->inversion)
	  val&=0xdf;
	writereg(client, 0, val & 0xfe);
	writereg(client, 0, val);
}

void SetPWM(struct i2c_client* client) 
{
	struct ves1820 *ves=(struct ves1820 *) client->data;

	writereg(client, 0x34, ves->pwm); 
}

int SetSymbolrate(struct i2c_client* client, u32 Symbolrate, int DoCLB)
{
	struct ves1820 *ves=(struct ves1820 *) client->data;
	s32 BDR; 
	s32 BDRI;
	s16 SFIL=0;
// #define XIN 57840000UL
// #define FIN (57840000UL>>4)
        // the dbox seems to use another crystal, 69.6 Mhz.
#define XIN 69600000UL
#define FIN (XIN>>4)
	s16 NDEC = 0;
	u32 tmp, ratio;

	if (Symbolrate > XIN/2) 
		Symbolrate=XIN/2;
	if (Symbolrate < 500000)
		Symbolrate=500000;
	ves->srate=Symbolrate;

	if (Symbolrate < XIN/16) NDEC = 1;
	if (Symbolrate < XIN/32) NDEC = 2;
	if (Symbolrate < XIN/64) NDEC = 3;

	if (Symbolrate < (u32)(XIN/12.3)) SFIL = 1;
	if (Symbolrate < (u32)(XIN/16))	 SFIL = 0;
	if (Symbolrate < (u32)(XIN/14.6)) SFIL = 1;
	if (Symbolrate < (u32)(XIN/32))	 SFIL = 0;
	if (Symbolrate < (u32)(XIN/49.2)) SFIL = 1;
	if (Symbolrate < (u32)(XIN/64))	 SFIL = 0;
	if (Symbolrate < (u32)(XIN/98.4)) SFIL = 1;
	
	Symbolrate<<=NDEC;
	ratio=(Symbolrate<<4)/FIN;
	tmp=((Symbolrate<<4)%FIN)<<8;
	ratio=(ratio<<8)+tmp/FIN;
	tmp=(tmp%FIN)<<8;
	ratio=(ratio<<8)+(tmp+FIN/2)/FIN;
	
	BDR= ratio;
	BDRI= (((XIN<<5) / Symbolrate)+1)/2;
	
	if (BDRI > 0xFF) 
		BDRI = 0xFF;
	
	SFIL = (SFIL << 4) | Init1820PTab[0x0E];
	
	NDEC = (NDEC << 6) | Init1820PTab[0x03];

	writereg(client, 0x03, NDEC);
	writereg(client, 0x0a, BDR&0xff);
	writereg(client, 0x0b, (BDR>> 8)&0xff);
	writereg(client, 0x0c, (BDR>>16)&0x3f);

	writereg(client, 0x0d, BDRI);
	writereg(client, 0x0e, SFIL);

	SetPWM(client);
	
	if (DoCLB) ClrBit1820(client);
	
	return 0;
}

typedef struct {
	Modulation      QAM_Mode;
	int	     NoOfSym;
	unsigned char   Reg1;
	unsigned char   Reg5;
	unsigned char   Reg8;
	unsigned char   Reg9;
} QAM_SETTING;


QAM_SETTING QAM_Values[] = {	
	{QAM_16 ,  16, 145, 164, 162, 145},
	{QAM_32 ,  32, 150, 120, 116, 150},
	{QAM_64 ,  64, 106,  70,  67, 106},
	{QAM_128, 128, 126,  54,  52, 126},
	{QAM_256, 256, 107,  38,  35, 107}
};


int SetQAM(struct i2c_client* client, Modulation QAM_Mode, int DoCLB)
{
	struct ves1820 *ves=(struct ves1820 *) client->data;
	int real_qam = 0;
	
	switch (QAM_Mode) {
	case QAM_16 : real_qam = 0; break;
	case QAM_32 : real_qam = 1; break;
	case QAM_64 : real_qam = 2; break;
	case QAM_128: real_qam = 3; break;
	case QAM_256: real_qam = 4; break;
	default:
		return -1;
	}
	ves->reg0=(ves->reg0 & 0xe3) | (real_qam << 2);

	writereg(client, 0x01, QAM_Values[real_qam].Reg1);
	writereg(client, 0x05, QAM_Values[real_qam].Reg5);
	writereg(client, 0x08, QAM_Values[real_qam].Reg8);
	writereg(client, 0x09, QAM_Values[real_qam].Reg9);
	
	if (DoCLB) 
		ClrBit1820(client);
	return 0;
}


int attach_adapter(struct i2c_adapter *adap)
{
	struct ves1820 *ves;
	struct i2c_client *client;
	
	client_template.adapter=adap;
	
	printk("attach_adapter\n");
/* 	if (i2c_master_send(&client_template,NULL,0))
		return -1; */

	client_template.adapter=adap;
	
	if ((readreg(&client_template, 0x1a)&0xf0)!=0x70)
		return -1;
	
	if (NULL == (client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
		return -ENOMEM;
	memcpy(client, &client_template, sizeof(struct i2c_client));
	
	client->data=ves=kmalloc(sizeof(struct ves1820),GFP_KERNEL);
	if (ves==NULL) {
		kfree(client);
		return -ENOMEM;
	}
       
	printk("VES1820: attaching VES1820 at 0x%02x\n", (client->addr)<<1);
	i2c_attach_client(client);
	
	init(client);
	
	printk("VES1820: attached to adapter %s\n", adap->name);
	MOD_INC_USE_COUNT;
	
	ves->frontend.type=DVB_C;
	ves->frontend.capabilities=0; // kann auch nix
	ves->frontend.i2cbus=adap;
	
	ves->frontend.demod=client;
	ves->frontend.demod_type=DVB_DEMOD_VES1820;
	
	register_frontend(&ves->frontend);
	
	return 0;
}

int detach_client(struct i2c_client *client)
{
	printk("VES1820: detach_client\n");
	unregister_frontend(&((struct ves1820*)client->data)->frontend);
	i2c_detach_client(client);
	kfree(client->data);
	kfree(client);
	MOD_DEC_USE_COUNT;
	return 0;
}

static int dvb_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	switch (cmd) 
	{
		
	case FE_READ_STATUS:
	{
		FrontendStatus *status=(FrontendStatus *) arg;
		int sync;

		*status=0;

		sync=readreg(client,0x11);
		if (sync&2)
			*status|=FE_HAS_SIGNAL;
		if (sync&2)
			*status|=FE_HAS_CARRIER;
		if (sync&4)
			*status|=FE_HAS_SYNC;
		if (sync&8)
			*status|=FE_HAS_LOCK;
		break;
	}

	case FE_WRITEREG:
	{
		u8 *msg = (u8 *) arg;
		writereg(client, msg[0], msg[1]);
		break;
	}
	case FE_INIT:
	{
		init(client);
		break;
	}
	case FE_RESET:
		ClrBit1820(client);
		break;

	case FE_SET_FRONTEND:
	{
		FrontendParameters *param = (FrontendParameters *) arg;

		SetQAM(client, param->u.qam.QAM, 1);
		SetSymbolrate(client, param->u.qam.SymbolRate, 1);
		break;
	}

/*	{
		struct frontend *front = (struct frontend *)arg;
		
		front->afc=(int)((char)(readreg(client,0x19)));
		front->afc=(front->afc*(int)(front->param.u.qam.SymbolRate/8))/128;
		front->agc=readreg(client,0x17);
		front->sync=readreg(client,0x11);
		front->nest=0;

		front->vber = readreg(client,0x14);
		front->vber|=(readreg(client,0x15)<<8);
		front->vber|=(readreg(client,0x16)<<16); 
		break;
	} */
	
	case FE_SETFREQ:
	{
		u32 freq=*(u32*)arg;
		u8 buffer[4];
		freq+=36125;
		freq*=10;
		freq/=625;

		buffer[0]=(freq>>8)&0x7F;
		buffer[1]=freq&0xFF;
		buffer[2]=0x80 | (((freq>>15)&3)<<6) | 5;
		buffer[3]=1;
		
		fp_set_tuner_dword(T_QAM, *((u32*)buffer));
		break;
	}

	default:
		return -1;
	}
	
	return 0;
} 

void inc_use (struct i2c_client *client)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

void dec_use (struct i2c_client *client)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

static struct i2c_driver dvbt_driver = {
	"VES1820 DVB demodulator",
	I2C_DRIVERID_VES1820,
	I2C_DF_NOTIFY,
	attach_adapter,
	detach_client,
	dvb_command,
	inc_use,
	dec_use,
};

static struct i2c_client client_template = {
	name:    "VES1820",
	id:      I2C_DRIVERID_VES1820,
	flags:   0,
	addr:    (0x10 >> 1),
	adapter: NULL,
	driver:  &dvbt_driver,
};


static __init int 
init_ves1820(void) {
	int res;
	
	if ((res = i2c_add_driver(&dvbt_driver))) 
	{
		printk("VES1820: i2c driver registration failed\n");
		return res;
	}
	
	dprintk("VES1820: init done\n");
	return 0;
}

static __exit void 
exit_ves1820(void)
{
	int res;
	
	if ((res = i2c_del_driver(&dvbt_driver))) 
	{
		printk("dvb-tuner: Driver deregistration failed, "
		       "module not removed.\n");
	}
	dprintk("VES1820: cleanup\n");
}

module_init(init_ves1820);
module_exit(exit_ves1820);
