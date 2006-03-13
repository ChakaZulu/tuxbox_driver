/*
 * at76c651.c
 *
 * Atmel DVB-C Frontend Driver (at76c651/tua6010xs)
 *
 * Copyright (C) 2001 fnbrd <fnbrd@gmx.de>
 *             & 2002-2004 Andreas Oberritter <obi@linuxtv.org>
 *             & 2003 Wolfram Joost <dbox2@frokaschwei.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * AT76C651
 * http://www.nalanda.nitc.ac.in/industry/datasheets/atmel/acrobat/doc1293.pdf
 * http://www.atmel.com/atmel/acrobat/doc1320.pdf
 *
 * TUA6010XS
 * http://www.infineon.com/cgi/ecrm.dll/ecrm/scripts/public_download.jsp?oid=19512
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>

#if defined(__powerpc__)
#include <asm/bitops.h>
#endif

#include "dvb_frontend.h"
#include "dvb_i2c.h"
#include "dvb_functions.h"

static int debug = 0;
static u8 at76c651_qam;
static u8 at76c651_revision;

#define dprintk	if (debug) printk

// #define AT76C651_PROC_INTERFACE

#ifdef AT76C651_PROC_INTERFACE
#include <linux/proc_fs.h>
static unsigned char at76c651_proc_registered = 0;
#endif

static struct dvb_frontend_info at76c651_info = {
	.name = "Atmel AT76C651B with TUA6010XS",
	.type = FE_QAM,
	.frequency_min = 48250000,
	.frequency_max = 863250000,
	.frequency_stepsize = 62500,
	/*.frequency_tolerance = */	/* FIXME: 12% of SR */
	.symbol_rate_min = 0,		/* FIXME */
	.symbol_rate_max = 9360000,	/* FIXME */
	.symbol_rate_tolerance = 4000,
	.notifier_delay = 0,
	.caps = FE_CAN_INVERSION_AUTO |
	    FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
	    FE_CAN_FEC_4_5 | FE_CAN_FEC_5_6 | FE_CAN_FEC_6_7 |
	    FE_CAN_FEC_7_8 | FE_CAN_FEC_8_9 | FE_CAN_FEC_AUTO |
	    FE_CAN_QAM_16 | FE_CAN_QAM_32 | FE_CAN_QAM_64 | FE_CAN_QAM_128 |
	    FE_CAN_QAM_256 /* | FE_CAN_QAM_512 | FE_CAN_QAM_1024 */ |
	    FE_CAN_RECOVER | FE_CAN_CLEAN_SETUP | FE_CAN_MUTE_TS
};

#if ! defined(__powerpc__)
static __inline__ int __ilog2(unsigned long x)
{
	int i;

	if (x == 0)
		return -1;

	for (i = 0; x != 0; i++)
		x >>= 1;

	return i - 1;
}
#endif

static int at76c651_writereg(struct dvb_i2c_bus *i2c, u8 reg, u8 data)
{
	int ret;
	u8 buf[] = { reg, data };
	struct i2c_msg msg =
		{ .addr = 0x1a >> 1, .flags = 0, .buf = buf, .len = 2 };

	ret = i2c->xfer(i2c, &msg, 1);

	if (ret != 1)
		dprintk("%s: writereg error "
			"(reg == 0x%02x, val == 0x%02x, ret == %i)\n",
			__FUNCTION__, reg, data, ret);

	dvb_delay(10);

	return (ret != 1) ? -EREMOTEIO : 0;
}

static u8 at76c651_readreg(struct dvb_i2c_bus *i2c, u8 reg)
{
	int ret;
	u8 val;
	struct i2c_msg msg[] = {
		{ .addr = 0x1a >> 1, .flags = 0, .buf = &reg, .len = 1 },
		{ .addr = 0x1a >> 1, .flags = I2C_M_RD, .buf = &val, .len = 1 }
	};

	ret = i2c->xfer(i2c, msg, 2);

	if (ret != 2)
		dprintk("%s: readreg error (ret == %i)\n", __FUNCTION__, ret);

	return val;
}

static int at76c651_reset(struct dvb_i2c_bus *i2c)
{
	return at76c651_writereg(i2c, 0x07, 0x01);
}

static void at76c651_disable_interrupts(struct dvb_i2c_bus *i2c)
{
	at76c651_writereg(i2c, 0x0b, 0x00);
}

