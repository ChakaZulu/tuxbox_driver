/* 
 * tda80xx.c
 *
 * Philips TDA8044 / TDA8083 QPSK demodulator driver
 *
 * Copyright (C) 2001 Felix Domke <tmbinc@elitedvb.net>
 * Copyright (C) 2002-2004 Andreas Oberritter <obi@linuxtv.org>
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

#include <linux/config.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/threads.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <asm/div64.h>

#include "dvb_frontend.h"
#include "dvb_functions.h"

enum {
	ID_TDA8044 = 0x04,
	ID_TDA8083 = 0x05,
};

enum {
	I2C_ADDR_TSA5522 = 0x61,
	I2C_ADDR_TSA5059 = 0x63,
	I2C_ADDR_TDA80xx = 0x68,
};

#if defined(CONFIG_DBOX2)
#define TDA80xx_IRQ		14
#else
#define TDA80xx_IRQ		0
#endif

/* board specific definitions for register 0x20 (gpio) */
#if defined(CONFIG_DBOX2)
#define EXP_LT			0x00
#define EXP_VOLT_13		0x3f
#define EXP_VOLT_18		0xbf
#else
#undef  EXP_LT
#define EXP_VOLT_13		0x00
#define EXP_VOLT_18		0x11
#endif

#define TSA5059_PLL_CLK		4000000	/* 4 MHz */

static int debug = 1;
#define dprintk	if (debug) printk

struct tda80xx {
	u32 clk;
	int afc_loop;
	unsigned int irq;
	struct work_struct worklet;
	fe_code_rate_t code_rate;
	fe_spectral_inversion_t spectral_inversion;
	fe_status_t status;
	struct dvb_i2c_bus *i2c;
	int (*set_pll)(struct dvb_i2c_bus *i2c, u32 freq);
	int (*init)(struct dvb_i2c_bus *i2c);
};

static struct dvb_frontend_info tda80xx_info = {
	.name = "TDA80xx QPSK Demodulator",
	.type = FE_QPSK,
	.frequency_min = 500000,
	.frequency_max = 2700000,
	.frequency_stepsize = 125,
	.symbol_rate_min = 4500000,
	.symbol_rate_max = 45000000,
	.notifier_delay = 0,
	.caps =	FE_CAN_INVERSION_AUTO |
		FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
		FE_CAN_FEC_4_5 | FE_CAN_FEC_5_6 | FE_CAN_FEC_6_7 |
		FE_CAN_FEC_7_8 | FE_CAN_FEC_8_9 | FE_CAN_FEC_AUTO |
		FE_CAN_QPSK |
		FE_CAN_MUTE_TS | FE_CAN_CLEAN_SETUP
};

static u8 tda8044_inittab[] = {
	0x02, 0x00, 0x6f, 0xb5, 0x86, 0x22, 0x00, 0xea,
	0x30, 0x42, 0x98, 0x68, 0x70, 0x42, 0x99, 0x58,
	0x95, 0x10, 0xf5, 0xe7, 0x93, 0x0b, 0x15, 0x68,
	0x9a, 0x90, 0x61, 0x80, 0x00, 0xe0, 0x40, 0x00,
	0x0f, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00
};

static u8 tda8083_inittab[] = {
	0x04, 0x00, 0x4a, 0x79, 0x04, 0x00, 0xff, 0xea,
	0x48, 0x42, 0x79, 0x60, 0x70, 0x52, 0x9a, 0x10,
	0x0e, 0x10, 0xf2, 0xa7, 0x93, 0x0b, 0x05, 0xc8,
	0x9d, 0x00, 0x42, 0x80, 0x00, 0x60, 0x40, 0x00,
	0x00, 0x75, 0x00, 0xe0, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};

static __inline__ u32 tda80xx_div(u32 a, u32 b)
{
	return (a + (b / 2)) / b;
}

static __inline__ u32 tda80xx_gcd(u32 a, u32 b)
{
	u32 r;

	while ((r = a % b)) {
		a = b;
		b = r;
	}

	return b;
}

