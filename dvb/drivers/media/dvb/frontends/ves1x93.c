/* 
    Driver for VES1893 and VES1993 QPSK Frontends

    Copyright (C) 1999 Convergence Integrated Media GmbH <ralph@convergence.de>
    Copyright (C) 2001 Ronny Strutz <3des@tuxbox.org>
    Copyright (C) 2002 Andreas Oberritter <obi@tuxbox.org>

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

#include "compat.h"
#include "dvb_frontend.h"
#include "dvb_i2c.h"
#ifdef DBOX2
#include <dbox/fp.h>
#endif


static int debug = 0;
#define dprintk	if (debug) printk


static int board_type = 0;
#define SIEMENS_PCI_1893	0x01
#define NOKIA_DBOX2_1893	0x02
#define	NOKIA_DBOX2_1993	0x03
#define	SAGEM_DBOX2_1993	0x04


static
struct dvb_frontend_info ves1x93_info = {
	name: "VES1893/VES1993",
	type: FE_QPSK,
	frequency_min: 950000,
	frequency_max: 2150000,
	frequency_stepsize: 250,           /* kHz for QPSK frontends */
	frequency_tolerance: 29500,
	symbol_rate_min: 1000000,
	symbol_rate_max: 45000000,
/*      symbol_rate_tolerance: ???,*/
	notifier_delay: 50,                /* 1/20 s */
	caps:   FE_CAN_INVERSION_AUTO |
		FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
		FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
		FE_CAN_QPSK
};




static
u8 init_1893_tab_siemens_pci [] = {
	0x01, 0xa4, 0x35, 0x81, 0x2a, 0x0d, 0x55, 0xc4,
	0x09, 0x69, 0x00, 0x86, 0x4c, 0x28, 0x7f, 0x00,
	0x00, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x80, 0x00, 0x31, 0xb0, 0x14, 0x00, 0xdc, 0x20,
	0x81, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x55, 0x00, 0x00, 0x7f, 0x00
};


static
u8 init_1893_tab_nokia_dbox2 [] = {
        0x01, 0x9c, 0x00, 0x80, 0x6a, 0x2c, 0x9b, 0xab,
        0x09, 0x6a, 0x00, 0x86, 0x4c, 0x08, 0x7f, 0x00,
        0x00, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x80, 0x00, 0x21, 0xb0, 0x14, 0x00, 0xdc, 0x00,
        0x81, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x55, 0x03, 0x00, 0x7f, 0x00
};


static
u8 init_1993_tab_nokia_dbox2 [] = {
	0x00, 0x94, 0x00, 0x80, 0x6a, 0x0b, 0xab, 0x2a,
	0x09, 0x70, 0x00, 0x00, 0x4c, 0x02, 0x00, 0x00,
	0x00, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x80, 0x40, 0x21, 0xb0, 0x00, 0x00, 0x00, 0x00,
	0x81, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x55, 0x03, 0x00, 0x00, 0x00, 0x01, 0x03,
	0x00, 0x00, 0x0e, 0xfd, 0x56
};


static
u8 init_1993_tab_sagem_dbox2 [] = {
	0x00, 0x9c, 0x35, 0x80, 0x6a, 0x29, 0x72, 0x8c,
	0x09, 0x6b, 0x00, 0x00, 0x4c, 0x08, 0x00, 0x00,
	0x00, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x80, 0x40, 0x21, 0xb0, 0x00, 0x00, 0x00, 0x10,
	0x81, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x55, 0x03, 0x00, 0x00, 0x00, 0x00, 0x03,
	0x00, 0x00, 0x0e, 0x80, 0x00
};


static
u8 * init_1x93_tab;


static
u8 init_1893_wtab[] =
{
        1,1,1,1,1,1,1,1, 1,1,0,0,1,1,0,0,
        0,1,0,0,0,0,0,0, 1,0,1,1,0,0,0,1,
        1,1,1,0,0,0,0,0, 0,0,1,1,0,0,0,0,
        1,1,1,0,1,1
};


