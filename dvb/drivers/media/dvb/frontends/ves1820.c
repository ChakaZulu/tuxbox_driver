/* 
    VES1820  - 

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
    Revision 1.14  2001/05/03 20:27:21  gillem
    - fix debug output

    Revision 1.13  2001/04/07 01:44:54  tmbinc
    put setfreq into frontend-driverstruct.

    Revision 1.12  2001/03/29 19:10:03  gillem
    - fix a patch from tmb

    Revision 1.11  2001/03/29 02:13:25  tmbinc
    Added register_demod, unregister_dmod. be sure to use new load script

    Revision 1.10  2001/03/19 18:19:25  gillem
    - add ioctl GET_UNCORRECTABLE_BLOCKS

    Revision 1.9  2001/03/17 13:34:36  gillem
    - fix for multiple interrupts at same time

    Revision 1.8  2001/03/17 11:32:57  gillem
    - add interrupt stuff


    $Revision: 1.14 $
*/

/* ---------------------------------------------------------------------- */

#include <linux/delay.h>	/* for mdelay */
#include <linux/kernel.h>
#include <linux/module.h>	/* for module-version */
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <asm/8xx_immap.h>

#include <dbox/dvb.h>
#include <dbox/ves.h>
#include <dbox/fp.h>

/* ---------------------------------------------------------------------- */

static void ves_write_reg(int reg, int val);
static void ves_init(void);
static void ves_set_frontend(struct frontend *front);
static void ves_get_frontend(struct frontend *front);
static int ves_get_unc_packet(uint32_t *uncp);
static int mitel_set_freq(int freq);

struct demod_function_struct ves1820={ves_write_reg, ves_init, ves_set_frontend, ves_get_frontend, ves_get_unc_packet, mitel_set_freq};

#define VES_INTERRUPT		14
static void ves_interrupt(int irq, void *vdev, struct pt_regs * regs);

static int debug = 0;

#ifdef MODULE
MODULE_PARM(debug,"i");
#endif

#define dprintk	if (debug) printk

static struct i2c_driver dvbt_driver;
static struct i2c_client client_template, *dclient;

/* ---------------------------------------------------------------------- */

u8 Init1820PTab[] =
{
  // changed by tmbinc, according to sniffed i2c-stuff. not validated at all.
  0x49, 0x6A, 0x13, 0x0A, 0x15, 0x46, 0x26, 0x1A,
  0x43, 0x6A, 0x1A, 0x61, 0x19, 0xA1, 0x63, 0x00,
  0xB8, 0x00, 0xE1, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x32, 0xc8, 0x00, 0x00
};

/*
u8 Init1820PTab[] =
{
  0x69, 0x6A, 0x9B, 0x0A, 0x52, 0x46, 0x26, 0x1A,
  0x43, 0x6A, 0xAA, 0xAA, 0x1E, 0x85, 0x43, 0x28,
  0xE0, 0x00, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x40
};
*/

/* ---------------------------------------------------------------------- */

static void ves_task(void*);

typedef struct ves1820_s {
        int inversion;
        u32 srate;
        u8 pwm;
        u8 reg0;
	u32 ber;
	u8 sync;
	u32 uncp; /* uncorrectable packet */
} ves1820_t;

struct tq_struct ves_tasklet=
{
	routine: ves_task,
	data: 0
};

/* ---------------------------------------------------------------------- */

int writereg(struct i2c_client *client, u8 reg, u8 data)
{
        int ret;
        u8 msg[] = {0x00, 0x1f, 0x00};
        
        msg[1]=reg; msg[2]=data;
        ret=i2c_master_send(client, msg, 3);
        if (ret!=3) 
                dprintk("ves1820.o: writereg error\n");
        mdelay(10);
        return ret;
}

/* ---------------------------------------------------------------------- */

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

/* ---------------------------------------------------------------------- */