static int tda80xx_read(struct dvb_i2c_bus *i2c, u8 reg, u8 *buf, u8 len)
{
	int ret;
	struct i2c_msg msg[] = { { .addr = I2C_ADDR_TDA80xx, .flags = 0, .buf = &reg, .len = 1 },
			  { .addr = I2C_ADDR_TDA80xx, .flags = I2C_M_RD, .buf = buf, .len = len } };

	ret = i2c->xfer(i2c, msg, 2);

	if (ret != 2)
		dprintk("%s: readreg error (reg %02x, ret == %i)\n",
				__FUNCTION__, reg, ret);

	mdelay(10);

	return (ret == 2) ? 0 : -EREMOTEIO;
}

static int tda80xx_write(struct dvb_i2c_bus *i2c, u8 reg, const u8 *buf, u8 len)
{
	int ret;
	u8 wbuf[len + 1];
	struct i2c_msg msg = { .addr = I2C_ADDR_TDA80xx, .flags = 0, .buf = wbuf, .len = len + 1 };

	wbuf[0] = reg;
	memcpy(&wbuf[1], buf, len);

	ret = i2c->xfer(i2c, &msg, 1);

	if (ret != 1)
		dprintk("%s: i2c xfer error (ret == %i)\n", __FUNCTION__, ret);

	mdelay(10);

	return (ret == 1) ? 0 : -EREMOTEIO;
}

static __inline__ u8 tda80xx_readreg(struct dvb_i2c_bus *i2c, u8 reg)
{
	u8 val;

	tda80xx_read(i2c, reg, &val, 1);

	return val;
}

static __inline__ int tda80xx_writereg(struct dvb_i2c_bus *i2c, u8 reg, u8 data)
{
	return tda80xx_write(i2c, reg, &data, 1);
}

static int tda80xx_pll_read_write(struct dvb_i2c_bus *i2c, u8 addr, u8 *buf, u8 len, int flags)
{
	int ret;
	struct i2c_msg msg = { .addr = addr, .flags = flags, .buf = buf, .len = len };

	tda80xx_writereg(i2c, 0x1c, 0x80);
	ret = i2c->xfer(i2c, &msg, 1);
	tda80xx_writereg(i2c, 0x1c, 0x00);

	if (ret != 1)
		dprintk("%s: i2c xfer error (ret == %i)\n",
			__FUNCTION__, ret);

	return (ret == 1) ? 0 : -EREMOTEIO;
}

static __inline__ int tda80xx_pll_read(struct dvb_i2c_bus *i2c, u8 addr, u8 *buf, u8 len)
{
	return tda80xx_pll_read_write(i2c, addr, buf, len, I2C_M_RD);
}

static __inline__ int tda80xx_pll_write(struct dvb_i2c_bus *i2c, u8 addr, u8 *buf, u8 len)
{
	return tda80xx_pll_read_write(i2c, addr, buf, len, 0);
}

static int tsa5059_set_pll(struct dvb_i2c_bus *i2c, u32 freq)
{
	u8 buf[4];
	u32 ref;
	u8 cp;
	u8 pe;
	u8 r;
	int diff;
	int i;

	if (freq < 1100000)		/*  555uA */
		cp = 2;
	else if (freq < 1200000)	/*  260uA */
		cp = 1;
	else if (freq < 1600000)	/*  120uA */
		cp = 0;
	else if (freq < 1800000)	/*  260uA */
		cp = 1;
	else if (freq < 2000000)	/*  555uA */
		cp = 2;
	else				/* 1200uA */
		cp = 3;

	if (freq <= 2300000)
		pe = 0;
	else if (freq <= 2700000)
		pe = 1;
	else
		return -EINVAL;

	diff = INT_MAX;
	ref = r = 0;

	/* allow 2000kHz - 125kHz */
	for (i = 0; i < 5; i++) {
		u32 cfreq = tda80xx_div(TSA5059_PLL_CLK, (2 << i));
		u32 tmpref = tda80xx_div((freq * 1000), (cfreq << pe));
		int tmpdiff = (freq * 1000) - (tmpref * (cfreq << pe));

		if (abs(tmpdiff) > abs(diff))
			continue;

		diff = tmpdiff;
		ref = tmpref;
		r = i;

		if (diff == 0)
			break;
	}

	buf[0] = (ref >> 8) & 0x7f;
	buf[1] = (ref >> 0) & 0xff;
	buf[2] = 0x80 | ((ref >> 10) & 0x60) | (pe << 4) | r;
	buf[3] = (cp << 6) | 0x40;

	return tda80xx_pll_write(i2c, I2C_ADDR_TSA5059, buf, sizeof(buf));
}

