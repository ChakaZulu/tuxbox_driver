/*

    $Id: at76c651.c,v 1.12 2001/04/24 20:36:16 fnbrd Exp $

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
    Revision 1.12  2001/04/24 20:36:16  fnbrd
    Kleinere Aenderungen.

    Revision 1.11  2001/04/24 01:56:36  fnbrd
    Koennte jetzt funktionieren.

    Revision 1.10  2001/03/29 02:13:25  tmbinc
    Added register_demod, unregister_dmod. be sure to use new load script

    Revision 1.9  2001/03/22 18:00:08  fnbrd
    Kleinigkeiten.

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

#define DONT_USE_IRQ // Wir brauchen die IRQ-Routine nicht

static void ves_write_reg(int reg, int val) {
  return; // dummy, andere koennen/sollen mit den Registern nichts anfangen
}

static void ves_init(void);
static void ves_set_frontend(struct frontend *front);
static void ves_get_frontend(struct frontend *front);
static void ves_tuner_i2c(int an);
static int ves_get_unc_packet(u32 *uncp);
static int tuner_set_freq(int freq);
static int tuner_initialized=0;
static int attach_adapter(struct i2c_adapter *adap);
static int detach_client(struct i2c_client *client);
static void at_restart(void);

struct demod_function_struct at76c651={
	write_reg:		ves_write_reg,
	init:			ves_init,
	set_frontend:	 	ves_set_frontend,
	get_frontend:		ves_get_frontend,
	get_unc_packet:		ves_get_unc_packet,
	set_frequency:		tuner_set_freq,
	set_sec:		0,
	send_diseqc:		0,
	sec_status:		0
};

#ifndef DONT_USE_IRQ
  #define VES_INTERRUPT		14
  static void ves_interrupt(int irq, void *vdev, struct pt_regs * regs);
#endif // DONT_USE_IRQ

#ifdef MODULE
MODULE_PARM(debug,"i");
#endif

static int debug = 9;
#define dprintk	if (debug) printk

static struct i2c_client *dclient=0;

// ------------------------------------ Tuner ---------------------------------------

#define TUNER_I2C_DRIVERID  0xF0C2

static int tuner_detach_client(struct i2c_client *tuner_client);
static int tuner_attach_adapter(struct i2c_adapter *adapter);
static int set_tuner_dword(u32 tw);
static struct i2c_client *dclient_tuner=0;

struct tuner_data
{
	u32 lastwrite;
};

struct tuner_data *tunerdata=0;

static struct i2c_driver tuner_driver = {
        "AT Tuner driver",
        TUNER_I2C_DRIVERID,
        I2C_DF_NOTIFY,
        &tuner_attach_adapter,
        &tuner_detach_client,
        0,
        0,
        0,
};

static struct i2c_client client_template_tuner = {
        "AT tuner",
        TUNER_I2C_DRIVERID,
        0,
        0x61, // = 0xc2 >> 1
        NULL,
        &tuner_driver,
        NULL
};

static int tuner_detach_client(struct i2c_client *tuner_client)
{
	int err;

	if ((err=i2c_detach_client(tuner_client)))
	{
		dprintk("AT76C651: couldn't detach tuner client driver.\n");
		return err;
	}

	kfree(tuner_client);
	return 0;
}

static int tuner_attach_adapter(struct i2c_adapter *adap)
{
        struct i2c_client *client;

        client_template_tuner.adapter=adap;

        if (NULL == (client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
                return -ENOMEM;

        memcpy(client, &client_template_tuner, sizeof(struct i2c_client));
        dclient_tuner=client;

        client->data=tunerdata=kmalloc(sizeof(struct tuner_data),GFP_KERNEL);

        if (tunerdata==NULL) {
          kfree(client);
          return -ENOMEM;
        }

        printk("AT76C651: attaching tuner at 0x%02x\n", (client->addr)<<1);
        i2c_attach_client(client);

        printk("AT76C651: tuner attached to adapter %s\n", adap->name);

  return 0;
}

static int tuner_init(void)
{
	int res;

	ves_tuner_i2c(1); //enable tuner access on at76c651
	printk("AT76C651: DBox2 tuner driver\n");
	if ((res=i2c_add_driver(&tuner_driver)))
	{
		printk("AT76C651: tuner driver registration failed!!!\n");
		return res;
	}

	if (!dclient_tuner)
	{
		i2c_del_driver(&tuner_driver);
		printk("AT76C651: Couldn't find tuner.\n");
		return -EBUSY;
	}
	ves_tuner_i2c(0); //disable tuner access on at76c651
//	tuner_set_freq(1198000000);

	printk("AT76C651: DBox2 tuner driver init fertig\n");
	return 0;
}

static int tuner_close(void)
{
	int res;

	if ((res=i2c_del_driver(&tuner_driver)))
	{
		dprintk("AT76C651: tuner driver unregistration failed.\n");
		return res;
	}

	return 0;
}

static int set_tuner_dword(u32 tw)
{
	char msg[4];
	int len=4;
	dprintk("AT76C651: set_tuner_dword: 0x%08x\n", tw);

	*((u32*)(msg))=tw;

	ves_tuner_i2c(1); //enable tuner access on at76c651
	if (i2c_master_send(dclient_tuner, msg, len)!=len)
	{
		return -1;
	}
	ves_tuner_i2c(0); //disable tuner access on at76c651
	tunerdata->lastwrite=tw;
	return -1;
}

static int tuner_set_freq(int freq)
{
  u32 dw=0;
  printk("AT76C651: tuner_set_freq: %d\n", freq);
  // Rechne mal wer, ich weiss nicht mal welcher Tuner :(
//  unsigned dw=0x17e28e06+(freq-346000000UL)/8000000UL*0x800000;
  switch(freq) {
    case 474000000:
      dw=0x1fe28e85; // Geschaetzt
      break;
    case 466000000:
      dw=0x1f628e85; // Geschaetzt
      break;
    case 458000000:
      dw=0x1ee28e85; // Geschaetzt
      break;
    case 450000000:
      dw=0x1e628e85; // Geschaetzt
      break;
    case 442000000:
      dw=0x1de28e85;
      break;
    case 434000000:
      dw=0x1d628e85;
      break;
    case 426000000:
      dw=0x1ce28e85;
      break;
    case 418000000:
      dw=0x1c628e85; // Geschaetzt
      break;
    case 410000000:
      dw=0x1be28e85;
      break;
    case 402000000:
      dw=0x1b628e85;
      break;
    case 394000000:
      dw=0x1ae28e06;
      break;
    case 386000000:
      dw=0x1a628e06;
      break;
    case 378000000:
      dw=0x19e28e06;
      break;
    case 370000000:
      dw=0x19628e06;
      break;
    case 362000000:
      dw=0x18e28e06;
      break;
    case 354000000:
      dw=0x18628e06;
      break;
    case 346000000:
      dw=0x17e28e06;
      break;
    default:
      printk("AT76C651: Frequency %d not supported\n", freq);
      break;
  }
  if(dw) {
    set_tuner_dword(dw);
    at_restart(); // Und den AT neu starten
    return 0;
  }
  else
    return -1;
}

//-------------------------------------------------------------------------

typedef struct at76c651 {
//        int inversion;
        u32 srate;
	u32 ber;
        u8 pwm;
	u8 sync;
	u32 uncp; /* uncorrectable packet */