static
u8 init_1993_wtab[] =
{
	0,1,1,1,1,1,1,1, 1,1,0,0,1,1,0,0,
	0,1,0,0,0,0,0,0, 1,1,1,1,0,0,0,1,
	1,1,1,0,0,0,0,0, 0,0,1,1,0,0,0,0,
	1,1,1,0,1,1,1,1, 1,1,1,1,1
};


static
int ves1x93_writereg (struct dvb_i2c_bus *i2c, int reg, int data)
{
        u8 buf [] = { 0x00, reg, data };
	struct i2c_msg msg = { addr: 0x08, flags: 0, buf: buf, len: 3 };
	int err;

        if ((err = i2c->xfer (i2c, &msg, 1)) != 1) {
		dprintk ("%s: writereg error (err == %i, reg == 0x%02x, data == 0x%02x)\n", __FUNCTION__, err, reg, data);
		return -EREMOTEIO;
	}

        return 0;
}


static
u8 ves1x93_readreg (struct dvb_i2c_bus *i2c, u8 reg)
{
	int ret;
	u8 b0 [] = { 0x00, reg };
	u8 b1 [] = { 0 };
	struct i2c_msg msg [] = { { addr: 0x08, flags: 0, buf: b0, len: 2 },
			   { addr: 0x08, flags: I2C_M_RD, buf: b1, len: 1 } };

	ret = i2c->xfer (i2c, msg, 2);

	if (ret != 2)
		dprintk("%s: readreg error (ret == %i)\n", __FUNCTION__, ret);

	return b1[0];
}


static
int tuner_write (struct dvb_i2c_bus *i2c, u8 *data, u8 len)
{
        int ret;
        struct i2c_msg msg = { addr: 0x61, flags: 0, buf: data, len: len };

        ret = i2c->xfer (i2c, &msg, 1);

        if (ret != 1)
                printk("%s: i/o error (ret == %i)\n", __FUNCTION__, ret);

        return (ret != 1) ? -1 : 0;
}



/**
 *   set up the downconverter frequency divisor for a
 *   reference clock comparision frequency of 125 kHz.
 */
static
int sp5659_set_tv_freq (struct dvb_i2c_bus *i2c, u32 freq, u8 pwr)
{
        u32 div = (freq + 479500) / 125;
        u8 buf [4] = { (div >> 8) & 0x7f, div & 0xff, 0x95, (pwr << 5) | 0x30 };

#ifdef DBOX2
	if (board_type == NOKIA_DBOX2_1893)
		return dbox2_fp_tuner_write(buf, sizeof(buf));
#endif
	return tuner_write (i2c, buf, sizeof(buf));
}


static
int gnarf_set_tv_freq (struct dvb_i2c_bus *i2c, u32 freq)
{
	int ret;
	u8 buf [2];

	freq /= 1000;

	buf[0] = (freq >> 8) & 0x7F;
	buf[1] = freq & 0xFF;

	ves1x93_writereg(i2c, 0x00, 0x11);
	ret = tuner_write(i2c, buf, sizeof(buf));
	ves1x93_writereg(i2c, 0x00, 0x01);

	return ret;
}


static
int tuner_set_tv_freq (struct dvb_i2c_bus *i2c, u32 freq, u8 pwr)
{
	switch (board_type) {
	case SIEMENS_PCI_1893:
	case NOKIA_DBOX2_1893:
		return sp5659_set_tv_freq (i2c, freq, pwr);
	case NOKIA_DBOX2_1993:
	case SAGEM_DBOX2_1993:
		return gnarf_set_tv_freq (i2c, freq);
	default:
		return -EINVAL;
	}
}