int init(struct i2c_client *client)
{
        ves1820_t *ves=(ves1820_t *) client->data;
        struct i2c_adapter *adap=client->adapter;
        struct i2c_msg msgs[2];
        unsigned char mm1[] = {0xff};
        unsigned char mm2[] = {0x00};
        int i;
        
        printk("ves1820.o: init chip\n");

        msgs[0].flags=0;
        msgs[1].flags=1;
        msgs[0].addr=msgs[1].addr=(0x28<<1);
        mm1[0]=0xff;
        msgs[0].len=1; msgs[1].len=1;
        msgs[0].buf=mm1; msgs[1].buf=mm2;
        i2c_transfer(adap, msgs, 2);
        ves->pwm=*mm2;
        printk("ves1820.o: pwm=%02x\n", ves->pwm);
        if (ves->pwm == 0xff)
                ves->pwm=0x48;
       
        if (writereg(client, 0, 0)<0)
                dprintk("ves1820.o: send error\n");
        for (i=0; i<53; i++)
                writereg(client, i, Init1820PTab[i]);

        writereg(client, 0, Init1820PTab[0]&~1);
        writereg(client, 0, Init1820PTab[0]|1);
        mdelay(1000);
  
        ves->inversion=0;
        ves->srate=0;
        ves->reg0=Init1820PTab[0];
	ves->ber = 0xFFFFFFFF;
	ves->sync = 0;
	ves->uncp = 0;

        writereg(client, 0x34, ves->pwm);
        return 0;
}

/* ---------------------------------------------------------------------- */

void ClrBit1820(struct i2c_client *client)
{
        ves1820_t *ves=(ves1820_t *) client->data;
        u8 val;
        
        val=ves->reg0;
        if (ves->inversion)
          val&=0xdf;
        writereg(client, 0, val & 0xfe);
        writereg(client, 0, val);
}

/* ---------------------------------------------------------------------- */

void SetPWM(struct i2c_client* client) 
// puts pwm as pulse width modulated signal to pin FE_LOCK
{
        ves1820_t *ves=(ves1820_t *) client->data;

        writereg(client, 0x34, ves->pwm); 
}

/* ---------------------------------------------------------------------- */

