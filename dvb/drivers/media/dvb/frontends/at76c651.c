/*

    $Id: at76c651.c,v 1.6 2001/03/20 15:10:29 fnbrd Exp $

    AT76C651  - DVB demux driver (dbox-II-project)

    Homepage: http://dbox2.elxsi.de

    Copyright (C) 2001 fnbrd (fnbrd@gmx.de)

    Teile sind noch Copyright (C) 1999 Convergence Integrated Media GmbH <ralph@convergence.de>

    da zum Vergleich noch Reste des ves1820-Treibers vorhanden sind

    Habe mal angefangen in der Hoffnung das einer der Fachmaenner mir weiterhelfen
    kann oder es selbst machen will.

    Das Datenblatt findet sich unter http://www.atmel.com/atmel/acrobat/doc1293.pdf


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

    $Log: at76c651.c,v $
    Revision 1.6  2001/03/20 15:10:29  fnbrd
    Kleine Aenderung bzgl. Berechnung der Symbolrate

    Revision 1.5  2001/03/16 13:04:16  fnbrd
    Alle Interrupt-Routinen auskommentiert (task), da sie auch beim alten Treiber
    anscheinend nichts gemacht habe.

    Revision 1.4  2001/03/16 12:27:29  fnbrd
    Symbolrate wird berechnet.


*/

#include <linux/delay.h>	/* for mdelay */
#include <linux/kernel.h>
#include <linux/module.h>	/* for module-version */
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <dbox/dvb.h>

#include <asm/8xx_immap.h>

#include <dbox/ves.h>
/*
  exported functions:
    void ves_write_reg(int reg, int val);
    void ves_init(void);
    void ves_set_frontend(struct frontend *front);
    void ves_get_frontend(struct frontend *front);
*/
//EXPORT_SYMBOL(ves_write_reg); // Zur Sicherheit auskommentiert.
EXPORT_SYMBOL(ves_init);
EXPORT_SYMBOL(ves_set_frontend);
EXPORT_SYMBOL(ves_get_frontend);

// Da im irq (task) beim ves1820 anscheinend nichts gemacht
// irqs komplett auskommentiert
//#define VES_INTERRUPT		14
//static void ves_interrupt(int irq, void *vdev, struct pt_regs * regs);

#ifdef MODULE
MODULE_PARM(debug,"i");
#endif

static int debug = 9;
#define dprintk	if (debug) printk

static struct i2c_driver dvbt_driver;
static struct i2c_client client_template, *dclient;
/*
u8 Init1820PTab[] =
{
  0x49, 0x6A, 0x13, 0x0A, 0x15, 0x46, 0x26, 0x1A,               // changed by tmbinc, according to sniffed i2c-stuff. not validated at all.
  0x43, 0x6A, 0x1A, 0x61, 0x19, 0xA1, 0x63, 0x00,
  0xB8, 0x00, 0xE1, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x32, 0xc8, 0x00, 0x00
};
*/

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
/*
int ves_task(void*);

struct tq_struct ves_tasklet=
{
	routine: ves_task,
	data: 0
};
*/
struct ves1820 {
//        int inversion;
        u32 srate;
        u8 pwm;
//        u8 reg0;
};


int writereg(struct i2c_client *client, u8 reg, u8 data)
{
        int ret;
        u8 msg[2];

        msg[0]=reg; msg[1]=data;
        ret=i2c_master_send(client, msg, 2);
        if (ret!=2)
                printk("writereg error\n");
    /*    printk("writereg: [%02x]=%02x\n", reg, data); */
        mdelay(10);
        return ret;
}

u8 readreg(struct i2c_client *client, u8 reg)
{
        struct i2c_adapter *adap=client->adapter;
        unsigned char mm1[1];
        unsigned char mm2[] = {0x00};
        struct i2c_msg msgs[2];

        msgs[0].flags=0;
        msgs[1].flags=I2C_M_RD;
        msgs[0].addr=msgs[1].addr=client->addr;
        mm1[0]=reg;
        msgs[0].len=1; msgs[1].len=1;
        msgs[0].buf=mm1; msgs[1].buf=mm2;
        i2c_transfer(adap, msgs, 2);

        return mm2[0];
}