static int at76c651_set_auto_config(struct dvb_i2c_bus *i2c)
{
	/*
	 * Autoconfig
	 */
	at76c651_writereg(i2c, 0x06, 0x01);

	/*
	 * Performance optimizations, should be done after autoconfig
	 */
	at76c651_writereg(i2c, 0x10, 0x06);
	at76c651_writereg(i2c, 0x11, ((at76c651_qam == 5) || (at76c651_qam == 7)) ? 0x12 : 0x10);
	at76c651_writereg(i2c, 0x15, 0x28);
	at76c651_writereg(i2c, 0x20, 0x09);
	at76c651_writereg(i2c, 0x24, ((at76c651_qam == 5) || (at76c651_qam == 7)) ? 0xC0 : 0x90);
	at76c651_writereg(i2c, 0x30, 0x90);
	if (at76c651_qam == 5)
		at76c651_writereg(i2c, 0x35, 0x2A);

	/*
	 * Initialize A/D-converter
	 */
	if (at76c651_revision == 0x11) {
		at76c651_writereg(i2c, 0x2E, 0x38);
		at76c651_writereg(i2c, 0x2F, 0x13);
	}

	at76c651_disable_interrupts(i2c);

	/*
	 * Restart operation
	 */
	at76c651_reset(i2c);

	return 0;
}

static void at76c651_set_bbfreq(struct dvb_i2c_bus *i2c)
{
	at76c651_writereg(i2c, 0x04, 0x3f);
	at76c651_writereg(i2c, 0x05, 0xee);
}

static int at76c651_pll_write(struct dvb_i2c_bus *i2c, u8 *buf, size_t len)
{
	int ret;
	struct i2c_msg msg =
		{ .addr = 0xc2 >> 1, .flags = 0, .buf = buf, .len = len };

	at76c651_writereg(i2c, 0x0c, 0xc3);

	ret = i2c->xfer(i2c, &msg, 1);

	at76c651_writereg(i2c, 0x0c, 0xc2);

	return (ret != 1) ? -EREMOTEIO : 0;
}

static int tua6010_setfreq(struct dvb_i2c_bus *i2c, u32 freq)
{
	u32 div;
	u8 buf[4];
	u8 vu, p2, p1, p0;

	/* 47 MHz ... 862 MHz */
	if ((freq < 47000000) || (freq > 862000000))
		return -EINVAL;

	div = (freq + 36118750 + 31250) / 62500;

	if (freq > 401250000)
		vu = 1;	/* UHF */
	else
		vu = 0; /* VHF */

	if (freq > 401250000)
		p2 = 1, p1 = 0, p0 = 1;
	else if (freq > 117250000)
		p2 = 1, p1 = 1, p0 = 0;
	else
		p2 = 0, p1 = 1, p0 = 1;

	buf[0] = (div >> 8) & 0x7f;
	buf[1] = (div >> 0) & 0xff;
	buf[2] = 0x8e;
	buf[3] = (vu << 7) | (p2 << 2) | (p1 << 1) | p0;

	return at76c651_pll_write(i2c, buf, 4);
}

static int at76c651_set_symbol_rate(struct dvb_i2c_bus *i2c, u32 symbol_rate)
{
	u8 exponent;
	u32 mantissa;

	if (symbol_rate > 9360000)
		return -EINVAL;

	/*
	 * FREF = 57800 kHz
	 * exponent = 10 + floor (log2(symbol_rate / FREF))
	 * mantissa = (symbol_rate / FREF) * (1 << (30 - exponent))
	 */

	exponent = __ilog2((symbol_rate << 4) / 903125);
	mantissa = ((symbol_rate / 3125) * (1 << (24 - exponent))) / 289;

	at76c651_writereg(i2c, 0x00, mantissa >> 13);
	at76c651_writereg(i2c, 0x01, mantissa >> 5);
	at76c651_writereg(i2c, 0x02, (mantissa << 3) | exponent);

	return 0;
}

static int at76c651_set_qam(struct dvb_i2c_bus *i2c, fe_modulation_t qam)
{
	switch (qam) {
	case QPSK:
		at76c651_qam = 0x02;
		break;
	case QAM_16:
		at76c651_qam = 0x04;
		break;
	case QAM_32:
		at76c651_qam = 0x05;
		break;
	case QAM_64:
		at76c651_qam = 0x06;
		break;
	case QAM_128:
		at76c651_qam = 0x07;
		break;
	case QAM_256:
		at76c651_qam = 0x08;
		break;
#if 0
	case QAM_512:
		at76c651_qam = 0x09;
		break;
	case QAM_1024:
		at76c651_qam = 0x0A;
		break;
#endif
	default:
		return -EINVAL;
	}

	return at76c651_writereg(i2c, 0x03, at76c651_qam);
}