static
int ves1x93_init (struct dvb_i2c_bus *i2c)
{
	int i;
	int size;
	u8 *init_1x93_wtab;
        
	dprintk("%s: init chip\n", __FUNCTION__);

	switch (board_type) {
	case SIEMENS_PCI_1893:
		init_1x93_tab = init_1893_tab_siemens_pci;
		init_1x93_wtab = init_1893_wtab;
		size = sizeof(init_1893_tab_siemens_pci);
		break;
	case NOKIA_DBOX2_1893:
		init_1x93_tab = init_1893_tab_nokia_dbox2;
		init_1x93_wtab = init_1893_wtab;
		size = sizeof(init_1893_tab_nokia_dbox2);
		break;
	case NOKIA_DBOX2_1993:
		init_1x93_tab = init_1993_tab_nokia_dbox2;
		init_1x93_wtab = init_1993_wtab;
		size = sizeof(init_1993_tab_nokia_dbox2);
		break;
	case SAGEM_DBOX2_1993:
		init_1x93_tab = init_1993_tab_sagem_dbox2;
		init_1x93_wtab = init_1993_wtab;
		size = sizeof(init_1993_tab_sagem_dbox2);
		break;
	default:
		return -EINVAL;
	}
	
	for (i=0; i<size; i++)
		if (init_1x93_wtab[i])
			ves1x93_writereg (i2c, i, init_1x93_tab[i]);

	return 0;
}


static
int ves1x93_clr_bit (struct dvb_i2c_bus *i2c)
{
        ves1x93_writereg (i2c, 0, init_1x93_tab[0] & 0xfe);
        ves1x93_writereg (i2c, 0, init_1x93_tab[0]);
        ves1x93_writereg (i2c, 3, 0x00);
        return ves1x93_writereg (i2c, 3, init_1x93_tab[3]);
}


static
int ves1x93_set_inversion (struct dvb_i2c_bus *i2c, fe_spectral_inversion_t inversion)
{
	u8 val;

	switch (inversion) {
	case INVERSION_OFF:
		val = 0xc0;
		break;
	case INVERSION_ON:
		val = 0x80;
		break;
	case INVERSION_AUTO:
		val = 0x40;
		break;
	default:
		return -EINVAL;
	}

	return ves1x93_writereg (i2c, 0x0c, (init_1x93_tab[0x0c] & 0x3f) | val);
}


static
int ves1x93_set_fec (struct dvb_i2c_bus *i2c, fe_code_rate_t fec)
{
	if (fec == FEC_AUTO)
		return ves1x93_writereg (i2c, 0x0d, 0x08);
	else if (fec < FEC_1_2 || fec > FEC_8_9)
		return -EINVAL;
	else
		return ves1x93_writereg (i2c, 0x0d, fec - FEC_1_2);
}


static
fe_code_rate_t ves1x93_get_fec (struct dvb_i2c_bus *i2c)
{
	return FEC_1_2 + ((ves1x93_readreg (i2c, 0x0d) >> 4) & 0x7);
}