int init(struct i2c_client *client)
{
        struct ves1820 *ves=(struct ves1820 *) client->data;

        dprintk("AT76C651: init chip\n");
	ves->pwm=readreg(client, 0x17);
        printk("AT76C651: pwm=%02x\n", ves->pwm);
        if (ves->pwm == 0xff) // Richtig?
           ves->pwm=0x48;
        if (writereg(client, 0, 0)<0)
          printk("AT76C651: send error\n");
        // SETAUTOCFG (setzt alles bis auf SYMRATE, QAMSEL und BBFREQ)
	writereg(client, 0x06, 0x01);
        // Performance optimieren (laut Datenblatt)
	writereg(client, 0x10, 0x06);
	writereg(client, 0x11, 0x10);
	writereg(client, 0x15, 0x28);
	writereg(client, 0x20, 0x09);
	writereg(client, 0x24, 0x90);
	writereg(client, 0x30, 0x94);
	writereg(client, 0x35, 0x2a); // nur bei 32-QAM
	writereg(client, 0x37, 0x13); // nur bei 32-QAM

//        ves->inversion=0;
        ves->srate=0;
//        ves->reg0=Init1820PTab[0];
        // PWM setzen
        writereg(client, 0x17, ves->pwm);
        // Und ein Reset
	writereg(client, 0x07, 0x01);

        // Noch mehr noetig?

	return 0;

/* VES1820:

        struct i2c_adapter *adap=client->adapter;
        struct i2c_msg msgs[2];
        unsigned char mm1[] = {0xff};
        unsigned char mm2[] = {0x00};
        int i;

        msgs[0].flags=0; // write
        msgs[1].flags=1; // read
        msgs[0].addr=msgs[1].addr=(0x28<<1);
        mm1[0]=0xff;
        msgs[0].len=1; msgs[1].len=1;
        msgs[0].buf=mm1; msgs[1].buf=mm2;
        i2c_transfer(adap, msgs, 2);
        ves->pwm=*mm2;
        printk("AT76C651: pwm=%02x\n", ves->pwm);
        if (ves->pwm == 0xff)
                ves->pwm=0x48;

        if (writereg(client, 0, 0)<0)
                printk("AT76C651: send error\n");
        for (i=0; i<53; i++)
                writereg(client, i, Init1820PTab[i]);

        writereg(client, 0, Init1820PTab[0]&~1);
        writereg(client, 0, Init1820PTab[0]|1);
        mdelay(1000);

        ves->inversion=0;
        ves->srate=0;
        ves->reg0=Init1820PTab[0];

        writereg(client, 0x34, ves->pwm);
        return 0;
*/
}

/*
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
*/

void SetPWM(struct i2c_client* client)
// puts pwm as pulse width modulated signal to pin FE_LOCK
{

        struct ves1820 *ves=(struct ves1820 *) client->data;

	dprintk("AT76C651: SetPWM\n");

        writereg(client, 0x17, ves->pwm); // Angepasst an AT... muss aber noch ueberprueft werden

//        writereg(client, 0x34, ves->pwm);
	return;
}

// Meine Box hat einen Quartz mit 28.9
// Tabelle zum Berechnen des Exponenten zur gegebenen Symbolrate (fref=69.6 MHz)
u32 expTab[] = {
  67969, 135938, 271875, 543750,
  1087500, 2175000, 4350000, 8700000
};
/*
// Tabelle zum Berechnen des Exponenten zur gegebenen Symbolrate (fref=4.35 MHz)
u32 expTab[] = {
  4249, 8497, 16993, 33985,
  67969, 135938, 271875, 543750
};
*/