//        u8 reg0;
} at76c651_t;


static int writereg(struct i2c_client *client, u8 reg, u8 data)
{
        int ret;
        u8 msg[2];

        msg[0]=reg; msg[1]=data;
        ret=i2c_master_send(client, msg, 2);
        if (ret!=2)
                printk("writereg error\n");
    /*    printk("writereg: [%02x]=0x%02x\n", reg, data); */
        mdelay(10);
        return ret;
}

static u8 readreg(struct i2c_client *client, u8 reg)
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

// Tuner an i2c an/abhaengen
static void ves_tuner_i2c(int an)
{
  if(an) {
    writereg(dclient, 0x0c, 0xc2|1);
    dprintk("AT76C651: tuner now attached to i2c at 0xc2\n");
  }
  else {
    writereg(dclient, 0x0c, 0xc2);
    dprintk("AT76C651: tuner now detached from i2c\n");
  }
}

// Und ein Restart
static void at_restart(void)
{
	writereg(dclient, 0x07, 0x01);
}

static int init(struct i2c_client *client)
{

        struct at76c651 *ves=(struct at76c651 *) client->data;

        dprintk("AT76C651: init chip\n");

	// BBFREQ
	writereg(client, 0x04, 0x3f);
	writereg(client, 0x05, 0xee);

	set_tuner_dword(0x19628e06); // (370000)

	// SYMRATE 6900000
	writereg(client, 0x00, 0xf4);
	writereg(client, 0x01, 0x7c);
	writereg(client, 0x02, 0x06);

	// QAMSEL 64 QAM
	writereg(client, 0x03, 0x06);

        // SETAUTOCFG (setzt alles bis auf SYMRATE, QAMSEL und BBFREQ)
	writereg(client, 0x06, 0x01);

        // Performance optimieren (laut Datenblatt)
	writereg(client, 0x10, 0x06);
	writereg(client, 0x11, 0x10);
	writereg(client, 0x15, 0x28); // BBCFG
//	writereg(client, 0x20, 0x09); // ?
	writereg(client, 0x24, 0x90); // TIMLOOPCFG
	writereg(client, 0x30, 0x90);
//	writereg(client, 0x30, 0x94);

        // Und ein Restart
	at_restart();
//	writereg(client, 0x07, 0x01);

        /* mask interrupt */
	// Moegliche Trigger: Pins LOCK1/LOCK2, singal loss, frame rate lost und per frame-timer
        // Wird auch im ves_task gemacht, um den IRQ zurueckzusetzen
#ifndef DONT_USE_IRQ
	writereg(client, 0x0b, 0x0c); // signal input loss und frame lost
#else
	writereg(client, 0x0b, 0x0); // keine IRQs
#endif // DONT_USE_IRQ
	ves->uncp = 0;
	ves->ber=0;

	ves->pwm=readreg(client, 0x17);
        dprintk("AT76C651: pwm=%02x\n", ves->pwm);
/*
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

        // Noch mehr noetig?
        // BBFREQ (0x04, 0x05) ?
        // OUTPUTCFG (0x08) ? z.B. Pin TUNCLK

//        ves->inversion=0;
        ves->srate=0;
//        ves->reg0=Init1820PTab[0];
        // PWM setzen
        writereg(client, 0x17, ves->pwm);

        // Und ein Reset
	writereg(client, 0x07, 0x01);

        // Wir enablen mal den Tuner (staendig, zum testen) (addr c2)
        ves_tuner_i2c(1);
*/

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

static int init0(struct i2c_client *client)
{
  if(tuner_initialized)
	return init(client);
 else
   return 0;
}

/*
void SetPWM(struct i2c_client* client)
// puts pwm as pulse width modulated signal to pin FE_LOCK
{

    struct ves1820 *ves=(struct ves1820 *) client->data;
	dprintk("AT76C651: SetPWM %02x\n", ves->pwm);

        writereg(client, 0x17, ves->pwm); // Angepasst an AT... muss aber noch ueberprueft werden
	return;
}
*/

// Meine Box hat einen Quartz mit 28.9 -> fref = 2*28.9 = 57.8 (gleich dem Eval-Board zum AT)
/*
// Tabelle zum Berechnen des Exponenten zur gegebenen Symbolrate (fref=57.8 MHz)
u32 expTab[] = {
  5645, 11290, 22579, 45157,
  90313, 180625, 361250, 722500
};
*/

static int SetSymbolrate(struct i2c_client* client, u32 Symbolrate)
{
        struct at76c651 *ves=(struct at76c651 *) client->data;
#define FREF 57800000UL
//#define FREF 69600000UL
        u32 mantisse;
        u8 exp;
//	int i;
        u32 tmp;
	u32 fref;
        if(Symbolrate > FREF/2)
          Symbolrate=FREF/2;
        if(Symbolrate < 500000)
          Symbolrate=500000;
        ves->srate=Symbolrate;
	dprintk("AT76C651: SetSymbolrate %u\n", Symbolrate);
        // Ich Kernel-Dummy, Kernel nix float -> haendisch div.
        // Wenn's nur nicht so lange her waere
//  	mantisse= ((float)Symbolrate/FREF)*(1<<(30-exp));
  	tmp=Symbolrate;
        fref=FREF;
  	mantisse=tmp/fref;
  	tmp=(tmp%fref)<<8;
  	mantisse=(mantisse<<8)+tmp/fref;
  	tmp=(tmp%fref)<<8;
  	mantisse=(mantisse<<8)+tmp/fref;
  	tmp=(tmp%fref)<<8;
  	mantisse=(mantisse<<8)+tmp/fref;
/*
        for(i=6; i>2; i--)
          if(Symbolrate>=expTab[i])
            break;
        exp=i;
*/
	if(Symbolrate<722500) {
          exp=5;
//          mantisse<<=1; // unschoen, evtl. nochmal teilen
          tmp=(tmp%fref)<<8;
          mantisse=(mantisse<<1)+((tmp/fref)>>7);
 	}
 	else
          exp=6;
	dprintk("AT76C651: exp: %02x mantisse: %x <- someone should correct this\n", exp, mantisse);
	dprintk("AT76C651: Not changed, fixed to 6900000 (exp 0x06, mantisse 0x1E8f80).\n");
return 0;
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
}

typedef enum QAM_TYPE
{	QAM_16,
	QAM_32,
	QAM_64,
	QAM_128,
	QAM_256
} QAM_TYPE, *PQAM_TYPE;

static const char *qamstr[5]= {
	"QAM 16",
	"QAM 32",
	"QAM 64",
	"QAM 128",
	"QAM 256"
};

static int SetQAM(struct i2c_client* client, QAM_TYPE QAM_Mode)
{
  u8 qamsel;
  if(QAM_Mode<0 || QAM_Mode>4) {
    dprintk("AT76C651: set QAM unknow type: %d\n", (int)QAM_Mode);
    return -1;
  }
  dprintk("AT76C651: set QAM %s\n", qamstr[QAM_Mode]);

//dprintk("AT76C651: set QAM (not supported, fixed to QAM 64)\n");
//return 0;

	// Read QAMSEL
	qamsel=readreg(client, 0x03);
	qamsel &= 0xf0;
        qamsel |= QAM_Mode+4; // 4 = QAM-16
/*
        qamsel=QAM_Mode+4; // 4 = QAM-16
        // coherent modulation und Maptyp DVB
*/
        writereg(client, 0x03, qamsel);
	// Und ein reset um das Dingens richtig einzustellen
//  	writereg(client, 0x07, 0x01);
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
        0x0d, // =0x1a >> 1
        NULL,
        &dvbt_driver,
        NULL
};


#ifndef DONT_USE_IRQ

static void ves_task(void*);

struct tq_struct ves_tasklet=
{
	routine: ves_task,
	data: 0
};
#endif // DONT_USE_IRQ

static int attach_adapter(struct i2c_adapter *adap)
{
        struct ves1893 *ves;
        struct i2c_client *client;

//		unsigned long flags;

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

//        printk("feID: 1820 %x\n", readreg(&client_template, 0x1a));

        if (NULL == (client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
                return -ENOMEM;

        memcpy(client, &client_template, sizeof(struct i2c_client));
        dclient=client;

        client->data=ves=kmalloc(sizeof(struct at76c651),GFP_KERNEL);

        if (ves==NULL) {
                kfree(client);
                return -ENOMEM;
        }

        printk("AT76C651: attaching AT76C651 at 0x%02x\n", (client->addr)<<1);
        i2c_attach_client(client);

        printk("AT76C651: attached to adapter %s\n", adap->name);

#ifndef DONT_USE_IRQ
	ves_tasklet.data = (void*)client->data;
#endif // DONT_USE_IRQ

	if (register_demod(&at76c651))
 		printk("at76c651.o: can't register demod.\n");
#ifndef DONT_USE_IRQ
	if (request_8xxirq(VES_INTERRUPT, ves_interrupt, SA_ONESHOT, "at76c651", NULL) != 0)
	{
		i2c_del_driver(&dvbt_driver);
		dprintk("AT76C651: can't request interrupt\n");
	       return -EBUSY;
	}
#endif // DONT_USE_IRQ
  return 0;
}

static int detach_client(struct i2c_client *client)
{
        printk("AT76C651: detach_client\n");
        // IRQs abschalten
	writereg(client, 0x0b, 0x00);

        i2c_detach_client(client);
        kfree(client->data);
        kfree(client);
        unregister_demod(&at76c651);
        return 0;
}

static void ves_init(void)
{
  init0(dclient);
}

static void ves_set_frontend(struct frontend *front)
{
//  if (front->flags&FRONT_FREQ_CHANGED)
  dprintk("AT76C651: ves_set_frontend\n");
  SetQAM(dclient, front->qam);
  SetSymbolrate(dclient, front->srate);
  at_restart();
}

static void ves_get_frontend(struct frontend *front)
{
  at76c651_t *ves = (at76c651_t*)dclient->data;
  dprintk("AT76C651: ves_get_frontend\n");
  front->type=FRONT_DVBC;
  ves->ber=readreg(dclient, 0x83);
  ves->ber|=(readreg(dclient, 0x82)<<8);
  ves->ber|=((readreg(dclient, 0x81)&0x0f)<<16);
  front->vber=ves->ber; // Wird allerdings nur gesetzt, wenn IRQ kam, d.h. immer vom letzten signal lost
  front->nest=0;
//  front->inv= // ?
  front->sync=readreg(dclient, 0x80); // Bits: FEC, CAR, EQU, TIM, AGC2, AGC1, ADC, PLL (PLL=0)
  // AGC1 Lock fuer analog AGC ist hier immer 0, daher setzen wir das mal, wenn AGC2 (digital) 1 ist
  if(front->sync&0x08)
    front->sync|=0x04;

/* VES1820:
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

static int ves_get_unc_packet(u32 *uncp)
{
//  at76c651_t *ves = (at76c651_t*)dclient->data;
  dprintk("AT76C651: ves_get_unc_packet\n");
  *uncp=readreg(dclient, 0x85);
  return 0;
}

/* ---------------------------------------------------------------------- */

#ifndef DONT_USE_IRQ

static void ves_task(void *data)
{
        u8 ber2;
	at76c651_t *ves = (at76c651_t*)data;

        dprintk("AT76C651: task (IRQ)\n");

        // Kein Register zur Unterscheidung der IRQ-Quelle gefunden
        // Evtl. LOCK (0x80) benutzen
	ves->ber=readreg(dclient, 0x83);
        ves->ber|=(readreg(dclient, 0x82)<<8);
        ves->ber|=((readreg(dclient, 0x81)&0x0f)<<16);
        dprintk("AT76C651: ber1: 0x%x\n", ves->ber);
	ber2=readreg(dclient, 0x84);
        dprintk("AT76C651: ber2: 0x%02x\n", ber2);
	ves->uncp=readreg(dclient, 0x85);
        dprintk("AT76C651: nperr: 0x%02x\n", ves->uncp);
	ves->sync=readreg(dclient, 0x80);
        dprintk("AT76C651: sync (lock): 0x%02x\n", ves->sync);

        // IRQ Pin ruecksetzen durch schreiben in ein Register
	writereg(dclient, 0x0b, 0x0c); // signal input loss und frame lost
	enable_irq(VES_INTERRUPT);

	return;
}

static void ves_interrupt(int irq, void *vdev, struct pt_regs * regs)
{
	schedule_task(&ves_tasklet);
}
#endif // DONT_USE_IRQ

/* ---------------------------------------------------------------------- */

#ifdef MODULE
int init_module(void) {
        int res;

        dprintk("AT76C651: $Id: at76c651.c,v 1.12 2001/04/24 20:36:16 fnbrd Exp $\n");
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
	tuner_init();
	tuner_initialized=1;
	ves_init();
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

//	free_irq(VES_INTERRUPT, NULL);

        dprintk("AT76C651: cleanup\n");
	tuner_close();

}
#endif