static int tsa5522_set_pll(struct dvb_i2c_bus *i2c, u32 freq)
{
	u32 div;
	u8 buf[4];

	div = tda80xx_div(freq, 125);

	buf[0] = (div >> 8) & 0x7f;
	buf[1] = (div >> 0) & 0xff;
	buf[2] = 0x8e;
	buf[3] = 0x00;

	return tda80xx_pll_write(i2c, I2C_ADDR_TSA5522, buf, sizeof(buf));
}

static int tda8044_init(struct dvb_i2c_bus *i2c)
{
	int ret;

	/*
	 * this function is a mess...
	 */

	if ((ret = tda80xx_write(i2c, 0x00, tda8044_inittab, sizeof(tda8044_inittab))))
		return ret;

	tda80xx_writereg(i2c, 0x0f, 0x50);
#if 1
	tda80xx_writereg(i2c, 0x20, 0x8F);		/* FIXME */
	tda80xx_writereg(i2c, 0x20, EXP_VOLT_18);	/* FIXME */
	//tda80xx_writereg(i2c, 0x00, 0x04);
	tda80xx_writereg(i2c, 0x00, 0x0C);
#endif
	//tda80xx_writereg(i2c, 0x00, 0x08); /* Reset AFC1 loop filter */

	tda8044_inittab[0x00] = 0x04;	/* 0x04: request interrupt */
	tda8044_inittab[0x0f] = 0x50;	/* AGC: 0, AGI, ADI, ADOM, AINV, 0, SCD[1:0] */
	tda8044_inittab[0x1f] = 0x6c;

	return tda80xx_write(i2c, 0x00, tda8044_inittab, sizeof(tda8044_inittab));
}

static int tda8083_init(struct dvb_i2c_bus *i2c)
{
	return tda80xx_write(i2c, 0x00, tda8083_inittab, sizeof(tda8083_inittab));
}