int SetSymbolrate(struct i2c_client* client, u32 Symbolrate, int DoCLB)
{
        struct ves1820 *ves=(struct ves1820 *) client->data;
#define FREF 69600000UL
        u32 mantisse;
        u8 exp;
	int i;
        u32 tmp;
	u32 fref;
        if (Symbolrate > FREF/2)
                Symbolrate=FREF/2;
        if (Symbolrate < 500000)
                Symbolrate=500000;

        ves->srate=Symbolrate;
	dprintk("AT76C651: SetSymbolrate\n");

        for(i=6; i>2; i--)
          if(Symbolrate>=expTab[i])
            break;
        exp=i;
        // Ich Kernel-Dummy, Kernel nix float -> haendisch div.
        // Wenn's nur nicht so lange her waere
//  	mantisse= ((float)Symbolrate/FREF)*(1<<(30-exp));
	// Achtung: stimmt momentan nur wenn exp=6 d.h. Symbolrate >= 4350000
        // fuer kleinere Symbolraten muss ich's nochmal durchrechnen.
  	tmp=Symbolrate;
        fref=FREF;
  	mantisse=tmp/fref;
  	tmp=(tmp%fref)<<8;
  	mantisse=(mantisse<<8)+tmp/fref;
  	tmp=(tmp%fref)<<8;
  	mantisse=(mantisse<<8)+tmp/fref;
  	tmp=(tmp%fref)<<8;
  	mantisse=(mantisse<<8)+tmp/fref;

	dprintk("exp: %02x mantisse: %x\n", exp, mantisse);
        // Exponent und Mantisse Bits 0-4 setzen
        writereg(client, 0x02, ((mantisse&0x0000001f)<<3)|exp);
        mantisse>>=5;
        // Mantisse Bits 5-12
        writereg(client, 0x01, mantisse&0x000000ff);
        mantisse>>=8;
        // Mantisse Bits 13-20
        writereg(client, 0x00, mantisse&0x000000ff);
	// Und ein reset um das Dingens richtig einzustellen
  	writereg(client, 0x07, 0x01);
  	return 0;

/* VES1820:

// #define XIN 57840000UL
// #define FIN (57840000UL>>4)
        // the dbox seems to use another crystal, 69.6 Mhz.
//#define XIN 69600000UL
//#define FIN (XIN>>4)
        s32 BDR;
        s32 BDRI;
        s16 SFIL=0;
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
*/
}

typedef enum QAM_TYPE
{	QAM_16,
	QAM_32,
	QAM_64,
	QAM_128,
	QAM_256
} QAM_TYPE, *PQAM_TYPE;

/*
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
*/

int SetQAM(struct i2c_client* client, QAM_TYPE QAM_Mode, int DoCLB)
{
	u8 qamsel;

dprintk("AT76C651: set QAM\n");
// Was ist DoCLB?

	// Read QAMSEL
	qamsel=readreg(client, 0x03); 	// Nur weil ich keine Ahnung
	qamsel &= 0xf0;			// von der Bedeutung der 4 anderen Bits habe
        qamsel |= QAM_Mode+4; // 4 = QAM-16
        writereg(client, 0x03, qamsel);
	// Und ein reset um das Dingens richtig einzustellen
  	writereg(client, 0x07, 0x01);
  	return 0;

/* VES1820:
        struct ves1820 *ves=(struct ves1820 *) client->data;

        ves->reg0=(ves->reg0 & 0xe3) | (QAM_Mode << 2);

        writereg(client, 0x01, QAM_Values[QAM_Mode].Reg1);
        writereg(client, 0x05, QAM_Values[QAM_Mode].Reg5);
        writereg(client, 0x08, QAM_Values[QAM_Mode].Reg8);
        writereg(client, 0x09, QAM_Values[QAM_Mode].Reg9);

        if (DoCLB)
                ClrBit1820(client);
        return 0;
*/
}


