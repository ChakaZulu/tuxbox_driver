/* 
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

*/    

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <asm/io.h>

#include <linux/i2c.h>
#include "dvb.h"

#include "ves.h"

#ifdef MODULE
MODULE_PARM(debug,"i");
#endif

/*
   exported functions:
void ves_write_reg(int reg, int val);
void ves_init(void);
void ves_set_frontend(struct frontend *front);
void ves_get_frontend(struct frontend *front);
void ves_reset(void);
int ves_read_reg(int reg);
*/

void ves_reset(void);
int ves_read_reg(int reg);

EXPORT_SYMBOL(ves_write_reg);
EXPORT_SYMBOL(ves_init);
EXPORT_SYMBOL(ves_set_frontend);
EXPORT_SYMBOL(ves_get_frontend);

EXPORT_SYMBOL(ves_reset);
EXPORT_SYMBOL(ves_read_reg);

static int debug = 9;
#define dprintk	if (debug) printk

static struct i2c_driver dvbt_driver;
static struct i2c_client client_template, *dclient;

#define BN
static u8 Init1893Tab[] =
{
#ifdef BN
        0x01, 0x94, 0x00, 0x80, 0x80, 0x6a, 0x9b, 0xab,
        0x09, 0x69, 0x00, 0x86, 0x4c, 0x28, 0x7F, 0x00,
        0x00, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
        0x80, 0x00, 0x21, 0xb0, 0x14, 0x00, 0xDC, 0x00,
        0x81, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x55, 0x03, 0x00, 0x7f, 0x00
#else
        0x01, 0xA4, 0x35, 0x81, 0x2A, 0x0d, 0x55, 0xC4,
        0x09, 0x69, 0x00, 0x86, 0x4c, 0x28, 0x7F, 0x00,
        0x00, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
        0x80, 0x00, 0x31, 0xb0, 0x14, 0x00, 0xDC, 0x20,
        0x81, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x55, 0x00, 0x00, 0x7f, 0x00
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

/*
static int dump(struct i2c_client *client)
{
        int i;
        
        printk("VES1893: DUMP\n");
        
        for (i=0; i<54; i++) 
        {
                printk("%02x ", readreg(client, i));
                if ((i&7)==7)
                        printk("\n");
        }
        printk("\n");
        return 0;
}
*/

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

	if (srate>90100000UL/2)
                srate=90100000UL/2;
        if (srate<500000)
                srate=500000;
        ves->srate=srate;
        
#define MUL (1UL<<24)
// #define FIN (90106000UL>>4)
#define FIN (91000000UL>>4)
	ratio=(srate<<4)/FIN;

	tmp=((srate<<4)%FIN)<<8;
	ratio=(ratio<<8)+tmp/FIN;
        
	tmp=(tmp%FIN)<<8;
	ratio=(ratio<<8)+tmp/FIN;
        
	FNR = 0xFF;
	
	if (ratio < MUL/3)           FNR = 0;
	if (ratio < (MUL*11)/50)     FNR = 1;
	if (ratio < MUL/6)           FNR = 2;
	if (ratio < MUL/9)           FNR = 3;
	if (ratio < MUL/12)          FNR = 4;
	if (ratio < (MUL*11)/200)    FNR = 5;
	if (ratio < MUL/24)          FNR = 6;
	if (ratio < (MUL*27)/1000)   FNR = 7;
	if (ratio < MUL/48)          FNR = 8;
	if (ratio < (MUL*137)/10000) FNR = 9;

	if (FNR == 0xFF)
	{
		ADCONF = 0x89;		//bypass Filter
		FCONF  = 0x80;		//default
		FNR	= 0;
	}
	else
	{
		ADCONF = 0x81;
		FCONF  = 0x88 | (FNR >> 1) | ((FNR & 0x01) << 5); //default | DFN | AFS
	}

	//(int)( ((1<<21)<<(FNR>>1)) * (float)(srate) / 90100000.0 + 0.5);
	//(int)(32 * 90100000.0 / (float)(srate) / (1<<(FNR>>1)) + 0.5);


	BDR = ((  (ratio<<(FNR>>1))  >>2)+1)>>1;
	BDRI = (  ((90100000UL<<4) / ((srate << (FNR>>1))>>2)  ) +1 ) >> 1;

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
        
        if (i2c_master_send(&client_template,NULL,0))
                return -1;
        
        client_template.adapter=adap;
        
        dprintk("readreg\n");
        if ((readreg(&client_template, 0x1e)&0xf0)!=0xd0)
        {
          if ((readreg(&client_template, 0x1a)&0xF0)==0x70)
            printk("warning, no VES1893 found but a VES1820\n");
          return -1;
        }
        printk("feID: 1893 %x\n", readreg(&client_template, 0x1e));
        
        if (NULL == (client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
                return -ENOMEM;
        memcpy(client, &client_template, sizeof(struct i2c_client));
        dclient=client;
        
        client->data=ves=kmalloc(sizeof(struct ves1893),GFP_KERNEL);
        if (ves==NULL) {
                kfree(client);
                return -ENOMEM;
        }
       
        i2c_attach_client(client);
        init(client);

        printk("VES1893: attaching VES1893 at 0x%02x ", (client->addr)<<1);
        printk("to adapter %s\n", adap->name);
        return 0;
}

static int detach_client(struct i2c_client *client)
{
        printk("VES1893: detach_client\n");
        i2c_detach_client(client);
        kfree(client->data);
        kfree(client);
        return 0;
}

void ves_write_reg(int reg, int val)
{
  writereg(dclient, reg, val);
}

int ves_read_reg(int reg)
{
  return readreg(dclient, reg);
}

void ves_init(void)
{
  init(dclient);
}

void ves_reset(void)
{
  ClrBit1893(dclient);
}

void ves_set_frontend(struct frontend *front)
{
  struct ves1893 *ves=(struct ves1893 *) dclient->data;
  if (ves->inv!=front->inv)
  {
    ves->inv=front->inv;
    writereg(dclient, 0x0c, Init1893Tab[0x0c] ^ (ves->inv ? 0x40 : 0x00));
    ClrBit1893(dclient);
  }
  SetFEC(dclient, front->fec);
  SetSymbolrate(dclient, front->srate, 1);
  printk("sync: %x\n", readreg(dclient, 0x0E));
}

void ves_get_frontend(struct frontend *front)
{
  front->type=FRONT_DVBS;
  front->afc=((int)((char)(readreg(dclient,0x0a)<<1)))/2;
  front->afc=(front->afc*(int)(front->srate/8))/16;
  front->agc=(readreg(dclient,0x0b)<<8);
  front->sync=readreg(dclient,0x0e);
  printk("sync: %x\n", front->sync);
  front->nest=(readreg(dclient,0x1c)<<8);

  front->vber = readreg(dclient,0x15);
  front->vber|=(readreg(dclient,0x16)<<8);
  front->vber|=(readreg(dclient,0x17)<<16);
  printk("vber: %x\n", front->vber);

  if ((front->fec==8) && ((front->sync&0x1f) == 0x1f))
    front->fec=(readreg(dclient, 0x0d)>>4)&0x07;
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
        "VES1893 DVB DECODER",
        I2C_DRIVERID_VES1893,
        I2C_DF_NOTIFY,
        attach_adapter,
        detach_client,
        0,
        inc_use,
        dec_use,
};

static struct i2c_client client_template = {
        "VES1893",
        I2C_DRIVERID_VES1893,
        0,
        (0x10 >> 1),
        NULL,
        &dvbt_driver,
        NULL
};


#ifdef MODULE
int init_module(void) {
        int res;
        
        if ((res = i2c_add_driver(&dvbt_driver))) 
        {
                printk("VES1893: Driver registration failed, module not inserted.\n");
                return res;
        }
        if (!dclient)
        {
                printk("VES1893: not found.\n");
                i2c_del_driver(&dvbt_driver);
                return -EBUSY;
        }
        
        dprintk("VES1893: init_module\n");
        return 0;
}

void cleanup_module(void)
{
        int res;
        
        if ((res = i2c_del_driver(&dvbt_driver))) 
        {
                printk("dvb-tuner: Driver deregistration failed, "
                       "module not removed.\n");
        }
        dprintk("VES1893: cleanup\n");
}
#endif


/*
 * Local variables:
 * c-indent-level: 8
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -8
 * c-argdecl-indent: 8
 * c-label-offset: -8
 * c-continued-statement-offset: 8
 * c-continued-brace-offset: 0
 * indent-tabs-mode: nil
 * tab-width: 8
 * End:
 */