static
int ves1x93_set_symbolrate (struct dvb_i2c_bus *i2c, u32 srate)
{
	u32 BDR;
        u32 ratio;
	u8  ADCONF, FCONF, FNR;
	u32 BDRI;
	u32 tmp;
	u32 XIN, FIN;

	dprintk("%s: srate == %d\n", __FUNCTION__, srate);

	switch (board_type) {
	case SIEMENS_PCI_1893:
		XIN = 90100000UL;
		break;
	case NOKIA_DBOX2_1893:
		XIN = 91000000UL;
		break;
	case NOKIA_DBOX2_1993:
		XIN = 96000000UL;
		break;
	case SAGEM_DBOX2_1993:
		XIN = 92160000UL;
		break;
	default:
		return -EINVAL;
	}

	if (srate > XIN/2)
		srate = XIN/2;

	if (srate < 500000)
		srate = 500000;

#define MUL (1UL<<26)

	FIN = XIN >> 4;

	tmp = srate << 6;
	ratio = tmp / FIN;

	tmp = (tmp % FIN) << 8;
	ratio = (ratio << 8) + tmp / FIN;

	tmp = (tmp % FIN) << 8;
	ratio = (ratio << 8) + tmp / FIN;

	FNR = 0xff;

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

	if (FNR == 0xff) {
		ADCONF = 0x89;
		FCONF  = 0x80;
		FNR	= 0;
	} else {
		ADCONF = 0x81;
		FCONF  = 0x88 | (FNR >> 1) | ((FNR & 0x01) << 5);
	}

	BDR = (( (ratio << (FNR >> 1)) >> 4) + 1) >> 1;
	BDRI = ( ((FIN << 8) / ((srate << (FNR >> 1)) >> 2)) + 1) >> 1;

        dprintk("FNR= %d\n", FNR);
        dprintk("ratio= %08x\n", ratio);
        dprintk("BDR= %08x\n", BDR);
        dprintk("BDRI= %02x\n", BDRI);

	if (BDRI > 0xff)
		BDRI = 0xff;

	ves1x93_writereg (i2c, 0x06, 0xff & BDR);
	ves1x93_writereg (i2c, 0x07, 0xff & (BDR >> 8));
	ves1x93_writereg (i2c, 0x08, 0x0f & (BDR >> 16));

	ves1x93_writereg (i2c, 0x09, BDRI);
	ves1x93_writereg (i2c, 0x20, ADCONF);
	ves1x93_writereg (i2c, 0x21, FCONF);

	if (srate < 6000000) 
		ves1x93_writereg (i2c, 0x05, init_1x93_tab[0x05] | 0x80);
	else
		ves1x93_writereg (i2c, 0x05, init_1x93_tab[0x05] & 0x7f);

	ves1x93_writereg (i2c, 0x00, 0x00);
	ves1x93_writereg (i2c, 0x00, 0x01);

	ves1x93_clr_bit (i2c);

	return 0;
}


static
int ves1x93_set_voltage (struct dvb_i2c_bus *i2c, fe_sec_voltage_t voltage)
{
	switch (board_type) {
	case SIEMENS_PCI_1893:
		return ves1x93_writereg (i2c, 0x1f, (voltage == SEC_VOLTAGE_13) ? 0x20 : 0x30);
#ifdef DBOX2
	case NOKIA_DBOX2_1893:
	case NOKIA_DBOX2_1993:
	case SAGEM_DBOX2_1993:
		return dbox2_fp_sec_set_voltage((voltage == SEC_VOLTAGE_13) ? 0x00 : 0x01);
#endif
	default:
		return -EINVAL;
	}
}


static
int ves1x93_set_tone (struct dvb_i2c_bus *i2c, fe_sec_tone_mode_t tone)
{
	switch (board_type) {
	case SIEMENS_PCI_1893:
		return -EOPNOTSUPP;
#ifdef DBOX2
	case NOKIA_DBOX2_1893:
		return dbox2_fp_sec_set_tone((tone == SEC_TONE_OFF) ? 0x00 : 0x01);
#endif
	case NOKIA_DBOX2_1993:
	case SAGEM_DBOX2_1993:
		return ves1x93_writereg(i2c, 0x36, (tone == SEC_TONE_OFF) ? 0x00 : 0x01);
	default:
		return -EINVAL;
	}
}