static int at76c651_set_inversion(struct dvb_i2c_bus *i2c,
		       fe_spectral_inversion_t inversion)
{
	u8 feciqinv = at76c651_readreg(i2c, 0x60);

	switch (inversion) {
	case INVERSION_OFF:
		feciqinv |= 0x02;
		feciqinv &= 0xFE;
		break;

	case INVERSION_ON:
		feciqinv |= 0x03;
		break;

	case INVERSION_AUTO:
		feciqinv &= 0xFC;
		break;

	default:
		return -EINVAL;
	}

	return at76c651_writereg(i2c, 0x60, feciqinv);
}

static int at76c651_set_parameters(struct dvb_i2c_bus *i2c,
			struct dvb_frontend_parameters *p)
{
	int ret;

	if ((ret = tua6010_setfreq(i2c, p->frequency)))
		return ret;

	if ((ret = at76c651_set_symbol_rate(i2c, p->u.qam.symbol_rate)))
		return ret;

	if ((ret = at76c651_set_qam(i2c, p->u.qam.modulation)))
		return ret;

	if ((ret = at76c651_set_inversion(i2c, p->inversion)))
		return ret;

	return at76c651_set_auto_config(i2c);
}

static int at76c651_set_defaults(struct dvb_i2c_bus *i2c)
{
	at76c651_set_symbol_rate(i2c, 6900000);
	at76c651_set_qam(i2c, QAM_64);
	at76c651_set_bbfreq(i2c);
	at76c651_set_auto_config(i2c);

	return 0;
}

static int at76c651_ioctl(struct dvb_frontend *fe, unsigned int cmd, void *arg)
{

	switch (cmd) {
	case FE_GET_INFO:
		memcpy(arg, &at76c651_info, sizeof(struct dvb_frontend_info));
		break;

	case FE_READ_STATUS:
	{
		fe_status_t *status = arg;
		u8 sync;

		/*
		 * Bits: FEC, CAR, EQU, TIM, AGC2, AGC1, ADC, PLL (PLL=0) 
		 */
		sync = at76c651_readreg(fe->i2c, 0x80);

		*status = 0;

		if (sync & (0x04 | 0x10))	/* AGC1 || TIM */
			*status |= FE_HAS_SIGNAL;

		if (sync & 0x10)	/* TIM */
			*status |= FE_HAS_CARRIER;

		if (sync & 0x80)	/* FEC */
			*status |= FE_HAS_VITERBI;

		if (sync & 0x40)	/* CAR */
			*status |= FE_HAS_SYNC;

		if ((sync & 0xF0) == 0xF0)	/* TIM && EQU && CAR && FEC */
			*status |= FE_HAS_LOCK;

		break;
	}

	case FE_READ_BER:
	{
		u32 *ber = arg;
		*ber = (at76c651_readreg(fe->i2c, 0x81) & 0x0F) << 16;
		*ber |= at76c651_readreg(fe->i2c, 0x82) << 8;
		*ber |= at76c651_readreg(fe->i2c, 0x83);
		*ber *= 10;
		break;
	}

	case FE_READ_SIGNAL_STRENGTH:
	{
		u8 gain = ~at76c651_readreg(fe->i2c, 0x91);
		*(u16 *)arg = (gain << 8) | gain;
		break;
	}

	case FE_READ_SNR:
		*(u16 *)arg = 0xFFFF -
		    ((at76c651_readreg(fe->i2c, 0x8F) << 8) |
		     at76c651_readreg(fe->i2c, 0x90));
		break;

	case FE_READ_UNCORRECTED_BLOCKS:
		*(u32 *)arg = at76c651_readreg(fe->i2c, 0x82);
		break;

	case FE_SET_FRONTEND:
		return at76c651_set_parameters(fe->i2c, arg);

	case FE_GET_FRONTEND:
		break;

	case FE_SLEEP:
		break;

	case FE_INIT:
		return at76c651_set_defaults(fe->i2c);

	case FE_RESET:
		return at76c651_reset(fe->i2c);

	default:
		return -ENOIOCTLCMD;
	}

	return 0;
}

