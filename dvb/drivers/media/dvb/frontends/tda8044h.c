/* 
 * $Id: tda8044h.c,v 1.15 2002/10/22 01:52:25 obi Exp $
 *   
 * tda8044h.c - Philips TDA8044H (d-box 2 project) 
 *
 * Copyright (C) 2001 Felix Domke <tmbinc@gmx.net>
 * Copyright (C) 2002 Andreas Oberritter <obi@tuxbox.org>
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
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>

#include "compat.h"
#include "dvb_frontend.h"


static int debug = 0;
#define dprintk	if (debug) printk


static
struct dvb_frontend_info tda8044_info = {
	name: "TDA8044H QPSK Demodulator",
	type: FE_QPSK,
	frequency_min: 500,
	frequency_max: 2700000,
	frequency_stepsize: 125,
	/*frequency_tolerance: ???,*/
	symbol_rate_min: 4500000,
	symbol_rate_max: 45000000,
	/*symbol_rate_tolerance: ???,*/	/* ppm */
	notifier_delay: 0,
	caps:	FE_CAN_INVERSION_AUTO |
		FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
		FE_CAN_FEC_4_5 | FE_CAN_FEC_5_6 | FE_CAN_FEC_5_6 |
		FE_CAN_FEC_7_8 | FE_CAN_FEC_8_9 | FE_CAN_FEC_AUTO |
		FE_CAN_QPSK |
		FE_CAN_MUTE_TS
};


static u8 fbcn = 0xF;


static
u8 tda8044_inittab [] =
{
	0x06, 0x00, 0x6f, 0xb5, 0x86, 0x22, 0x00, 0xea,
	0x30, 0x42, 0x98, 0x68, 0x70, 0x42, 0x99, 0x58,
	0x95, 0x10, 0xf5, 0xe7, 0x93, 0x0b, 0x15, 0x68,
	0x9a, 0x90, 0x61, 0x80, 0x00, 0xe0, 0x40, 0x00,
	0x0f, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00
};


static
int tda8044_writereg (struct dvb_i2c_bus *i2c, u8 reg, u8 data)
{
	int ret;
	u8 buf[] = { reg, data };
	struct i2c_msg msg = { addr: 0x68, flags: 0, buf: buf, len: 2 };

	ret = i2c->xfer (i2c, &msg, 1);

	if (ret != 1)
		dprintk("%s: writereg error "
			"(reg == 0x%02x, val == 0x%02x, ret == %i)\n",
			__FUNCTION__, reg, data, ret);

	mdelay(10);
	return (ret != 1) ? -EREMOTEIO : 0;;
}


static
u8 tda8044_readreg (struct dvb_i2c_bus *i2c, u8 reg)
{
	int ret;
	u8 b0 [] = { reg };
	u8 b1 [] = { 0x00 };
	struct i2c_msg msg [] = { { addr: 0x68, flags: 0, buf: b0, len: 1 },
			   { addr: 0x68, flags: I2C_M_RD, buf: b1, len: 1 } };

	ret = i2c->xfer (i2c, msg, 2);

	if (ret != 2)
		dprintk("%s: readreg error (ret == %i)\n", __FUNCTION__, ret);

	return b1[0];
}


static
int tda8044_set_freq (struct dvb_i2c_bus *i2c, u32 freq)
{
	int ret;
	u8 buf [] = { ((freq / 1000) >> 8) & 0x7f, (freq / 1000) & 0xff, 0x88, 0x60 };
	struct i2c_msg msg = { addr: 0x63, flags: 0, buf: buf, len: 4 };

	tda8044_writereg(i2c, 0x1c, 0x80);

	ret = i2c->xfer (i2c, &msg, 1);

	tda8044_writereg(i2c, 0x1c, 0x00);

	if (ret != 1)
		dprintk("%s: i2c xfer error (ret == %i)\n",
			__FUNCTION__, ret);

	return (ret != 1) ? -1 : 0;
}


