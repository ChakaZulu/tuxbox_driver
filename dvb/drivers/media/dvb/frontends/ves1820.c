/*
   $Id: ves1820.c,v 1.26 2002/06/28 01:07:56 Homar Exp $

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


    $Log: ves1820.c,v $
    Revision 1.26  2002/06/28 01:07:56  Homar
    UHF-Scan gefixt

    Revision 1.25  2002/06/27 19:55:08  Homar
    Scan Mix-mode eingebaut

    Revision 1.24  2002/06/16 19:49:35  Homar
    AutoInversion eingefügt

    Revision 1.23  2002/03/06 09:33:12  gillem
    - remove dec/inc use count
    - clean module unload (set into standby)

    Revision 1.22  2002/02/24 15:32:06  woglinde
    new tuner-api now in HEAD, not only in branch,
    to check out the old tuner-api should be easy using
    -r and date

    Revision 1.19.2.3  2002/01/22 23:44:56  fnbrd
    Id und Log reingemacht.

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
/*  // changed by tmbinc, according to sniffed i2c-stuff. not validated at all.
  // does NO spectral inversion.
  0x49, 0x6A, 0x13, 0x0A, 0x15, 0x46, 0x26, 0x1A,
  0x43, 0x6A, 0x1A, 0x61, 0x19, 0xA1, 0x63, 0x00,
  0xB8, 0x00, 0xE1, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x32, 0xc8, 0x00, 0x00
};
*/
  0x49, //0  noStdBy / internADC / noInversion / 64-QAM / outputs active / CLB-Softreset default
  0x6A, //1  AGC-Reference set to 64-QAM
  0x13, //2  Frontend Lock-Indicator /  internADC / AGC in PWM ?? (def=1) / AGC nonInverted / AGC Time constant is minimum (def=00)
  0x0A, //3  NDEC=0 / gain(3.decimation filter)=1 / gain(2.decimation filter)=1 / DYN=1 acquisition range for clock recovery is ± 240 ppm / CLK_C=def.
  0x15, //4  carrier acquisition range is ± 2.5% / loop bandwith (BW/RS)=0.03
  0x46, //5  carrier lock detection = 64-QAM
  0x26, //6  reference coefficient of the equalizer=def. / coefficients of the equalizer are continuously adjusted / linear transversal equalizer
  0x1A, //7  convergence steps during the acquisition phase / tracking phase
  0x43, //8  threshold value used to switch from the acquisition phase to the tracking phase=64-QAM
  0x6A, //9  reference parameter that optimizes the equalizer convergence during the acquisition phase=64-QAM
  0x1A, //A  LSB of the BAUD rate frequency to program
  0x61, //B  MID of the BAUD rate frequency to program
  0x19, //C  MSB of the BAUD rate frequency to program
  		//   according to int((2^24*SymbolRate*2^NDEC)/SYSCLK)
  0xA1, //D  BAUD rate inverse (If BDRI > 255, then set BDRI to 255)
  		//   according to int(16*SYSCLK /(SymbolRate*2^NDEC))
  0x63, //E  gain of the Nyquist filter=3 GNYQ=011 / SFIL=0 / gain of the antialiasing filter=1 GLPF=00 / number of samples=16384 SSAT=11
  0x00, //F  Test-Byte
  0xB8, //10 PVBER=10 (def=01) / CLB_UNC=1 / sync-unsync detection C=11 / correction capability of RS decoder validated RSI=0 / descrambler validated DESCI=0 / IEI=0
  0x00, //11 Status-Read-Register: 0 / (FEL but) NODVB / rough idea of the Bit Error Rate: BER[1] BER[0] / DVB lock: FEL=(FSYNC&&CARLOCK) / MPEG2 sync: if(FSYNC) / CarrierLock: if(CARLOCK) / Phase detection: acquisition=0 tracking=1 EQ_ALGO
  0xE1, //12 if some output Pins inverted or not
  0x00, //13 7-bit counter uncorractable Packets
  0x00, //14 8-bit LSB of the BER=OUTPUT SIGNAL QUALITY MEASUREMENT (BER)
  0x00, //15 8-bit MID of the BER
  0x00, //16 4-bit MSB of the BER
  0x00, //17 AGC information 0xFF is maximum
  0x00, //18 MSE Mean Square Error = representation of the signal quality
  0x00, //19 VAFC indicates the frequency offset when the carrier has been recovered
  		//	 sigmaF = (VAFC * SymbolRate) / 1024
  0x00, //1A IDENTITY
  0x01, //1B if(PDOWN) internal ADC in STDBY / PCLK polarity of SamplingClock SACLOCK if internal ADC is used
  0x00, //1C EQCONF2
  0x00, //1D CKOFF symbol clock frequency offset
  		//   If DYN=0 SRoffset = (CKOFF * 120 / 128) ppm
		//	 If DYN=1 SRoffset = (CKOFF * 240 / 128) ppm
  0x00, //1E PLL config
  0x00, //1F
  0x00, //20 INTSEL config
  0x00, //21 SATNYQ represents the number of saturations that occur at the output of the Nyquist filter
  0x00, //22 SATADC represents the number of saturations that occur at the output of the ADC
  0x00, //23 HALFADC HLFADC represents the number of times that the output of the ADC exceeds the mid-range
  0x00, //24 SATDEC1 SDEC1represents the number of saturations that occur at the output of first decimation filter
  0x00, //25 SATDEC2 SDEC2 represents the number of saturations that occur at the output of second decimation filter
  0x00, //26 SATDEC3 SDEC3 represents the number of saturations that occur at the output of third decimation filter
  0x00, //27 SATAAF SAAF represents the number of saturations that occur at the output of the antialiasing filter
  0x00, //28
  0x00, //29
  0x00, //2A
  0x00, //2B
  0x00, //2C
  0x00, //2D
  0x00, //2E
  0x00, //2F
  0x01, //30 SATTHR STHR is a threshold value, compared to the register SATADC. If SATADC > STHR then an interrupt can be generated on pin IT. (See register ITSEL)
  0x32, //31 HALFTHR HLFTHR is a threshold value, compared to the register HLFADC
		//	 If HLFADC < HLFTHR then an interrupt can be generated on pin IT (See register ITSEL)
  0xc8, //32 ITSEL ITEN=INTerrupt Enable / ITSEL[6]=interrupt if AGC is saturated (AGC = 0 or ACG = 255) / ITSEL[3]=interrupt if VBER is refreshed
  0x00, //33 ITSTAT interrupt status register corresponds to ITSEL(i)
  0x00  //34 PWM off
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
        while(ret=i2c_master_send(client, msg, 3)!=3);
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
        while (i2c_transfer(adap, msgs, 2)!=2);
        return mm2[0];
}