int SetSymbolrate(struct i2c_client* client, u32 Symbolrate, int DoCLB)
{
        ves1820_t *ves=(ves1820_t *) client->data;
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
        
        NDEC = (NDEC << 6) | Init1820PTab[0x03];	// Building the Regs

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

/* ---------------------------------------------------------------------- */

typedef enum QAM_TYPE
{	QAM_16,
	QAM_32,
	QAM_64,
	QAM_128,
	QAM_256
} QAM_TYPE, *PQAM_TYPE;

typedef struct {
        QAM_TYPE        QAM_Mode;
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

int SetQAM(struct i2c_client* client, QAM_TYPE QAM_Mode, int DoCLB)
{
        ves1820_t *ves=(ves1820_t *) client->data;
        
        ves->reg0=(ves->reg0 & 0xe3) | (QAM_Mode << 2);

        writereg(client, 0x01, QAM_Values[QAM_Mode].Reg1);
        writereg(client, 0x05, QAM_Values[QAM_Mode].Reg5);
        writereg(client, 0x08, QAM_Values[QAM_Mode].Reg8);
        writereg(client, 0x09, QAM_Values[QAM_Mode].Reg9);
        
        if (DoCLB) 
                ClrBit1820(client);
        return 0;
}

/* ---------------------------------------------------------------------- */

int attach_adapter(struct i2c_adapter *adap)
{
        ves1820_t *ves;
        struct i2c_client *client;

        client_template.adapter=adap;
        
/*        if (i2c_master_send(&client_template,NULL,0))
                return -1;
*/
        client_template.adapter=adap;
                        
        if ((readreg(&client_template, 0x1a)&0xf0)!=0x70)
        {
          if ((readreg(&client_template, 0x1e)&0xF0)==0xD0)
            dprintk("ves1820.o: no VES1820 found, but a VES1893\n");
          return -1;
        }
        
        printk("ves1820.o: feID: 1820 %x\n", readreg(&client_template, 0x1a));
        
        if (NULL == (client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
                return -ENOMEM;
        
        memcpy(client, &client_template, sizeof(struct i2c_client));
        dclient=client;
        
        client->data=ves=kmalloc(sizeof(ves1820_t),GFP_KERNEL);
        if (ves==NULL) {
                kfree(client);
                return -ENOMEM;
        }
       
        printk("ves1820.o: attaching VES1820 at 0x%02x\n", (client->addr)<<1);
        i2c_attach_client(client);
        
        init(client);

	if (register_demod(&ves1820))
 		printk("ves1820.o: can't register demod.\n");
	/* mask interrupt */
	writereg(client, 0x32 , 0x80 | 1 | (1<<1) | (1<<2) | (1<<3));

	ves_tasklet.data = (void*)client->data;

	if (request_8xxirq(VES_INTERRUPT, ves_interrupt, SA_ONESHOT, "ves1820", NULL) != 0)
 	{
		i2c_del_driver(&dvbt_driver);
		dprintk("ves1820.o: can't request interrupt\n");
		return -EBUSY;
	}

        return 0;
}

int detach_client(struct i2c_client *client)
{
        printk("ves1820.o: detach_client\n");
        i2c_detach_client(client);
        kfree(client->data);
        kfree(client);
        unregister_demod(&ves1820);
        return 0;
}

/* ---------------------------------------------------------------------- */

void ves_write_reg(int reg, int val)
{
	writereg(dclient, reg, val);
}

void ves_init(void)
{
	init(dclient);
}

void ves_set_frontend(struct frontend *front)
{                
//  if (front->flags&FRONT_FREQ_CHANGED)
	ClrBit1820(dclient);
	SetQAM(dclient, front->qam, front->flags&7);
	SetSymbolrate(dclient, front->srate, front->flags&7);
}

void ves_get_frontend(struct frontend *front)
{
	ves1820_t *ves = (ves1820_t*)dclient->data;

	if(!ves)
		return;

	front->type=FRONT_DVBC;
	front->afc=(int)((char)(readreg(dclient,0x19)));
	front->afc=(front->afc*(int)(front->srate/8))/128;
	front->agc=(readreg(dclient,0x17)<<8);
	front->nest=0;

	front->sync = ves->sync;
	front->vber = ves->ber;
/*
	front->sync=readreg(dclient,0x11);
	front->vber = readreg(dclient,0x14);
	front->vber|=(readreg(dclient,0x15)<<8);
	front->vber|=(readreg(dclient,0x16)<<16);
*/
} 

int ves_get_unc_packet(u32 *uncp)
{
	ves1820_t *ves = (ves1820_t*)dclient->data;

	*uncp=ves->uncp;

	return 0;
}

/* ---------------------------------------------------------------------- */

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
        "VES1820 DVB DECODER",
        I2C_DRIVERID_VES1820,
        I2C_DF_NOTIFY,
        attach_adapter,
        detach_client,
        0,
        inc_use,
        dec_use,
};

static struct i2c_client client_template = {
        "VES1820",
        I2C_DRIVERID_VES1820,
        0,
        (0x10 >> 1),
        NULL,
        &dvbt_driver,
        NULL
};

/* ---------------------------------------------------------------------- */

static void ves_task(void*data)
{
	u8 status;
	ves1820_t *ves = (ves1820_t*)data;

	status = readreg(dclient, 0x33);

	/* ves (not) synchronized */
	if ( status&(1|(1<<1)) )
	{
		ves->sync = readreg(dclient,0x11);

		if( ves->sync&0x1f )
		{
			dprintk("ves1820.o: synchronized %02X\n",ves->sync);
		}
		else
		{
			dprintk("ves1820.o: not synchronized %02X\n",ves->sync);
		}
	}

	/* uncorrectable packet */
	if ( status&(1<<2) )
	{
		// TODO: overflow check ???
		ves->uncp++;

		dprintk("ves1820.o: uncorrectable packet %04X\n",ves->uncp);
	}

	/* ber changed */
	if (status&(1<<3))
	{
//		printk("READ BER\n");
		ves->ber = readreg(dclient,0x14);
		ves->ber|=(readreg(dclient,0x15)<<8);
		ves->ber|=(readreg(dclient,0x16)<<16);
	}

	enable_irq(VES_INTERRUPT);
}

/* ---------------------------------------------------------------------- */

static int mitel_set_freq(int freq)
{
	u8 buffer[4];
	freq+=36125000;
	freq/=62500;
		
	buffer[0]=(freq>>8)&0x7F;
	buffer[1]=freq&0xFF;
	buffer[2]=0x80 | (((freq>>15)&3)<<6) | 5;
	buffer[3]=1;
		
	fp_set_tuner_dword(T_QAM, *((u32*)buffer));
	return 0;
}

/* ---------------------------------------------------------------------- */

static void ves_interrupt(int irq, void *vdev, struct pt_regs * regs)
{
	schedule_task(&ves_tasklet);
}

/* ---------------------------------------------------------------------- */

#ifdef MODULE
int init_module(void) {
        int res;
        
        if ((res = i2c_add_driver(&dvbt_driver))) 
        {
                printk("ves1820.o: Driver registration failed, module not inserted.\n");
                return res;
        }

        if (!dclient)
        {
                i2c_del_driver(&dvbt_driver);
                printk("ves1820.o: VES not found.\n");
                return -EBUSY;
        }

	return 0;
}

void cleanup_module(void)
{
        int res;

	free_irq(VES_INTERRUPT, NULL);

        if ((res = i2c_del_driver(&dvbt_driver))) 
        {
                printk("ves1820.o: Driver deregistration failed, "
                       "module not removed.\n");
        }

        printk("ves1820.o: cleanup\n");
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

