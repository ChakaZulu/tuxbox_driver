/* 
    VES1993  - Single Chip Satellite Channel Receiver driver module
               
    Copyright (C) 2001 Ronny Strutz  <3DES@tuxbox.org>

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
#include <dbox/dvb.h>
#include <dbox/ves.h>
#include <dbox/fp.h>

#ifdef MODULE
MODULE_PARM(debug,"i");
#endif

static struct i2c_driver dvbt_driver;
static struct i2c_client client_template, *dclient;

static void ves_write_reg(int reg, int val);
static void ves_init(void);
static void ves_set_frontend(struct frontend *front);
static void ves_get_frontend(struct frontend *front);
static void ves_reset(void);
static int ves_read_reg(int reg);
static int mitel_set_freq(int freq);
static int ves_set_sec(int power,int tone);
static int ves_get_unc_packet(u32 *uncp);
static int ves_send_diseqc(u8 *cmd, unsigned int len);

struct demod_function_struct ves1993={
		write_reg:		ves_write_reg, 
		init:			ves_init,
		set_frontend:	 	ves_set_frontend,
		get_frontend:		ves_get_frontend,
		get_unc_packet:		ves_get_unc_packet,
		set_frequency:		mitel_set_freq,
		set_sec:		ves_set_sec,								// das hier stimmt nicht, oder?
		send_diseqc:		ves_send_diseqc,
		sec_status:		fp_sec_status};

static int debug = 0;
#define dprintk	if (debug) printk
#define TUNER_I2C_DRIVERID  0xF0C2
//Tuner ----------------------------------------------------------------------
static int tuner_detach_client(struct i2c_client *tuner_client);
static int tuner_detect_client(struct i2c_adapter *adapter, int address, unsigned short flags,int kind);
static int tuner_attach_adapter(struct i2c_adapter *adapter);
int set_tuner_dword(u32 tw);

static unsigned short normal_i2c[] = { 0xc2>>1,I2C_CLIENT_END };
static unsigned short normal_i2c_range[] = { 0xc2>>1, 0xc2>>1, I2C_CLIENT_END };
I2C_CLIENT_INSMOD;

struct tuner_data
{
	struct i2c_client *tuner_client;
};	
struct tuner_data *defdata=0;

static struct i2c_driver tuner_driver = {
        "Mitel tuner driver",
        TUNER_I2C_DRIVERID,
        I2C_DF_NOTIFY,
        &tuner_attach_adapter,
        &tuner_detach_client,
        0,
        0,
        0,
};
static struct i2c_client tuner_client =  {
        "MITEL",
        TUNER_I2C_DRIVERID,
        0,
        0xC2,
        NULL,
        &tuner_driver,
        NULL
};


static int tuner_detach_client(struct i2c_client *tuner_client)
{
	int err;
	
	if ((err=i2c_detach_client(tuner_client)))
	{
		dprintk("VES1993: couldn't detach tuner client driver.\n");
		return err;
	}
	
	kfree(tuner_client);
	return 0;
}
static int tuner_detect_client(struct i2c_adapter *adapter, int address, unsigned short flags,int kind)
{
	int err = 0;
	struct i2c_client *new_client;
	struct tuner_data *data;
	const char *client_name="DBox2 Tuner Driver";
	
	if (!(new_client=kmalloc(sizeof(struct i2c_client)+sizeof(struct tuner_data), GFP_KERNEL)))
	{
		return -ENOMEM;
	}
	
	new_client->data=new_client+1;
	defdata=data=(struct tuner_data*)(new_client->data);
	new_client->addr=address;
	data->tuner_client=new_client;
	new_client->data=data;
	new_client->adapter=adapter;
	new_client->driver=&tuner_driver;
	new_client->flags=0;
	
	strcpy(new_client->name, client_name);
	
	if ((err=i2c_attach_client(new_client)))
	{
		kfree(new_client);
		return err;
	}
	
	dprintk("VES1993: mitel tuner attached @%02x\n", address>>1);
	
	return 0;
}
static int tuner_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_probe(adapter, &addr_data, &tuner_detect_client);
}

static int tuner_init(void)
{
	int res;
	
	ves_write_reg(0x00,0x11);  //enable tuner access on ves1993
	printk("VES1993: DBox2 mitel tuner driver\n");
	if ((res=i2c_add_driver(&tuner_driver)))
	{
		printk("VES1993: mitel tuner driver registration failed!!!\n");
		return res;
	}
		
	if (!defdata)
	{
		i2c_del_driver(&tuner_driver);
		printk("VES1993: Couldn't find tuner.\n");
		return -EBUSY;
	}
	ves_write_reg(0x00,0x01);  //disable tuner access on ves1993
	mitel_set_freq(2099000000);
	
	return 0;
}

static int tuner_close(void)
{
	int res;
	
	if ((res=i2c_del_driver(&tuner_driver)))
	{
		printk("VES1993: tuner driver unregistration failed.\n");
		return res;
	}
	
	return 0;
}
		
int set_tuner_dword(u32 tw)
{
	char msg[4];
	int len=4;
	*((u32*)(msg))=tw;
	
	dprintk("VES1993: set_tuner_dword (QPSK): %08x\n",tw);
	
	ves_write_reg(0x00,0x11);
	if (i2c_master_send(defdata->tuner_client, msg, len)!=len)
	{
		return -1;
	}
	ves_write_reg(0x00,0x01);
	return -1;

}

static int mitel_set_freq(int freq)		
{
	u8 buffer[4]={0x25,0x70,0x92,0x40}; 
		
	dprintk("SET:%x\n",freq);
	freq/=125000*8;
	
	buffer[0]=(freq>>8) & 0x7F;
	buffer[1]=freq & 0xFF;
	
	dprintk("SET:%x\n",*((u32*)buffer));
	set_tuner_dword(*((u32*)buffer));

	return 0;
}

//----------------------------------------------------------------------------


static u8 Init1993Tab[] =
{
        0x00, 0x9c, 0x35, 0x80, 0x6a, 0x2b, 0xab, 0xaa,
        0x0e, 0x45, 0x00, 0x00, 0x4c, 0x0a, 0x00, 0x00,
        0x00, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
        0x80, 0x40, 0x21, 0xb0, 0x00, 0x00, 0x00, 0x10,
        0x89, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x02, 0x55, 0x03, 0x00, 0x00, 0x00, 0x00, 0x03,
	0x00, 0x00, 0x0c, 0x80, 0x00
};

static u8 Init1993WTab[] =
{     //0 1 2 3 4 5 6 7  8 9 a b c d e f
        0,1,1,1,1,1,1,1, 1,1,0,0,1,1,0,0,
        0,1,0,0,0,0,0,0, 1,1,1,1,0,0,0,1,
        1,1,1,0,0,0,0,0, 0,0,1,1,0,0,0,0,
        1,1,1,0,1,1,1,1, 1,1,1,1,1
};

struct ves1993 {
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
        dprintk("VES_writereg\n");
	if (ret!=3) 
                dprintk("writereg error\n");
        return ret;
}

static int ves_get_unc_packet(u32 *uncp)
{
 return 0;
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


static int dump(struct i2c_client *client)
{
	int i;
        
	printk("ves1993: DUMP\n");
        
	for (i=0; i<0x3c; i++) 
		printk("%02x - %02x \n", i,readreg(client, i));
    
	printk("\n");
	return 0;
}

static int ves_set_sec(int power, int tone){
     
	dprintk("VES1993: Set SEC\n"); 
	fp_sagem_set_SECpower(power);

	return 0;
}
static int init(struct i2c_client *client)
{
	struct ves1993 *ves=(struct ves1993 *) client->data;
	int i;
	        
	dprintk("ves1993: init chip\n");

	if (writereg(client, 0, 0)<0)
		dprintk("ves1993: send error\n");
		
	//Init fuer VES1993
	writereg(client,0x3a, 0x0c);	

	for (i=0; i<0x3d; i++)
		if (Init1993WTab[i])
		writereg(client, i, Init1993Tab[i]);
		    
	writereg(client,0x3a, 0x0e);
	writereg(client,0x21, 0x81);
	writereg(client,0x00, 0x00);
	writereg(client,0x06, 0x72);
	writereg(client,0x07, 0x8c);
	writereg(client,0x08, 0x09);
	writereg(client,0x09, 0x6b);
	writereg(client,0x20, 0x81);
	writereg(client,0x21, 0x80);
	writereg(client,0x00, 0x01);
	writereg(client,0x0d, 0x0a);
	    
	ves->ctr=Init1993Tab[0x1f];
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
        dprintk("VES_clrbit1893\n");
        writereg(client, 0, 0);
        writereg(client, 0, 1);
}

static int SetFEC(struct i2c_client *client, u8 fec)
{
	struct ves1993 *ves=(struct ves1993 *) client->data;
        
	if (fec>=8) 
		fec=8;
	if (ves->fec==fec)
		return 0;
	ves->fec=fec;
	return writereg(client, 0x0d, 8);
}

static int SetSymbolrate(struct i2c_client *client, u32 srate, int doclr)
{
	struct ves1993 *ves=(struct ves1993 *) client->data;
	u32 BDR;
	u32 ratio;
	u8  ADCONF, FCONF, FNR;
	u32 BDRI;
	u32 tmp;
        
	if (ves->srate==srate) 
	{
		if (doclr)
			ClrBit1893(client);
		return 0;
	}
	dprintk("VES_setsymbolrate %d\n", srate);

#define XIN (92160000UL) // 3des sagem dbox Crystal ist 92,16 MHz !!

	if (srate>XIN/2)
                srate=XIN/2;
        if (srate<500000)
                srate=500000;
        ves->srate=srate;
        
#define MUL (1UL<<25)
#define FIN (XIN>>4)
        tmp=srate<<6;
	ratio=tmp/FIN;

	tmp=(tmp%FIN)<<8;
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
	} else	{
		ADCONF = 0x81;
		FCONF  = 0x88 | (FNR >> 1) | ((FNR & 0x01) << 5); //default | DFN | AFS
	}


	BDR = ((  (ratio<<(FNR>>1))  >>4)+1)>>1;
	BDRI = (  ((FIN<<8) / ((srate << (FNR>>1))>>2)  ) +1 ) >> 1;

        dprintk("VES_FNR= %d\n", FNR);
        dprintk("VES_ratio= %08x\n", ratio);
        dprintk("VES_BDR= %08x\n", BDR);
        dprintk("VES_BDRI= %02x\n", BDRI);

	if (BDRI > 0xFF)
	        BDRI = 0xFF;

	writereg(client, 0x20, ADCONF);
	writereg(client, 0x21, FCONF);

	if (srate<6000000) 
		writereg(client, 5, Init1993Tab[0x05] | 0x80);
	else
		writereg(client, 5, Init1993Tab[0x05] & 0x7f);

	ves_write_reg(0x00,0x00);
  writereg(client, 6, 0xff&BDR);
	writereg(client, 7, 0xff&(BDR>>8));
	writereg(client, 8, 0x0f&(BDR>>16));
  writereg(client, 9, BDRI);
	ves_write_reg(0x20,0x81);
	ves_write_reg(0x21,0x80);
	ves_write_reg(0x00,0x01);

	return 0;
}

static int attach_adapter(struct i2c_adapter *adap)
{
        struct ves1993 *ves;
        struct i2c_client *client;
        
        client_template.adapter=adap;
        client_template.adapter=adap;
        
        dprintk("readreg\n");
        if ((readreg(&client_template, 0x1e)&0xf0)!=0xd0)
        {
          if ((readreg(&client_template, 0x1a)&0xF0)==0x70)
            printk("warning, no ves1993 found but a VES1820\n");
          return -1;
        }
        dprintk("feID: 1893 %x\n", readreg(&client_template, 0x1e));
        
        if (NULL == (client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
                return -ENOMEM;
        memcpy(client, &client_template, sizeof(struct i2c_client));
        dclient=client;
        
        client->data=ves=kmalloc(sizeof(struct ves1993),GFP_KERNEL);
        if (ves==NULL) {
                kfree(client);
                return -ENOMEM;
        }
       
        i2c_attach_client(client);
        init(client);
	if (register_demod(&ves1993))
		printk("ves1993.o: can't register demod.\n");
              
        return 0;
}

static int detach_client(struct i2c_client *client)
{
        printk("ves1993: detach_client\n");
        i2c_detach_client(client);
        kfree(client->data);
        kfree(client);
        unregister_demod(&ves1993);
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
	int afc=0,agc=0,snr=0,sync=0;
	unsigned int i;
	u32 a;
	u32 sr;
	
  struct ves1993 *ves=(struct ves1993 *) dclient->data;
  if (ves->inv!=front->inv)
  {
    ves->inv=front->inv;
    writereg(dclient, 0x0c, Init1993Tab[0x0c] ^ (ves->inv ? 0x40 : 0x00));
    ClrBit1893(dclient);
  }
  SetFEC(dclient, front->fec);              
  SetSymbolrate(dclient, front->srate, 1);
  dprintk("sync: %x\n", readreg(dclient, 0x0E));

}

void ves_get_frontend(struct frontend *front)
{
  front->type=FRONT_DVBS;
  front->afc=((int)((char)(readreg(dclient,0x0a)<<1)))/2;
  front->afc=(front->afc*(int)(front->srate/8))/16;
  front->agc=(readreg(dclient,0x0b)<<8);
  front->sync=readreg(dclient,0x0e);
  dprintk("sync: %x\n", front->sync);
  front->nest=(readreg(dclient,0x1c)<<8);

  front->vber = readreg(dclient,0x15);
  front->vber|=(readreg(dclient,0x16)<<8);
  front->vber|=(readreg(dclient,0x17)<<16);
  dprintk("vber: %x\n", front->vber);

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
        "VES1993 DVB DECODER",
        I2C_DRIVERID_VES1993,
        I2C_DF_NOTIFY,
        attach_adapter,
        detach_client,
        0,
        inc_use,
        dec_use,
};

static struct i2c_client client_template = {
        "VES1993",
        I2C_DRIVERID_VES1993,
        0,
        (0x10 >> 1),
        NULL,
        &dvbt_driver,
        NULL
};

int ves_send_diseqc(u8 *cmd, unsigned int len)
{
  //return fp_send_diseqc(2,cmd,len);
	return 0;
}

/* ---------------------------------------------------------------------- */

#ifdef MODULE
int init_module(void) {
	int res;
        	
	if ((res = i2c_add_driver(&dvbt_driver))) 
	{
		printk("ves1993: Driver registration failed, module not inserted.\n");
		return res;
	}
	if (!dclient)
	{
		printk("ves1993: not found.\n");
		i2c_del_driver(&dvbt_driver);
		return -EBUSY;
	}
        
	dprintk("ves1993: init_module\n");

	tuner_init();
	ves_set_sec(2,0);                    //switch to highband
	
	return 0;
}

void cleanup_module(void)
{
	int res;
        
	if ((res = i2c_del_driver(&dvbt_driver))) 
	{
		printk("dvb-tuner: Driver deregistration failed, module not removed.\n");
	}
	dprintk("ves1993: cleanup\n");
	tuner_close();
}
#endif