static int tda80xx_set_parameters(struct tda80xx *tda,
				  fe_spectral_inversion_t inversion,
				  u32 symbol_rate,
				  fe_code_rate_t fec_inner)
{
	struct dvb_i2c_bus *i2c = tda->i2c;
	u8 buf[15];
	u64 ratio;
	u32 clk;
	u32 k;
	u32 sr = symbol_rate;
	u32 gcd;
	u8 scd;

	if (symbol_rate > (tda->clk * 3) / 16)
		scd = 0;
	else if (symbol_rate > (tda->clk * 3) / 32)
		scd = 1;
	else if (symbol_rate > (tda->clk * 3) / 64)
		scd = 2;
	else
		scd = 3;

	clk = scd ? (tda->clk / (scd * 2)) : tda->clk;

	/*
	 * Viterbi decoder:
	 * Differential decoding off
	 * Spectral inversion unknown
	 * QPSK modulation
	 */
	if (inversion == INVERSION_ON)
		buf[0] = 0x60;
	else if (inversion == INVERSION_OFF)
		buf[0] = 0x20;
	else
		buf[0] = 0x00;

	/*
	 * CLK ratio:
	 * system clock frequency is up to 64 or 96 MHz
	 *
	 * formula:
	 * r = k * clk / symbol_rate
	 *
	 * k:	2^21 for caa 0..3,
	 * 	2^20 for caa 4..5,
	 * 	2^19 for caa 6..7
	 */
	if (symbol_rate <= (clk * 3) / 32)
		k = (1 << 19);
	else if (symbol_rate <= (clk * 3) / 16)
		k = (1 << 20);
	else
		k = (1 << 21);

	gcd = tda80xx_gcd(clk, sr);
	clk /= gcd;
	sr /= gcd;

	gcd = tda80xx_gcd(k, sr);
	k /= gcd;
	sr /= gcd;

	ratio = (u64)k * (u64)clk;
	do_div(ratio, sr);

	buf[1] = ratio >> 16;
	buf[2] = ratio >> 8;
	buf[3] = ratio;

	/* nyquist filter roll-off factor 35% */
	buf[4] = 0x20;

	clk = scd ? (tda->clk / (scd * 2)) : tda->clk;

	/* Anti Alias Filter */
	if (symbol_rate < (clk * 3) / 64)
		printk("tda80xx: unsupported symbol rate: %u\n", symbol_rate);
	else if (symbol_rate <= clk / 16)
		buf[4] |= 0x07;
	else if (symbol_rate <= (clk * 3) / 32)
		buf[4] |= 0x06;
	else if (symbol_rate <= clk / 8)
		buf[4] |= 0x05;
	else if (symbol_rate <= (clk * 3) / 16)
		buf[4] |= 0x04;
	else if (symbol_rate <= clk / 4)
		buf[4] |= 0x03;
	else if (symbol_rate <= (clk * 3) / 8)
		buf[4] |= 0x02;
	else if (symbol_rate <= clk / 2)
		buf[4] |= 0x01;
	else
		buf[4] |= 0x00;

	/* Sigma Delta converter */
	buf[5] = 0x00;

	/* FEC: Possible puncturing rates */
	if (fec_inner == FEC_NONE)
		buf[6] = 0x00;
	else if ((fec_inner >= FEC_1_2) && (fec_inner <= FEC_8_9))
		buf[6] = (1 << (8 - fec_inner));
	else if (fec_inner == FEC_AUTO)
		buf[6] = 0xff;
	else
		return -EINVAL;

	/* carrier lock detector threshold value */
	buf[7] = 0x30;
	/* AFC1: proportional part settings */
	buf[8] = 0x42;
	/* AFC1: integral part settings */
	buf[9] = 0x98;
	/* PD: Leaky integrator SCPC mode */
	buf[10] = 0x28;
	/* AFC2, AFC1 controls */
	buf[11] = 0x30;
	/* PD: proportional part settings */
	buf[12] = 0x42;
	/* PD: integral part settings */
	buf[13] = 0x99;
	/* AGC */
	buf[14] = 0x50 | scd;

	printk("symbol_rate=%u clk=%u\n", symbol_rate, clk);

	return tda80xx_write(i2c, 0x01, buf, sizeof(buf));
}

static int tda80xx_set_clk(struct dvb_i2c_bus *i2c)
{
	u8 buf[2];

	/* CLK proportional part */
	buf[0] = (0x06 << 5) | 0x08;	/* CMP[2:0], CSP[4:0] */
	/* CLK integral part */
	buf[1] = (0x04 << 5) | 0x1a;	/* CMI[2:0], CSI[4:0] */

	return tda80xx_write(i2c, 0x17, buf, sizeof(buf));
}

#if 0
static int tda80xx_set_scpc_freq_offset(struct dvb_i2c_bus *i2c)
{
	/* a constant value is nonsense here imho */
	return tda80xx_writereg(i2c, 0x22, 0xf9);
}
#endif

static int tda80xx_close_loop(struct dvb_i2c_bus *i2c)
{
	u8 buf[2];

	/* PD: Loop closed, LD: lock detect enable, SCPC: Sweep mode - AFC1 loop closed */
	buf[0] = 0x68;
	/* AFC1: Loop closed, CAR Feedback: 8192 */
	buf[1] = 0x70;

	return tda80xx_write(i2c, 0x0b, buf, sizeof(buf));
}