void ClrBit1820(struct i2c_client *client)
{
        struct ves1820 *ves=(struct ves1820 *) client->data;
        writereg(client, 0, ves->reg0 & 0xfe);
        writereg(client, 0, ves->reg0);
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
//        if (ves->pwm == 0xff)
//                ves->pwm=0x48;
        if (writereg(client, 0, 0)<0)
                printk("VES1820: send error\n");
        for (i=0; i<53; i++)
                writereg(client, i, Init1820PTab[i]);

        ves->inversion=0;
        ves->srate=0;
        ves->reg0=Init1820PTab[0];
//        writereg(client, 0x34, ves->pwm);
		ClrBit1820(client);
        return 0;
}


static int SetInversion(struct i2c_client *client, int inversion)
{
	struct ves1820 *ves=(struct ves1820 *) client->data;
	u8 val;
	printk("SetInversion: Einsprung \n");
	if (inversion == ves->inversion)
		return 0;
	val=ves->reg0;
	printk("STATUS ves->reg0 = 0x%x\n",val);
	ves->inversion=inversion;
	switch (inversion)
	{
	case INVERSION_OFF:
		val&=0xdf;
		printk("NEUER STATUS: INVERSION = OFF    ves->reg0 = 0x%x\n",val);
		break;
	case INVERSION_ON:
		val|=0x20;
		printk("NEUER STATUS: INVERSION = ON    ves->reg0 = 0x%x\n",val);
		break;
	default:
	case INVERSION_AUTO:
		val&=0xdf;
		printk("NEUER STATUS: INVERSION = AUTO    ves->reg0 = 0x%x\n",val);
		break;
	}
	ves->reg0=val;
	writereg(client, 0, val);
	return -1;
}