static
int tda8044_init (struct dvb_frontend *frontend)
{
	u32 i;

	for (i = 0; i < sizeof(tda8044_inittab); i++)
		if (tda8044_writereg(frontend->i2c, i, tda8044_inittab[i]) < 0)
			return -1;

	udelay(10000);

	tda8044_writereg(frontend->i2c, 0x0F, 0x50);
	//tda8044_writereg(frontend->i2c, 0x20, 0x8F);
	//tda8044_writereg(frontend->i2c, 0x20, 0xBF);
	//tda8044_writereg(frontend->i2c, 0x00, 0x04);
	//tda8044_writereg(frontend->i2c, 0x00, 0x0C);
	tda8044_writereg(frontend->i2c, 0x00, 0x08); /* Reset AFC1 loop filter */

	udelay(10000);

	tda8044_inittab[0x00] = 0x00; /* 0x04: request interrupt */
	tda8044_inittab[0x0F] = 0x50;
	tda8044_inittab[0x16] |= fbcn << 4;
	tda8044_inittab[0x1F] = 0x7F;

	for (i = 0; i < sizeof(tda8044_inittab); i++)
		if (tda8044_writereg(frontend->i2c, i, tda8044_inittab[i]) < 0)
			return -1;

	return 0;
}


static
int tda8044_write_buf (struct dvb_i2c_bus *i2c, u8 *buf, u8 len)
{
	int ret;
	struct i2c_msg msg = { addr: 0x68, flags: 0, buf: buf, len: len };

	ret = i2c->xfer (i2c, &msg, 1);

	if (ret != 1) {
		dprintk("%s: i2c xfer error (ret == %i)\n", __FUNCTION__, ret);
		return -EREMOTEIO;
	}

	return 0;
}


static
int tda8044_request_interrupt (struct dvb_i2c_bus *i2c)
{
	u8 reg0 = tda8044_readreg(i2c, 0x00);

	return tda8044_writereg(i2c, 0x00, reg0 | 0x04);
}


static
int tda8044_set_parameters (struct dvb_i2c_bus *i2c,
			    fe_spectral_inversion_t inversion,
			    u32 symbol_rate, fe_code_rate_t fec_inner)
{
	u8 buf [16];
	u32 ratio;

	/* register */
	buf[0x00] = 0x01;

	/*
	 * Viterbi decoder:
	 * Differential decoding off
	 * Spectral inversion unknown
	 * QPSK modulation
	 */
	buf[0x01] = 0x00;

	if (inversion == INVERSION_ON)
		buf[0x01] |= 0x60;
	else if (inversion == INVERSION_OFF)
		buf[0x01] |= 0x20;

	/*
	 * CLK ratio:
	 * 
	 * system clock frequency is 96000000 Hz
	 * TODO: calculate better ;)
	 * formula: 2^21 * freq / symrate
	 * 
	 * clock and symbol rate are divided by 100000
	 * to avoid an integer overflow
	 */
	ratio = (2 << 20) * 960 / (symbol_rate / 100000);
	buf[0x02] = ratio >> 16;
	buf[0x03] = ratio >> 8;
	buf[0x04] = ratio;

	/* nyquist filter roll-off factor 35% */
	buf[0x05] = 0x20;

	/* Anti Alias Filter */
	if (symbol_rate < 4500000)
		printk("%s: unsupported symbol rate: %d\n", __FILE__, symbol_rate);
	else if (symbol_rate <= 6000000)
		buf[0x05] |= 0x07;
	else if (symbol_rate <= 9000000)
		buf[0x05] |= 0x06;
	else if (symbol_rate <= 12000000)
		buf[0x05] |= 0x05;
	else if (symbol_rate <= 18000000)
		buf[0x05] |= 0x04;
	else if (symbol_rate <= 24000000)
		buf[0x05] |= 0x03;
	else if (symbol_rate <= 36000000)
		buf[0x05] |= 0x02;
	else if (symbol_rate <= 45000000)
		buf[0x05] |= 0x01;
	else
		printk("%s: unsupported symbol rate: %d\n", __FILE__, symbol_rate);

	/* Sigma Delta converter */
	buf[0x06] = 0x00;

	/* FEC: Possible puncturing rates */
	if (fec_inner == FEC_NONE)
		buf[0x07] = 0x00;
	else if ((fec_inner >= FEC_1_2) && (fec_inner <= FEC_8_9))
		buf[0x07] = (1 << (8 - FEC_1_2));
	else if (fec_inner == FEC_AUTO)
		buf[0x07] = 0xff;
	else
		return -EINVAL;

	/* carrier lock detector threshold value */
	buf[0x08] = 0x30;
	/* AFC1: proportional part settings */
	buf[0x09] = 0x42;
	/* AFC1: integral part settings */
	buf[0x0a] = 0x98;
	/* PD: Leaky integrator SCPC mode */
	buf[0x0b] = 0x28;
	/* AFC2, AFC1 controls */
	buf[0x0c] = 0x30;
	/* PD: proportional part settings */
	buf[0x0d] = 0x42;
	/* PD: integral part settings */
	buf[0x0e] = 0x99;
	/* AGC */
	buf[0x0f] = 0x50;

	return tda8044_write_buf(i2c, buf, sizeof(buf));
}