static int tda80xx_set_voltage(struct dvb_i2c_bus *i2c, fe_sec_voltage_t voltage)
{
	switch (voltage) {
	case SEC_VOLTAGE_13:
		return tda80xx_writereg(i2c, 0x20, EXP_VOLT_13);
	case SEC_VOLTAGE_18:
		return tda80xx_writereg(i2c, 0x20, EXP_VOLT_18);
#if defined(EXP_LT)
	case SEC_VOLTAGE_OFF:
		return tda80xx_writereg(i2c, 0x20, EXP_LT);
#endif
	default:
		return -EINVAL;
	}
}

static int tda80xx_set_tone(struct dvb_i2c_bus *i2c, fe_sec_tone_mode_t tone)
{
	switch (tone) {
	case SEC_TONE_OFF:
		return tda80xx_writereg(i2c, 0x29, 0x00);
	case SEC_TONE_ON:
		return tda80xx_writereg(i2c, 0x29, 0x80);
	default:
		return -EINVAL;
	}
}

static void tda80xx_wait_diseqc_fifo(struct dvb_i2c_bus *i2c)
{
	size_t i;

	for (i = 0; i < 100; i++) {
		if (tda80xx_readreg(i2c, 0x02) & 0x80)
			break;
		dvb_delay(10);
	}
}

static int tda80xx_send_diseqc_msg(struct dvb_i2c_bus *i2c, struct dvb_diseqc_master_cmd *cmd)
{
	if (cmd->msg_len > 6)
		return -EINVAL;

	tda80xx_writereg(i2c, 0x29, 0x08 | (cmd->msg_len - 3));
	tda80xx_write(i2c, 0x23, cmd->msg, cmd->msg_len);
	tda80xx_writereg(i2c, 0x29, 0x0c | (cmd->msg_len - 3));
	tda80xx_wait_diseqc_fifo(i2c);

	return 0;
}

static int tda80xx_send_diseqc_burst(struct dvb_i2c_bus *i2c, fe_sec_mini_cmd_t cmd)
{
	switch (cmd) {
	case SEC_MINI_A:
		tda80xx_writereg(i2c, 0x29, 0x14);
		break;
	case SEC_MINI_B:
		tda80xx_writereg(i2c, 0x29, 0x1c);
		break;
	default:
		return -EINVAL;
	}

	tda80xx_wait_diseqc_fifo(i2c);

	return 0;
}

static int tda80xx_sleep(struct dvb_i2c_bus *i2c)
{
#if defined(EXP_LT)
	tda80xx_writereg(i2c, 0x20, EXP_LT);	/* enable loop through */
#endif
	tda80xx_writereg(i2c, 0x00, 0x02);	/* enter standby */

	return 0;
}

static int tda80xx_reset(struct dvb_i2c_bus *i2c)
{
	tda80xx_writereg(i2c, 0x00, 0x3c);
	tda80xx_writereg(i2c, 0x00, 0x04);

	return 0;
}