static
int ves1x93_ioctl (struct dvb_frontend *fe, unsigned int cmd, void *arg)
{
	struct dvb_i2c_bus *i2c = (struct dvb_i2c_bus *) fe->data;
 
        switch (cmd) {
        case FE_GET_INFO:
		memcpy (arg, &ves1x93_info, sizeof(struct dvb_frontend_info));
		break;

        case FE_READ_STATUS:
	{
		fe_status_t *status = arg;
		int sync = ves1x93_readreg (i2c, 0x0e);

		*status = 0;

		if (sync & 1)
			*status |= FE_HAS_SIGNAL;

		if (sync & 2)
			*status |= FE_HAS_CARRIER;

		if (sync & 4)
			*status |= FE_HAS_VITERBI;

		if (sync & 8)
			*status |= FE_HAS_SYNC;

		if ((sync & 0x1f) == 0x1f)
			*status |= FE_HAS_LOCK;

		break;
	}

        case FE_READ_BER:
	{
		u32 *ber = (u32 *) arg;

		*ber = ves1x93_readreg (i2c, 0x15);
                *ber |= (ves1x93_readreg (i2c, 0x16) << 8);
                *ber |= ((ves1x93_readreg (i2c, 0x17) & 0x0F) << 16);
		*ber *= 10;
		break;
	}

        case FE_READ_SIGNAL_STRENGTH:
	{
		u8 signal = ~ves1x93_readreg (i2c, 0x0b);
		*((u16*) arg) = (signal << 8) | signal;
		break;
	}

        case FE_READ_SNR:
	{
		u8 snr = ~ves1x93_readreg (i2c, 0x1c);
		*(u16*) arg = (snr << 8) | snr;
		break;
	}

	case FE_READ_UNCORRECTED_BLOCKS: 
	{
		*(u32*) arg = ves1x93_readreg (i2c, 0x18) & 0x7f;

		if (*(u32*) arg == 0x7f)
			*(u32*) arg = 0xffffffff;   /* counter overflow... */
		
		ves1x93_writereg (i2c, 0x18, 0x00);  /* reset the counter */
		ves1x93_writereg (i2c, 0x18, 0x80);  /* dto. */
		break;
	}

        case FE_SET_FRONTEND:
        {
		struct dvb_frontend_parameters *p = arg;

		tuner_set_tv_freq (i2c, p->frequency, 0);
		ves1x93_set_inversion (i2c, p->inversion);
		ves1x93_set_fec (i2c, p->u.qpsk.fec_inner);
		ves1x93_set_symbolrate (i2c, p->u.qpsk.symbol_rate);
                break;
        }

	case FE_GET_FRONTEND:
	{
		struct dvb_frontend_parameters *p = arg;
		s32 afc;

		afc = ((int)((char)(ves1x93_readreg (i2c, 0x0a) << 1)))/2;
		afc = (afc * (int)(p->u.qpsk.symbol_rate/8))/16;

		p->frequency += afc;
		p->inversion = (ves1x93_readreg (i2c, 0x0f) & 2) ? 
					INVERSION_ON : INVERSION_OFF;
		p->u.qpsk.fec_inner = ves1x93_get_fec (i2c);
	/*  XXX FIXME: timing offset !! */
		break;
	}

        case FE_SLEEP:
		if (board_type == SIEMENS_PCI_1893)
			ves1x93_writereg (i2c, 0x1f, 0x00);    /*  LNB power off  */
		return ves1x93_writereg (i2c, 0x00, 0x08);

        case FE_INIT:
		return ves1x93_init (i2c);

	case FE_RESET:
		return ves1x93_clr_bit (i2c);

	case FE_SET_TONE:
		return ves1x93_set_tone (i2c, (fe_sec_tone_mode_t) arg);

	case FE_SET_VOLTAGE:
		return ves1x93_set_voltage (i2c, (fe_sec_voltage_t) arg);

	default:
		return -EOPNOTSUPP;
        };
        
        return 0;
} 


static
int ves1x93_attach (struct dvb_i2c_bus *i2c)
{
	if ((ves1x93_readreg (i2c, 0x1e) & 0xf0) != 0xd0)
		return -ENODEV;

	dvb_register_frontend (ves1x93_ioctl, i2c->adapter, NULL, &ves1x93_info);

	return 0;
}


static
void ves1x93_detach (struct dvb_i2c_bus *i2c)
{
	dvb_unregister_frontend (ves1x93_ioctl, i2c->adapter);
}


static
int __init init_ves1x93 (void)
{
	return dvb_register_i2c_device (THIS_MODULE, ves1x93_attach, ves1x93_detach);
}


static 
void __exit exit_ves1x93 (void)
{
	dvb_unregister_i2c_device (ves1x93_attach);
}


module_init(init_ves1x93);
module_exit(exit_ves1x93);


MODULE_DESCRIPTION("VES1x93 DVB-S Frontend");
MODULE_AUTHOR("Ralph Metzler");
MODULE_LICENSE("GPL");
MODULE_PARM(debug,"i");
MODULE_PARM(board_type,"i");

