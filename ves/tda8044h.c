/* 
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
 *   $Revision: 1.4 $
 *
 */

/* ---------------------------------------------------------------------- */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <asm/8xx_immap.h>

#include <dbox/dvb.h>
#include <dbox/ves.h>
#include <dbox/fp.h>

/* ---------------------------------------------------------------------- */

static void tda_write_reg(int reg, int val);
static void tda_init(void);
static void tda_set_frontend(struct frontend *front);
static void tda_get_frontend(struct frontend *front);
static int tda_get_unc_packet(uint32_t *uncp);
static int tda_set_freq(int freq);

static int tda_set_sec(int power,int tone);
static int tda_send_diseqc(u8 *cmd,unsigned int len);
static int tda_sec_status(void);

#define TDA_INTERRUPT 14
static void tda_interrupt(int irq, void *vdev, struct pt_regs * regs);

static void tda_task(void*);

struct tq_struct tda_tasklet=
{
	routine: tda_task,
	data: 0
};

static int debug = 1;
static DECLARE_MUTEX(i2c_mutex);

#ifdef MODULE
MODULE_PARM(debug,"i");
#endif

#define dprintk	if (debug) printk

struct demod_function_struct tda8044h={
		write_reg:			tda_write_reg, 
		init:						tda_init,
		set_frontend: 	tda_set_frontend,
		get_frontend:		tda_get_frontend,
		get_unc_packet: tda_get_unc_packet,
		set_frequency:	tda_set_freq,
		set_sec:				tda_set_sec,
		send_diseqc:		tda_send_diseqc,
		sec_status:			tda_sec_status};

static struct i2c_driver dvbt_driver;
static struct i2c_client *dclient;

int attach_adapter(struct i2c_adapter *adap);
int detach_client(struct i2c_client *client);
static void inc_use (struct i2c_client *client);
static void dec_use (struct i2c_client *client);

static struct i2c_driver dvbt_driver = {
	"TDA8044H DVB DECODER",
	I2C_DRIVERID_TDA8044H,
	I2C_DF_NOTIFY,
	attach_adapter,
	detach_client,
	0,
	inc_use,
	dec_use,
};

static struct i2c_client client_template = {
	"TDA8044H",
	I2C_DRIVERID_TDA8044H,
	0,
	(0xD0 >> 1),
	NULL,
	&dvbt_driver,
	NULL
};

static struct i2c_client tuner = {
	"TUNER",
	I2C_DRIVERID_TDA8044H-1,
	0,
	(0xC6 >> 1),
	NULL,
	&dvbt_driver,
	NULL
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

typedef struct tda8044h_s
{
	int inversion, srate, sync, fec;
	
	int loopopen;
} tda8044h_t;

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
	tda8044h_t *tda=(tda8044h_t*)client->data;
	int i;
	
	printk("tda8044h: INIT\n");
	dprintk("tda8044h.o: %x\n", readreg(client, 0x17));
	
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
	tda8044h_t *tda=(tda8044h_t*)client->data;
	__u8 srate[16];
	int i;
	
	srate[0]=1;
	srate[1]=0;			// no differential encoding, spectral inversion unknown, QPSK encoding
	/*
		00 6f b5 87 22 00 20 30 42 98 28 30 42 99 50 -> 01   27500  3/4
		00 6f b5 87 22 00 80 30 42 98 28 30 42 99 50         27500  1/2
		00 7a e1 47 23 00 02 30 42 98 28 30 42 99 50         25000  7/8
		00 7a e1 47 23 00 08 30 42 98 28 30 42 99 50         25000  5/6
		00 7a e1 47 23 00 40 30 42 98 28 30 42 99 50         25000  2/3
		20 8b a2 e9 23 00 08 30 42 98 28 30 42 99 50         22000  5/6
		---------------
		00 99 99 9a 25 00 80 30 51 78 28 30 42 79 50         10000  1/2 VERT
		00 99 99 9a 25 00 80 30 51 78 28 30 42 79 50         10000  1/2 HOR
		
		die 10000er sind mit vorsicht zu geniessen, scheint son unteres limit zu geben.
		
		ansonsten ist - unschwer erkennbar - das dword ab 1 die symbolrate, die auch in 
		einem gewissen verhältnis steht.
		
		also X/symbolrate = dword.
		
		wer lust auf bitschieben und dividieren hat kann das ja mal machen.
	*/
	
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
		printk("fec: %02x\n", FEC);
//	srate[7]=0x100>>FEC;
	srate[7]=0xFF; 							// allow all
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
		printk("%02x ", srate[i]);
	printk("\n");
	i2c_master_send(client, "\x17\x68\x9a", 3);
	i2c_master_send(client, "\x22\xf9", 2);				// SCPC
	tda->loopopen=1;
}