static
int tda8044_set_clk (struct dvb_i2c_bus *i2c)
{
	u8 buf [3];

	/* register */
	buf[0x00] = 0x17;
	/* CLK proportional part */
	buf[0x01] = 0x68;
	/* CLK integral part */
	buf[0x02] = 0x9a;

	return tda8044_write_buf(i2c, buf, sizeof(buf));
}


static
int tda8044_set_scpc_freq_offset (struct dvb_i2c_bus *i2c)
{
	return tda8044_writereg(i2c, 0x22, 0xf9);
}


static
int tda8044_close_loop (struct dvb_i2c_bus *i2c)
{
	u8 buf [3];

	/* register */
	buf[0x00] = 0x0b;
	/* PD: Loop closed, LD: lock detect enable, SCPC: Sweep mode - AFC1 loop closed */
	buf[0x01] = 0x68;
	/* AFC1: Loop closed, CAR Feedback: 8192 */
	buf[0x02] = 0x70;

	return tda8044_write_buf(i2c, buf, sizeof(buf));
}


static
int tda8044_set_voltage (struct dvb_i2c_bus *i2c, fe_sec_voltage_t voltage)
{
	switch (voltage) {
	case SEC_VOLTAGE_13:
		return tda8044_writereg(i2c, 0x20, 0x3f);
	case SEC_VOLTAGE_18:
		return tda8044_writereg(i2c, 0x20, 0xbf);
	default:
		return -EINVAL;
	}
}


static
int tda8044_set_tone (struct dvb_i2c_bus *i2c, fe_sec_tone_mode_t tone)
{
	switch (tone) {
	case SEC_TONE_OFF:
		return tda8044_writereg(i2c, 0x29, 0x00);
	case SEC_TONE_ON:
		return tda8044_writereg(i2c, 0x29, 0x80);
	default:
		return -EINVAL;
	}
}


static
int tda8044_send_diseqc_msg (struct dvb_i2c_bus *i2c, struct dvb_diseqc_master_cmd * cmd)
{
	u8 buf [cmd->msg_len + 1];

	/* register */
	buf[0] = 0x23;
	/* diseqc command */
	memcpy(buf + 1, cmd->msg, cmd->msg_len);

	tda8044_write_buf(i2c, buf, cmd->msg_len + 1);
	tda8044_writereg(i2c, 0x29, 0x0c + (cmd->msg_len - 3));

	return 0;
}


static
int tda8044_send_diseqc_burst (struct dvb_i2c_bus *i2c, fe_sec_mini_cmd_t cmd)
{
	switch (cmd) {
	case SEC_MINI_A:
		return tda8044_writereg(i2c, 0x29, 0x14);
	case SEC_MINI_B:
		return tda8044_writereg(i2c, 0x29, 0x1c);
	default:
		return -EINVAL;
	}
}


static
int tda8044_sleep (struct dvb_i2c_bus *i2c)
{
	return tda8044_writereg(i2c, 0x00, 0x02);
}


static
int tda8044_reset (struct dvb_i2c_bus *i2c)
{
	u8 reg0 = tda8044_readreg(i2c, 0x00);

	tda8044_writereg(i2c, 0x00, reg0 | 0x35);
	tda8044_writereg(i2c, 0x00, reg0);

	return 0;
}