#ifdef AT76C651_PROC_INTERFACE
static int at76c651_proc_read(char *buf, char **start, off_t offset, int len, int *eof, void *i2c)
{
	int nr;
	u8 val;
	int idx;
	u8 bit;
	u32 ber;

	const char *lockdescr[8] =
		{ "FEC", "CAR", "EQU", "TIM", "AGC2", "AGC1", "ADC", "PLL" };

	if (at76c651_revision == 0x10)
		nr = sprintf(buf, "Status of AT76C651A demodulator:\n");
	else
		nr = sprintf(buf, "Status of AT76C651B demodulator:\n");

	val = at76c651_readreg(i2c, 0x80);
	nr += sprintf(buf, "Lock (0x%02X): ", val);

	bit = 0x80;

	for (idx = 0; idx < 8; idx++) {
		if (val & bit)
			nr += sprintf(buf + nr, "%s ", lockdescr[idx]);
		bit >>= 1;
	}

	ber = ((at76c651_readreg(i2c, 0x81) & 0x0F) << 16) |
		(at76c651_readreg(i2c, 0x82) << 8) |
		at76c651_readreg(i2c, 0x83);

	val = at76c651_readreg(i2c, 0x85);

	nr += sprintf(buf + nr - 1, "\nRecoverable block error rate: %d\n"
			"Number of uncorrectable frames: %d\n", ber, val) - 1;

	nr += sprintf(buf + nr, "Number of A/D-converter-saturations: %d\n"
			"AGC1-Value: %d\n",
			at76c651_readreg(i2c, 0x19),
			at76c651_readreg(i2c, 0x91));

	if ((val = at76c651_readreg(i2c, 0x60)) & 0x02)
		nr += sprintf(buf + nr, "FEC: manual mode, %s\n",
				(val & 0x01) ? "inverse" : "normal");
	else
		nr += sprintf(buf + nr, "FEC: automatic mode, %s\n",
				(val & 0x04) ? "inverse" : "normal");

	return nr;
}

static int at76c651_register_proc(struct dvb_i2c_bus *i2c)
{
	struct proc_dir_entry *proc_bus_at76c651;

	if (!proc_bus)
		return -ENOENT;

	proc_bus_at76c651 = create_proc_read_entry("at76c651", 0, proc_bus,
			&at76c651_proc_read,i2c);

	if (!proc_bus_at76c651) {
		printk(KERN_ERR "Cannot create /proc/bus/at76c651\n");
		return -ENOENT;
	}

	at76c651_proc_registered = 1;

	proc_bus_at76c651->owner = THIS_MODULE;

	return 0;
}

static void at76c651_deregister_proc(void)
{
	if (at76c651_proc_registered)
		remove_proc_entry("at76c651",proc_bus);

	at76c651_proc_registered = 0;
}
#endif

static int at76c651_attach(struct dvb_i2c_bus *i2c, void **data)
{
	if (at76c651_readreg(i2c, 0x0E) != 0x65)
		return -ENODEV;

	at76c651_revision = at76c651_readreg(i2c, 0x0F);

	switch (at76c651_revision) {
	case 0x10:
		at76c651_info.name[14] = 'A';
		break;
	case 0x11:
		at76c651_info.name[14] = 'B';
		break;
	default:
		return -ENODEV;
	}

	at76c651_set_defaults(i2c);

#ifdef AT76C651_PROC_INTERFACE
	at76c651_register_proc(i2c);
#endif

	return dvb_register_frontend(at76c651_ioctl, i2c, NULL, &at76c651_info);
}

static void at76c651_detach(struct dvb_i2c_bus *i2c, void *data)
{
#ifdef AT76C651_PROC_INTERFACE
	at76c651_deregister_proc();
#endif

	dvb_unregister_frontend(at76c651_ioctl, i2c);
}

static int __init at76c651_init(void)
{
	return dvb_register_i2c_device(THIS_MODULE,
			at76c651_attach, at76c651_detach);
}

static void __exit at76c651_exit(void)
{
	dvb_unregister_i2c_device(at76c651_attach);
}

module_init(at76c651_init);
module_exit(at76c651_exit);

MODULE_DESCRIPTION("at76c651/dat7021(tua6010xs) dvb-c frontend driver");
MODULE_AUTHOR("Andreas Oberritter <obi@linuxtv.org>");
MODULE_LICENSE("GPL");
MODULE_PARM(debug, "i");