void SetPWM(struct i2c_client* client)
{
/*        struct ves1820 *ves=(struct ves1820 *) client->data;
		printk("SetPWM: Einsprung \n");

//        writereg(client, 0x34, ves->pwm);
		printk("SetPWM: %u \n",ves->pwm);
*/
}

int SetSymbolrate(struct i2c_client* client, u32 Symbolrate, int DoCLB)
{

#define XIN 69600000UL
#define SYSCLK 69600000UL
#define SACLK (SYSCLK>>1)
#define FIN (XIN>>4)

        struct ves1820 *ves=(struct ves1820 *) client->data;
        u32 BDR;
        u16 BDRI;
        u8 SFIL = 0;
        u8 NDEC = 0;
        u32 tmp, ratio;

		printk("SetSymbolrate: Einsprung \n");

        if (Symbolrate >= (SACLK >> 2))
			Symbolrate = (SACLK >> 2);
        if (Symbolrate < (SACLK >> 6))
			Symbolrate = (SACLK >> 6);

        ves->srate=Symbolrate;

        if (Symbolrate <= (SACLK >> 2)) NDEC = 0;
        if (Symbolrate < (SACLK>>3)) NDEC = 1;
        if (Symbolrate < (SACLK>>4)) NDEC = 2;
        if (Symbolrate < (SACLK>>5)) NDEC = 3;

        if (Symbolrate < (u32)(SYSCLK>>3)) SFIL = 0;
        if (Symbolrate < (u32)(SYSCLK/12.3)) SFIL = 1;
        if (Symbolrate < (u32)(SYSCLK>>4))	 SFIL = 0;
        if (Symbolrate < (u32)(SYSCLK/24.6)) SFIL = 1;
        if (Symbolrate < (u32)(SYSCLK>>5))	 SFIL = 0;
        if (Symbolrate < (u32)(SYSCLK/49.2)) SFIL = 1;
        if (Symbolrate < (u32)(SYSCLK>>6))	 SFIL = 0;
        if (Symbolrate < (u32)(SYSCLK/98.4)) SFIL = 1;

//		BDR=int((2^24*SymbolRate*2^NDEC)/SYSCLK)
//		BDR = (BDR<<(16+NDEC))/(SYSCLK>>8);

        Symbolrate<<=NDEC;
        ratio=(Symbolrate<<4)/FIN;
        tmp=((Symbolrate<<4)%FIN)<<8;
        ratio=(ratio<<8)+tmp/FIN;
        tmp=(tmp%FIN)<<8;
        ratio=(ratio<<8)+(tmp+FIN/2)/FIN;

        BDR= ratio;

//		BDRI=int(16*SYSCLK /(SymbolRate*2^NDEC))
		BDRI  = (SYSCLK << 4) / (Symbolrate << NDEC);

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

        if (DoCLB) ClrBit1820(client);

        return 0;
}

typedef struct {
        Modulation      QAM_Mode;
        int             NoOfSym;
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
		printk("SetQAM: Einsprung \n");

        switch (QAM_Mode) {
        	case QAM_16 :
        		real_qam = 0;
				printk("QAM MODE: 16\n");
        		break;
        	case QAM_32 :
        		real_qam = 1;
				printk("QAM MODE: 32\n");
        		break;
        	case QAM_64 :
        		real_qam = 2;
				printk("QAM MODE: 64\n");
        		break;
        	case QAM_128:
        		real_qam = 3;
				printk("QAM MODE: 128\n");
        		break;
        	case QAM_256:
        		real_qam = 4;
				printk("QAM MODE: 256\n");
        		break;
        	default:
				printk("QAM MODE: !!!ERROR!!!\n");
                return -1;
        }
        ves->reg0=(ves->reg0 & 0xe3) | (real_qam << 2);

        writereg(client, 0x01, QAM_Values[real_qam].Reg1);
        writereg(client, 0x05, QAM_Values[real_qam].Reg5);
        writereg(client, 0x08, QAM_Values[real_qam].Reg8);
        writereg(client, 0x09, QAM_Values[real_qam].Reg9);
		writereg(client, 0x00, ves->reg0);

        if (DoCLB)
                ClrBit1820(client);
        return 0;
}