int attach_adapter(struct i2c_adapter *adap)
{
	tda8044h_t *tda;
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
	
	dclient=client;
	client->data=tda=kmalloc(sizeof(tda8044h_t), GFP_KERNEL);
	if (!tda)
	{
		kfree(client);
		return -ENOMEM;
	}
	
	printk("tda8044h.o: attaching TDA8044H at 0x%02x\n", (client->addr)<<1);
	i2c_attach_client(client);
	tuner.adapter=adap;
	if (register_demod(&tda8044h))
		printk("tda8044h.o: can't register demod.\n");

	tda_tasklet.data = (void*)client->data;

	if (request_8xxirq(TDA_INTERRUPT, tda_interrupt, SA_ONESHOT, "tda8044h", NULL) != 0)
	{
		i2c_del_driver(&dvbt_driver);
		dprintk("tda8044h.o: can't request interrupt\n");
		return -EBUSY;
	}

	return 0;
}

int detach_client(struct i2c_client *client)
{
	printk("tda8044h.o: detach_client\n");
	unregister_demod(&tda8044h);
	i2c_detach_client(client);
	kfree(client->data);
	kfree(client);
	free_irq(TDA_INTERRUPT, NULL);
	return 0;
}

static void tda_write_reg(int reg, int val)
{
	writereg(dclient, reg, val);
}

static void tda_init(void)
{
	if (dclient)
		init(dclient);
	else
		panic("tried to initialize pure virtual tuner.\n");
}

static void tda_set_frontend(struct frontend *front)
{
	SetSymbolrate(dclient, front->srate, front->fec);
	i2c_master_send(dclient, "\x0b\x68\x70", 3);		// stop sweep, close loop
}

static void tda_get_frontend(struct frontend *front)
{
	tda8044h_t *tda=(tda8044h_t*)dclient->data;
	front->type=FRONT_DVBS;
/*	front->afc=readreg(dclient, 5)<<16;
	front->afc|=readreg(dclient, 6)<<8;
	front->afc|=readreg(dclient, 7);
	front->agc=readreg(dclient, 1); */
	front->sync=tda->sync;
/*	printk("s/n ratio: %d\n", readreg(dclient, 8));
	printk("nr. of lost blocks: %d\n", readreg(dclient, 0xF));
	printk("nr. of corrected blocks: %d\n", readreg(dclient, 0x10)); */
/*	front->vber=readreg(dclient, 0xb)<<16;
	front->vber|=readreg(dclient, 0xc)<<8;
	front->vber|=readreg(dclient, 0xd);

	printk("nr. of bit-errors: %d:%d\n", front->vber>>21, front->vber&0x1FFFFF); */
	front->nest=-1;
}

static int tda_get_unc_packet(uint32_t *uncp)
{
	*uncp=0;
	return -1;
}

static int tda_set_freq(int freq)
{
	int tries=10;
	u8 msg[4]={0, 0, 0x81, 0x60};

		// semaphore, irgendwie
	writereg(dclient, 0x1C, 0x80);
	
	msg[0]=(freq/1000000)>>8;
	msg[1]=(freq/1000000)&0xFF;

	if (i2c_master_send(&tuner, msg, 4)!=4)
		dprintk("tda8044h.o: writereg error\n");

	writereg(dclient, 0x1C, 0x0);
	
	while (tries--)
	{
		udelay(10*1000);
		writereg(dclient, 0x1C, 0x80);
	
		if (i2c_master_recv(&tuner, msg, 1)!=1)
			dprintk("tda8044h.o: read tuner error\n");
	
		writereg(dclient, 0x1C, 0x0);
		if (msg[0]&0x40)		// in-lock flag
			return 0;
	}
	printk("tuner: couldn't acquire lock.\n");
	return -1;
}

static int tda_set_sec(int power, int tone)
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
	writereg(dclient, 0x20, powerv);
	writereg(dclient, 0x29, tone?0x80:0);
	return 0;
}

static int tda_send_diseqc(u8 *cmd,unsigned int len)
{
	int i,a;

	if(len==1)
	{
		printk("Ich kann kein mini-diseqc.\n");
	}
	else 
	{	
		if(len>6 || len<3)
		{
			printk("wrong message size.\n");
			return 0;
		}
		for(a=0;a<2;a++)
		{
		    writereg(dclient,0x29,8+(len-3));
		    for(i=0;i<len;i++) writereg(dclient,0x23+i,cmd[i]);
		    writereg(dclient,0x29,0x0C+(len-3));
		}
		    
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
	tda8044h_t *tda=(tda8044h_t*)dclient->data;
	int sync;
	writereg(dclient, 0, 4);
	enable_irq(TDA_INTERRUPT);
	
	sync=readreg(dclient, 2);
	
	if (tda->loopopen && (sync&2) && (!(tda->sync&2)))
	{
		printk("loop open, lock acquired, closing loop.\n");
//		i2c_master_send(dclient, "\x0b\x68\x70", 3);		// stop sweep, close loop
		tda->loopopen=0;
	}
	
	if ((sync == 0x1F) && (tda->sync != 0x1F))
	{
		int val=readreg(dclient, 0xE);
		tda->inversion=!!(val&0x80);
		tda->fec=val&7;
		printk("acquired full sync, found FEC: %d/%d, %sspectral inversion\n", tda->fec, tda->fec+1, tda->inversion?"":"no ");
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
	if (!dclient)
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