int attach_adapter(struct i2c_adapter *adap)
{
        struct ves1893 *ves;
        struct i2c_client *client;

//		unsigned long flags;
u8 t;

        client_template.adapter=adap;

/*        if (i2c_master_send(&client_template,NULL,0))
                return -1;
*/
        client_template.adapter=adap;

        if (readreg(&client_template, 0x0e)!=0x65) {
          printk("no AT76C651\n");
          return -1;
	}
        if (readreg(&client_template, 0x0f)!=0x10)
        {
          if (readreg(&client_template, 0x0f)==0x11)
            printk("no AT76C651 found, but a AT76C651B\n");
          return -1;
        }

t=readreg(&client_template, 0x17);
printk("AT76C651: pwm=%02x\n", t);

dprintk("AT76C651: Gefunden, aber Treiber noch nicht fertig\n");
return -1;


//        printk("feID: 1820 %x\n", readreg(&client_template, 0x1a));

        if (NULL == (client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
                return -ENOMEM;

        memcpy(client, &client_template, sizeof(struct i2c_client));
        dclient=client;

        client->data=ves=kmalloc(sizeof(struct ves1820),GFP_KERNEL);
        if (ves==NULL) {
                kfree(client);
                return -ENOMEM;
        }

        printk("AT76C651: attaching AT76C651 at 0x%02x\n", (client->addr)<<1);
        i2c_attach_client(client);

        init(client);

        printk("AT76C651: attached to adapter %s\n", adap->name);

        /* mask interrupt */
        // Wir brauchen keine irqs
	writereg(client, 0x0b, 0x00);

/*
// Kein Plan was gewuenscht/benoetigt wird
// Moegliche Trigger: Pins LOCK1/LOCK2, singal loss, frame rate lost und per frame-timer
// writereg(client, 0x0b, mask);

		writereg(client, 0x32 , 0x80 | (1<<3));

		if (request_8xxirq(VES_INTERRUPT, ves_interrupt, SA_ONESHOT, "at76c651", NULL) != 0)
		{
			i2c_del_driver(&dvbt_driver);
			dprintk("AT76C651: can't request interrupt\n");
	        return -EBUSY;
		}
*/
        return 0;
}

int detach_client(struct i2c_client *client)
{
        printk("AT76C651: detach_client\n");
        i2c_detach_client(client);
        kfree(client->data);
        kfree(client);
        return 0;
}

/*
void ves_write_reg(int reg, int val)
{
  writereg(dclient, reg, val);
}
*/

void ves_init(void)
{
  init(dclient);
}

void ves_set_frontend(struct frontend *front)
{
//  if (front->flags&FRONT_FREQ_CHANGED)
//    ClrBit1820(dclient); 	// Auskommentiert da keine Ahnung, Reset?
				// ja -> writereg(client, 0x07, 0x01)
  SetQAM(dclient, front->qam, front->flags&7);
  SetSymbolrate(dclient, front->srate, front->flags&7);
}

void ves_get_frontend(struct frontend *front)
{
/*
  front->type=FRONT_DVBC;
  front->afc=(int)((char)(readreg(dclient,0x19)));
  front->afc=(front->afc*(int)(front->srate/8))/128;
  front->agc=(readreg(dclient,0x17)<<8);
  front->sync=readreg(dclient,0x11);
  front->nest=0;

  front->vber = readreg(dclient,0x14);
  front->vber|=(readreg(dclient,0x15)<<8);
  front->vber|=(readreg(dclient,0x16)<<16);
*/
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
        "AT76C651 DVB DECODER",
        I2C_DRIVERID_EXP2, // experimental use id
//        I2C_DRIVERID_VES1820,
        I2C_DF_NOTIFY,
        attach_adapter,
        detach_client,
        0,
        inc_use,
        dec_use,
};

static struct i2c_client client_template = {
        "AT76C651",
        I2C_DRIVERID_EXP2, // experimental use id
//        I2C_DRIVERID_VES1820,
        0,
        0x0d,
//        (0x10 >> 1),
        NULL,
        &dvbt_driver,
        NULL
};

/* ---------------------------------------------------------------------- */

/*
int ves_task(void*dummy)
{
	u8 status;
	u32 vber;

	status = readreg(dclient, 0x33);

	/( read ber
	if (status&(1<<3))
	{
//		printk("READ BER\n");
		vber = readreg(dclient,0x14);
		vber|=(readreg(dclient,0x15)<<8);
		vber|=(readreg(dclient,0x16)<<16);
	}

	enable_irq(VES_INTERRUPT);

	return 0;
}
*/
/* ---------------------------------------------------------------------- */
/*
static void ves_interrupt(int irq, void *vdev, struct pt_regs * regs)
{
	schedule_task(&ves_tasklet);
}
*/
/* ---------------------------------------------------------------------- */

#ifdef MODULE
int init_module(void) {
        int res;

        if ((res = i2c_add_driver(&dvbt_driver)))
        {
                printk("AT76C651: Driver registration failed, module not inserted.\n");
                return res;
        }
        if (!dclient)
        {
                i2c_del_driver(&dvbt_driver);
                printk("AT76C651: not found.\n");
                return -EBUSY;
        }

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

//		free_irq(VES_INTERRUPT, NULL);

        dprintk("AT76C651: cleanup\n");
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