/* read and write STDBY bit */
int SetStdby(struct i2c_client* client, int DoStdby)
{
	u8 status;
	struct ves1820 *ves=(struct ves1820 *) client->data;
	printk("SetStdby: Einsprung \n");

	if(ves)
		status = ves->reg0;
	else
		status = readreg(client,0);

	if(DoStdby)
		status |= 0x80;
	else
		status &= ~0x80;

        writereg(client, 0x00, status );

	if(ves)
		ves->reg0 = status;

	return 0;
}

int GetStdby(struct i2c_client* client)
{
	u8 status;

	printk("GetStdby: Einsprung \n");
	status = readreg(client,0);

	return ((status>>7)&1);
}


int attach_adapter(struct i2c_adapter *adap)
{
	struct ves1820 *ves;
	struct i2c_client *client;

	client_template.adapter=adap;
	printk("attach_adapter\n");

	i2c_master_send(&client_template,NULL,0);

	client_template.adapter=adap;

	if ((readreg(&client_template, 0x1a)&0xf0)!=0x70)
		return -1;

	if (NULL == (client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
		return -ENOMEM;
	memcpy(client, &client_template, sizeof(struct i2c_client));

	client->data=ves=kmalloc(sizeof(struct ves1820),GFP_KERNEL);

	if (ves==NULL)
	{
		kfree(client);
		return -ENOMEM;
	}

        printk("VES1820: attaching VES1820 at 0x%02x\n", (client->addr)<<1);
        i2c_attach_client(client);

        init(client);

        printk("VES1820: attached to adapter %s\n\n", adap->name);
        printk("$Id: ves1820.c,v 1.26 2002/06/28 01:07:56 Homar Exp $\n");
//	MOD_INC_USE_COUNT;
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
//	MOD_DEC_USE_COUNT;
        return 0;
}

static int dvb_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	int temp;
	switch (cmd)
	{

        case FE_GET_FRONTEND:
        {
                struct frontend *front = (struct frontend *)arg;
                int sync;
				printk ("FE_GET_FRONTEND: Einsprung \n");

                front->afc=(int)((char)(readreg(client,0x19)));
					printk ("AFC: %16u\n",front->afc);
                front->afc=(front->afc*(int)(front->srate/8))/128;
					printk ("AFC / SRATE: %u / %lu %u\n",front->afc, (long)front->srate, (uint)front->srate);
                front->agc=readreg(client,0x17);
					printk ("AGC: %16u\n",front->agc);
                sync=front->sync=readreg(client,0x11);
				if (sync & DVB_SYNC_SIGNAL)
		        {
					printk ("FE_SYNC_SIGNAL\n");
				}
				if (sync & DVB_SYNC_CARRIER)
		        {
					printk ("FE_SYNC_CARRIER\n");
				}
				if (sync & DVB_SYNC_VITERBI)
		        {
					printk ("FE_SYNC_VITERBI\n");
				}
				if (sync & DVB_SYNC_FSYNC)
		        {
					printk ("FE_SYNC_FSYNC\n");
				}
				if (sync & DVB_SYNC_FRONT)
		        {
					printk ("FE_SYNC_FRONT\n");
				}
/*
#define DVB_SYNC_SIGNAL        1
#define DVB_SYNC_CARRIER       2
#define DVB_SYNC_VITERBI       4
#define DVB_SYNC_FSYNC         8
#define DVB_SYNC_FRONT        16
*/
				printk ("FRONTEND STATUS: %u\n",sync);
                front->nest=0;
				front->vber = readreg(client,0x14);
                front->vber|=(readreg(client,0x15)<<8);
                front->vber|=(readreg(client,0x16)<<16);
                break;
        }

        case FE_READ_STATUS:
		{
			FrontendStatus *status=(FrontendStatus *) arg;
			struct ves1820 *ves=(struct ves1820 *) client->data;
			int sync;
			int ber;
			printk ("FE_READ_STATUS: Einsprung \n");

			*status=0;

			sync=readreg(client,0x11);

			if (!sync)
	        {
				printk ("Searching Transponder...\n");
				ClrBit1820(client);
			}
			if (sync & 1)
	        {
				*status|=FE_HAS_POWER;
				printk ("Tracking...\n");
			}
			if (sync & 2)
	        {
				*status|=FE_HAS_LOCK;
				printk ("Demodulator has locked...\n");
			}
			if (sync & 4)
	        {
				*status|=FE_HAS_SYNC;
				printk ("MPEG2 sync pattern have been detected...\n");
			}
			if (sync & 8)
	        {
				*status|=FE_TUNER_HAS_LOCK;//austesten
				printk ("Front End Locked 8-)\n");
			}
			if (sync & 64)
	        {
				*status|=FE_SPECTRUM_INV;
				printk ("framing is not DVB compliant 8-(\n");
			}

			ber = (sync >> 4);
			printk ("bit error rate arround: ");
			printk (!ber?">10E-2\n":ber==1?"10E-3\n":ber==2?"10E-4\n":"<10E-4\n");
			printk ("Status of sync: %d\n",sync);
			printk ("Status of status: %d\n",*status);
/*
#define FE_HAS_POWER         1
#define FE_HAS_SIGNAL        2
#define FE_SPECTRUM_INV      4
#define FE_HAS_LOCK          8
#define FE_HAS_CARRIER      16
#define FE_HAS_VITERBI      32
#define FE_HAS_SYNC         64
#define FE_TUNER_HAS_LOCK  128
*/
			if (sync == 3)
			{
				printk("inv autom.\n");
				SetInversion	(client,ves->inversion==INVERSION_ON?INVERSION_AUTO:INVERSION_ON);
			}
		break;
		}

        case FE_WRITEREG:
        {
                u8 *msg = (u8 *) arg;
				printk ("WRITEREG: %16u / %16u\n",msg[0], msg[1]);
                writereg(client, msg[0], msg[1]);
                break;
        }
        case FE_INIT:
        {
				printk ("INIT: Einsprung \n");
                init(client);
                break;
        }

        case FE_RESET:
        {
				printk ("RESET: Einsprung \n");
				ClrBit1820(client);
				break;
        }
        case FE_SET_INVERSION:
        {
				struct ves1820 *ves=(struct ves1820 *) client->data;
				printk ("STATUS INVERSION: %s     ves->reg0 = 0x%x\n",ves->inversion==INVERSION_ON?"Inversion = ON":ves->inversion==INVERSION_OFF?"Inversion = OFF":"Inversion = AUTO", ves->reg0);
                SetInversion(client, (int) arg);
                break;
        }


        case FE_SET_FRONTEND:
        {
				FrontendParameters *param = (FrontendParameters *) arg;

				printk ("SET_FRONTEND: Einsprung \n");
				SetInversion(client, param->Inversion);
				SetQAM(client, param->u.qam.QAM, 1);
				SetSymbolrate(client, param->u.qam.SymbolRate, 1);
				break;
        }
		case FE_SETFREQ:
		{
				u32 freq=*(u32*)arg;
				u8 buffer[4];
				freq+=36125;
				freq/=125;

				buffer[0]=(freq>>8) & 0x7F;
				buffer[1]=freq & 0xFF;
				buffer[2]=0x80 | (((freq>>15)&3)<<6) | 4;
				buffer[3]=(uint)freq>4050?4:1;
				printk ("SETFREQ: Frequenz = %u %u %u %u \n",freq<<16, (uint)freq,(*(u32*)buffer),(*(u32*)arg));

				fp_set_tuner_dword(T_QAM, *((u32*)buffer));
				break;
		}
        default:
		{
 				printk ("dvb_command: !!!ERROR!!!\n");
 				return -1;
		}
	}

		for ( temp=0;temp==2000;temp++); //2500
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

	SetStdby(&client_template,1);

        if ((res = i2c_del_driver(&dvbt_driver)))
        {
                printk("dvb-tuner: Driver deregistration failed, "
                       "module not removed.\n");
        }
        dprintk("VES1820: cleanup\n");
}

module_init(init_ves1820);
module_exit(exit_ves1820);