static
int tda8044_ioctl (struct dvb_frontend *frontend, unsigned int cmd, void *arg)
{
	switch (cmd) {
	case FE_GET_INFO:
		memcpy (arg, &tda8044_info, sizeof(struct dvb_frontend_info));
		break;

	case FE_READ_STATUS:
	{
		fe_status_t *status = (fe_status_t *) arg;
		int sync;

		*status = 0;

		sync = tda8044_readreg (frontend->i2c, 0x02);

		if (sync & 0x01) /* demodulator lock */
			*status |= FE_HAS_SIGNAL;
		
		if (sync & 0x02) /* clock recovery lock */
			*status |= FE_HAS_CARRIER;
		
		if (sync & 0x04) /* viterbi lock */
			*status |= FE_HAS_VITERBI;
		
		if (sync & 0x08) /* deinterleaver lock */
			*status |= FE_HAS_SYNC;
		
		if (sync & 0x10) /* derandomizer lock */
			*status |= FE_HAS_LOCK;

		break;
	}

	case FE_READ_BER:
		*((u32*) arg) = ((tda8044_readreg(frontend->i2c, 0x0b) & 0x1f) << 16) |
				(tda8044_readreg(frontend->i2c, 0x0c) << 8) |
				tda8044_readreg(frontend->i2c, 0x0d);

		/* FIXME: scale to bit errors per 10^9 bits */
		*((u32*) arg) *= 1953125 / (2 << (5 + fbcn));

	case FE_READ_SIGNAL_STRENGTH:
	{
		u8 gain = ~tda8044_readreg(frontend->i2c, 0x01);
		*((u16*) arg) = (gain << 8) | gain;
		break;
	}

	case FE_READ_SNR:
	{
		u8 quality = tda8044_readreg(frontend->i2c, 0x08);
		*((u16*) arg) = (quality << 8) | quality;
		break;
	}

	case FE_READ_UNCORRECTED_BLOCKS:
	{
		*((u32*) arg) = tda8044_readreg(frontend->i2c, 0x0f);
		if (*((u32*) arg) == 0xff)
			*((u32*) arg) = 0xffffffff;
		break;
	}

	case FE_SET_FRONTEND:
	{
		struct dvb_frontend_parameters * p = arg;

		tda8044_set_freq(frontend->i2c, p->frequency);

		if (0)
		tda8044_request_interrupt(frontend->i2c);

		tda8044_set_parameters(frontend->i2c, p->inversion, p->u.qpsk.symbol_rate, p->u.qpsk.fec_inner);
		tda8044_set_clk(frontend->i2c);
		tda8044_set_scpc_freq_offset(frontend->i2c);
		tda8044_close_loop(frontend->i2c);

		break;
	}

	case FE_GET_FRONTEND:
	{
		struct dvb_frontend_parameters * p = arg;

		u8 reg0e = tda8044_readreg(frontend->i2c, 0x0e);

		/* FIXME: p->frequency = */

		p->inversion = (reg0e & 0x80) >> 7;

		/* FIXME: p->u.qpsk.symbol_rate = */

		if ((reg0e & 0x07) == 0)
			p->u.qpsk.fec_inner = FEC_8_9;
		else
			p->u.qpsk.fec_inner = reg0e & 0x07;
		break;
	}

	case FE_SLEEP:
		return tda8044_sleep(frontend->i2c);

	case FE_INIT:
		return tda8044_init(frontend);

	case FE_RESET:
		return tda8044_reset(frontend->i2c);

	case FE_DISEQC_SEND_MASTER_CMD:
		return tda8044_send_diseqc_msg(frontend->i2c, arg);

	case FE_DISEQC_SEND_BURST:
		return tda8044_send_diseqc_burst(frontend->i2c, (fe_sec_mini_cmd_t) arg);

	case FE_SET_TONE:
		return tda8044_set_tone(frontend->i2c, (fe_sec_tone_mode_t) arg);

	case FE_SET_VOLTAGE:
		return tda8044_set_voltage(frontend->i2c, (fe_sec_voltage_t) arg);

	default:
		return -EOPNOTSUPP;
	}
	
	return 0;
}


static
int tda8044_attach (struct dvb_i2c_bus *i2c)
{
	if (tda8044_writereg(i2c, 0x89, 0x00) < 0)
		return -ENODEV;

	if (tda8044_readreg(i2c, 0x00) != 0x04)
		return -ENODEV;

	dvb_register_frontend (tda8044_ioctl, i2c, NULL, &tda8044_info);

	return 0;
}


static
void tda8044_detach (struct dvb_i2c_bus *i2c)
{
	dvb_unregister_frontend (tda8044_ioctl, i2c);
}


static
int __init init_tda8044 (void)
{
	return dvb_register_i2c_device (THIS_MODULE,
					tda8044_attach, tda8044_detach);
}


static
void __exit exit_tda8044 (void)
{
	dvb_unregister_i2c_device (tda8044_attach);
}


module_init(init_tda8044);
module_exit(exit_tda8044);

MODULE_DESCRIPTION("TDA8044H QPSK Demodulator");
MODULE_AUTHOR("Felix Domke");
MODULE_LICENSE("GPL");
MODULE_PARM(debug,"i");