static void tda80xx_read_status(struct tda80xx *tda)
{
	u8 val;

	static const fe_spectral_inversion_t inv_tab[] = {
		INVERSION_OFF, INVERSION_ON
	};

	static const fe_code_rate_t fec_tab[] = {
		FEC_8_9, FEC_1_2, FEC_2_3, FEC_3_4,
		FEC_4_5, FEC_5_6, FEC_6_7, FEC_7_8,
	};

	if (debug) {
		tda80xx_pll_read(tda->i2c, I2C_ADDR_TSA5059, &val, 1);

		printk(KERN_DEBUG "pll status: %02x [POR=%x FL=%x I=%x A=%x]\n", val,
			(val >> 7) & 1,
			(val >> 6) & 1,
			(val >> 3) & 7,
			(val >> 0) & 7);
	}

	val = tda80xx_readreg(tda->i2c, 0x02);

	tda->status = 0;

	if (val & 0x01) /* demodulator lock */
		tda->status |= FE_HAS_SIGNAL;
	if (val & 0x02) /* clock recovery lock */
		tda->status |= FE_HAS_CARRIER;
	if (val & 0x04) /* viterbi lock */
		tda->status |= FE_HAS_VITERBI;
	if (val & 0x08) /* deinterleaver lock (packet sync) */
		tda->status |= FE_HAS_SYNC;
	if (val & 0x10) /* derandomizer lock (frame sync) */
		tda->status |= FE_HAS_LOCK;
	if (val & 0x20) /* frontend can not lock */
		tda->status |= FE_TIMEDOUT;

	if ((tda->status & (FE_HAS_CARRIER)) && (tda->afc_loop)) {
		printk("tda80xx: closing loop\n");
		tda80xx_close_loop(tda->i2c);
		tda->afc_loop = 0;
	}

	if (tda->status & (FE_HAS_VITERBI | FE_HAS_SYNC | FE_HAS_LOCK)) {
		val = tda80xx_readreg(tda->i2c, 0x0e);
		tda->code_rate = fec_tab[val & 0x07];
		if (tda->status & (FE_HAS_SYNC | FE_HAS_LOCK))
			tda->spectral_inversion = inv_tab[(val >> 7) & 0x01];
		else
			tda->spectral_inversion = INVERSION_AUTO;
	}
	else {
		tda->code_rate = FEC_AUTO;
	}
}

static int tda80xx_set_frontend(struct tda80xx *tda, struct dvb_frontend_parameters *p)
{
	struct dvb_i2c_bus *i2c = tda->i2c;

	tda->set_pll(i2c, p->frequency);

	tda80xx_set_parameters(tda, p->inversion, p->u.qpsk.symbol_rate, p->u.qpsk.fec_inner);
	tda80xx_set_clk(i2c);
	//tda80xx_set_scpc_freq_offset(i2c);
	tda->afc_loop = 1;

	return 0;
}

static int tda80xx_get_frontend(struct tda80xx *tda, struct dvb_frontend_parameters *p)
{
	if (!tda->irq)
		tda80xx_read_status(tda);

	p->inversion = tda->spectral_inversion;
	p->u.qpsk.fec_inner = tda->code_rate;

	return 0;
}

static int tda80xx_ioctl(struct dvb_frontend *fe, unsigned int cmd, void *arg)
{
	struct dvb_i2c_bus *i2c = fe->i2c;
	struct tda80xx *tda = fe->data;

	switch (cmd) {
	case FE_GET_INFO:
		memcpy(arg, &tda80xx_info, sizeof(struct dvb_frontend_info));
		break;

	case FE_READ_STATUS:
		if (!tda->irq)
			tda80xx_read_status(tda);
		*(fe_status_t *)arg = tda->status;
		break;

	case FE_READ_BER:
	{
		int ret;
		u8 buf[3];

		if ((ret = tda80xx_read(i2c, 0x0b, buf, sizeof(buf))))
			return ret;

		*(u32 *)arg = ((buf[0] & 0x1f) << 16) | (buf[1] << 8) | buf[2];
		break;
	}

	case FE_READ_SIGNAL_STRENGTH:
	{
		u8 gain = ~tda80xx_readreg(i2c, 0x01);
		*((u16*) arg) = (gain << 8) | gain;
		break;
	}

	case FE_READ_SNR:
	{
		u8 quality = tda80xx_readreg(i2c, 0x08);
		*((u16*) arg) = (quality << 8) | quality;
		break;
	}

	case FE_READ_UNCORRECTED_BLOCKS:
	{
		*((u32*) arg) = tda80xx_readreg(i2c, 0x0f);
		if (*((u32*) arg) == 0xff)
			*((u32*) arg) = 0xffffffff;
		break;
	}

	case FE_SET_FRONTEND:
		return tda80xx_set_frontend(tda, arg);

	case FE_GET_FRONTEND:
		return tda80xx_get_frontend(tda, arg);

	case FE_SLEEP:
		return tda80xx_sleep(i2c);

	case FE_INIT:
		return tda->init(i2c);

	case FE_RESET:
		return tda80xx_reset(i2c);

	case FE_DISEQC_SEND_MASTER_CMD:
		return tda80xx_send_diseqc_msg(i2c, arg);

	case FE_DISEQC_SEND_BURST:
		return tda80xx_send_diseqc_burst(i2c, (fe_sec_mini_cmd_t)arg);

	case FE_SET_TONE:
		return tda80xx_set_tone(i2c, (fe_sec_tone_mode_t)arg);

	case FE_SET_VOLTAGE:
		return tda80xx_set_voltage(i2c, (fe_sec_voltage_t)arg);

	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static irqreturn_t tda80xx_irq(int irq, void *priv, struct pt_regs *pt)
{
	schedule_work(priv);

	return IRQ_HANDLED;
}

static void tda80xx_worklet(void *priv)
{
	struct tda80xx *tda = priv;

	tda80xx_writereg(tda->i2c, 0x00, 0x04);
	enable_irq(TDA80xx_IRQ);

	tda80xx_read_status(tda);
}

static int tda80xx_attach(struct dvb_i2c_bus *i2c, void **data)
{
	struct tda80xx *tda;
	int ret;
	u8 id;

	/* write into nirvana */
	if (tda80xx_writereg(i2c, 0x89, 0x00) < 0)
		return -ENODEV;

	id = tda80xx_readreg(i2c, 0x00);

	if ((id != ID_TDA8044) && (id != ID_TDA8083))
		return -ENODEV;

	*data = tda = kmalloc(sizeof(struct tda80xx), GFP_KERNEL);
	if (!tda)
		return -ENOMEM;

	tda->spectral_inversion = INVERSION_AUTO;
	tda->code_rate = FEC_AUTO;
	tda->status = 0;
	tda->i2c = i2c;
	tda->irq = TDA80xx_IRQ;

	switch (id) {
	case ID_TDA8044:
		tda->clk = 96000000;
		tda->set_pll = tsa5059_set_pll;
		tda->init = tda8044_init;
		memcpy(tda80xx_info.name, "TDA8044", 7);
		break;

	case ID_TDA8083:
		tda->clk = 64000000;
		tda->set_pll = tsa5522_set_pll;
		tda->init = tda8083_init;
		memcpy(tda80xx_info.name, "TDA8083", 7);
		break;
	}

	if (tda->irq) {
		INIT_WORK(&tda->worklet, tda80xx_worklet, tda);
		if ((ret = request_irq(tda->irq, tda80xx_irq, SA_ONESHOT, "tda80xx", &tda->worklet)) < 0) {
			printk(KERN_ERR "tda80xx: request_irq failed (%d)\n", ret);
			return ret;
		}
	}

	return dvb_register_frontend(tda80xx_ioctl, i2c, tda, &tda80xx_info);
}

static void tda80xx_detach(struct dvb_i2c_bus *i2c, void *data)
{
	struct tda80xx *tda = data;

	if (tda->irq)
		free_irq(tda->irq, &tda->worklet);

	kfree(data);

	dvb_unregister_frontend(tda80xx_ioctl, i2c);
}

static int __init init_tda80xx(void)
{
	return dvb_register_i2c_device(THIS_MODULE, tda80xx_attach, tda80xx_detach);
}

static void __exit exit_tda80xx(void)
{
	dvb_unregister_i2c_device(tda80xx_attach);
}

module_init(init_tda80xx);
module_exit(exit_tda80xx);

MODULE_DESCRIPTION("TDA8044 / TDA8083 QPSK Demodulator");
MODULE_AUTHOR("Felix Domke, Andreas Oberritter");
MODULE_LICENSE("GPL");
MODULE_PARM(debug,"i");
