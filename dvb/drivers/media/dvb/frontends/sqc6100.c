/*
 * $Id: sqc6100.c,v 1.1 2003/07/29 18:30:30 obi Exp $
 *
 * Infineon SQC6100 DVB-T Frontend Driver
 *
 * (C) 2003 Andreas Oberritter <obi@saftware.de>
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
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "dvb_frontend.h"
#include "sqc6100.h"

#define I2C_ADDR_SQC6100	0x0d

#define SQC6100_DEBUG		1

#define CLK_SYS			115000000UL	/* 115.0 MHz */
#define XTAL_FRQ		 28900000UL	/*  28.9 MHz */
#define IFCLK			(XTAL_FRQ)

static struct dvb_frontend_info sqc6100_info = {
	.name = "Infineon SQC6100",
	.type = FE_OFDM,
	.frequency_min = 470000000,		/* FIXME */
	.frequency_max = 860000000,		/* FIXME */
	.frequency_stepsize = 166667,		/* FIXME */
	.notifier_delay = 0,
	.caps = FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
		FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 |
		FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_64 |
		FE_CAN_MUTE_TS
};

static int sqc6100_read(struct dvb_i2c_bus *i2c,
			const enum sqc6100_reg_addr reg,
			void *buf,
			const size_t count)
{
	int ret;
	struct i2c_msg msg[2];
	u8 regbuf[1] = { reg };

	msg[0].addr = I2C_ADDR_SQC6100;
	msg[0].flags = 0;
	msg[0].buf = regbuf;
	msg[0].len = 1;
	msg[1].addr = I2C_ADDR_SQC6100;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = count;

	ret = i2c->xfer(i2c, msg, 2);

	if (ret != 2) {
		printk(KERN_ERR "%s: ret == %d\n", __FUNCTION__, ret);
		return -EREMOTEIO;
	}

#if SQC6100_DEBUG
	{
		int i;
		printk(KERN_INFO "R(%d):", reg);
		for (i = 0; i < count; i++)
			printk(" %02x", ((const u8 *) buf)[i]);
		printk("\n");
	}
#endif

	return 0;
}

static int sqc6100_write(struct dvb_i2c_bus *i2c,
			 const enum sqc6100_reg_addr reg,
			 const void *src,
			 const size_t count)
{
	int ret;
	u8 buf[count + 1];
	struct i2c_msg msg;

#if SQC6100_DEBUG
	{
		int i;
		printk(KERN_INFO "W(%d):", reg);
		for (i = 0; i < count; i++)
			printk(" %02x", ((const u8 *) src)[i]);
		printk("\n");
	}
#endif

	buf[0] = reg;
	memcpy(&buf[1], src, count);

	msg.addr = I2C_ADDR_SQC6100;
	msg.flags = 0;
	msg.buf = buf;
	msg.len = count + 1;

	ret = i2c->xfer(i2c, &msg, 1);

	if (ret != 1) {
		printk(KERN_ERR "%s: ret == %d\n", __FUNCTION__, ret);
		return -EREMOTEIO;
	}

	return 0;
}

static inline int sqc6100_readreg(struct dvb_i2c_bus *i2c,
				  const enum sqc6100_reg_addr reg,
				  u8 *val)
{
	return sqc6100_read(i2c, reg, val, 1);
}

static inline int sqc6100_writereg(struct dvb_i2c_bus *i2c,
				   const enum sqc6100_reg_addr reg,
				   const u8 val)
{
	return sqc6100_write(i2c, reg, &val, 1);
}

static int sqc6100_read_status(struct dvb_i2c_bus *i2c, fe_status_t *status)
{
	int ret;
	u8 vd_status;
	u8 sd_status;
	u8 ct_status;

	if ((ret = sqc6100_readreg(i2c, SD_STATUS, &sd_status)) < 0)
		return ret;

	if ((ret = sqc6100_readreg(i2c, CT_STATUS, &ct_status)) < 0)
		return ret;

	if ((ret = sqc6100_readreg(i2c, VD_STATUS, &vd_status)) < 0)
		return ret;

	*status = 0;

	if (sd_status & 0x10)
		*status |= FE_HAS_SIGNAL;

	if ((ct_status & 0x06) == 0x06)
		*status |= FE_HAS_CARRIER;

	if (vd_status & 0x80)
		*status |= FE_HAS_VITERBI;

	/* FIXME */
	if (*status == (FE_HAS_SIGNAL | FE_HAS_CARRIER | FE_HAS_VITERBI))
		*status |= FE_HAS_SYNC;

	/* FIXME */
	if (*status == (FE_HAS_SIGNAL | FE_HAS_CARRIER | FE_HAS_VITERBI | FE_HAS_SYNC))
		*status |= FE_HAS_LOCK;

	return 0;
}

static int sqc6100_read_ber(struct dvb_i2c_bus *i2c, u32 *ber)
{
	int ret;
	u8 rs_biterr[2];

	if ((ret = sqc6100_read(i2c, RS_BITERR1, rs_biterr, 2)) < 0)
		return ret;

	/* FIXME: scale */
	*ber = (rs_biterr[1] << 8) | rs_biterr[0];

	return 0;
}

static int sqc6100_read_signal_strength(struct dvb_i2c_bus *i2c,
					u16 *signal_strength)
{
	int ret;
	u8 agc_gain;

	if ((ret = sqc6100_readreg(i2c, AGC_GAIN, &agc_gain)) < 0)
		return ret;

	*signal_strength = (agc_gain << 8) | agc_gain;

	return 0;
}

static int sqc6100_read_snr(struct dvb_i2c_bus *i2c, u16 *snr)
{
	/* FIXME */
	return 0;
}

static int sqc6100_read_uncorrected_blocks(struct dvb_i2c_bus *i2c,
					   u32 *uncorrected_blocks)
{
	int ret;
	u8 rs_frmerr;

	if ((ret = sqc6100_readreg(i2c, RS_FRMERR, &rs_frmerr)) < 0)
		return ret;

	*uncorrected_blocks = rs_frmerr;

	return 0;
}

static int sqc6100_set_frontend(struct dvb_i2c_bus *i2c,
				struct dvb_frontend_parameters *p)
{
	int ret;
	u8 frq_sub[2];
	u8 gc_mode[4];
	u8 rsu_smp[3];
	u32 tmp;

	const u8 ofdm_mode[] =
		{ 0x00, 0x01, 0x00 };
	const u8 code_rate_lo[] =
		{ 0x00, 0x00, 0x04, 0x08, 0x00, 0x0c, 0x00, 0x10, 0x00, 0x00 };
	const u8 code_rate_hi[] =
		{ 0x00, 0x00, 0x20, 0x40, 0x00, 0x60, 0x00, 0x80, 0x00, 0x00 };
	const u8 hierarc_mode[] =
		{ 0x00, 0x02, 0x04, 0x06, 0x00 };
	const u8 guard_interval[] =
		{ 0x00, 0x10, 0x20, 0x30, 0x00 };
	const u8 qam_constell[] =
		{ 0x00, 0x40, 0x00, 0x80, 0x00, 0x00, 0x00 };
	const u32 bandwidth_khz[] =
		{ 8000, 7000, 6000, 8000 };
	const u32 frq_spacing_hz[] =
		{ 4464, 3905, 3346, 4464 };

	if ((p->frequency < sqc6100_info.frequency_min) ||
		(p->frequency > sqc6100_info.frequency_max))
		return -EINVAL;

	/*
	 * xxx_AUTO parameters need interrupts and manual probing
	 * in channel search mode
	 */

	if ((p->inversion < INVERSION_OFF) ||
		(p->inversion > INVERSION_ON))
		return -EINVAL;

	if ((p->u.ofdm.bandwidth < BANDWIDTH_8_MHZ) ||
		(p->u.ofdm.bandwidth > BANDWIDTH_6_MHZ))
		return -EINVAL;

	if ((p->u.ofdm.code_rate_HP < FEC_1_2) ||
		(p->u.ofdm.code_rate_HP > FEC_7_8) ||
		(p->u.ofdm.code_rate_HP == FEC_4_5) ||
		(p->u.ofdm.code_rate_HP == FEC_6_7))
		return -EINVAL;

	if ((p->u.ofdm.code_rate_LP < FEC_1_2) ||
		(p->u.ofdm.code_rate_LP > FEC_7_8) ||
		(p->u.ofdm.code_rate_LP == FEC_4_5) ||
		(p->u.ofdm.code_rate_LP == FEC_6_7))
		return -EINVAL;

	if ((p->u.ofdm.constellation < QPSK) ||
		(p->u.ofdm.constellation > QAM_64) ||
		(p->u.ofdm.constellation == QAM_32))
		return -EINVAL;

	if ((p->u.ofdm.transmission_mode < TRANSMISSION_MODE_2K) ||
		(p->u.ofdm.transmission_mode > TRANSMISSION_MODE_8K))
		return -EINVAL;

	if ((p->u.ofdm.guard_interval < GUARD_INTERVAL_1_32) ||
		(p->u.ofdm.guard_interval > GUARD_INTERVAL_1_4))
		return -EINVAL;

	if ((p->u.ofdm.hierarchy_information < HIERARCHY_NONE) ||
		(p->u.ofdm.hierarchy_information > HIERARCHY_4))
		return -EINVAL;

	/* (2 ^ 21) * (7 / 8) = 12845056 */
	tmp = (12845056 * (XTAL_FRQ / 1000)) / bandwidth_khz[p->u.ofdm.bandwidth];
	rsu_smp[0] = (tmp >>  0) & 0xff;
	rsu_smp[1] = (tmp >>  8) & 0xff;
	rsu_smp[2] = (tmp >> 16) & 0xff;

	gc_mode[0] = code_rate_hi[p->u.ofdm.code_rate_HP] |
		     code_rate_lo[p->u.ofdm.code_rate_LP] |
		     ofdm_mode[p->u.ofdm.transmission_mode];
	gc_mode[1] = qam_constell[p->u.ofdm.constellation] |
		     guard_interval[p->u.ofdm.guard_interval] |
		     hierarc_mode[p->u.ofdm.hierarchy_information];
	gc_mode[2] = 0x4b;
	gc_mode[3] = 0x00;

	if (p->u.ofdm.transmission_mode == TRANSMISSION_MODE_2K) {
		tmp = (16777 * frq_spacing_hz[p->u.ofdm.bandwidth]) / (XTAL_FRQ / 1000);
		frq_sub[0] = (tmp >> 0) & 0xff;
		frq_sub[1] = (tmp >> 8) & 0xff;
	}

#if 0
	if ((ret = sqc6100_set_frequency(i2c, frontend->frequency)) < 0)
		return ret;

	if ((ret = sqc6100_set_inversion(i2c, frontend->inversion)) < 0)
		return ret;
#endif

	if ((ret = sqc6100_write(i2c, RSU_SMP1, rsu_smp, 3)) < 0)
		return ret;

	if ((ret = sqc6100_write(i2c, GC_MODE1, gc_mode, 4)) < 0)
		return ret;

	if (p->u.ofdm.transmission_mode == TRANSMISSION_MODE_2K) {
		if ((ret = sqc6100_write(i2c, FRQ_SUB1, frq_sub, 2)) < 0)
			return ret;
	}

	if ((ret = sqc6100_writereg(i2c, GC_START, 0x02)) < 0)
		return ret;

	return 0;
}

static int sqc6100_get_frontend(struct dvb_i2c_bus *i2c,
				struct dvb_frontend_parameters *p)
{
	return 0;
}

static int sqc6100_sleep(struct dvb_i2c_bus *i2c)
{
	return 0;
}

static int sqc6100_init(struct dvb_i2c_bus *i2c)
{
	int ret;

	/* fix default values */
	if ((ret = sqc6100_writereg(i2c, FS_MODE1, 0x24)) < 0)
		return ret;
	if ((ret = sqc6100_writereg(i2c, FS_MODE2, 0x4f)) < 0)
		return ret;

	return 0;
}

static int sqc6100_reset(struct dvb_i2c_bus *i2c)
{
	int ret;

	/* soft reset */
	if ((ret = sqc6100_writereg(i2c, GC_RES_TSERR, 0x01)) < 0)
		return ret;
	if ((ret = sqc6100_writereg(i2c, GC_RES_TSERR, 0x00)) < 0)
		return ret;

	return 0;
}

static int sqc6100_ioctl(struct dvb_frontend *fe, unsigned int cmd, void *arg)
{
	struct dvb_i2c_bus *i2c = fe->i2c;

	switch (cmd) {
	case FE_GET_INFO:
		memcpy(arg, &sqc6100_info, sizeof(struct dvb_frontend_info));
		break;

	case FE_DISEQC_RESET_OVERLOAD:
	case FE_DISEQC_SEND_MASTER_CMD:
	case FE_DISEQC_RECV_SLAVE_REPLY:
	case FE_DISEQC_SEND_BURST:
	case FE_SET_TONE:
	case FE_SET_VOLTAGE:
	case FE_ENABLE_HIGH_LNB_VOLTAGE:
		return -EOPNOTSUPP;

	case FE_READ_STATUS:
		return sqc6100_read_status(i2c, arg);

	case FE_READ_BER:
		return sqc6100_read_ber(i2c, arg);

	case FE_READ_SIGNAL_STRENGTH:
		return sqc6100_read_signal_strength(i2c, arg);

	case FE_READ_SNR:
		return sqc6100_read_snr(i2c, arg);

	case FE_READ_UNCORRECTED_BLOCKS:
		return sqc6100_read_uncorrected_blocks(i2c, arg);

	case FE_SET_FRONTEND:
		return sqc6100_set_frontend(i2c, arg);

	case FE_GET_FRONTEND:
		return sqc6100_get_frontend(i2c, arg);

	case FE_GET_EVENT:
		return -EOPNOTSUPP;

	case FE_SLEEP:
		return sqc6100_sleep(i2c);

	case FE_INIT:
		return sqc6100_init(i2c);

	case FE_RESET:
		return sqc6100_reset(i2c);

	default:
		return -ENOIOCTLCMD;
	}

	return 0;
}

static int sqc6100_attach(struct dvb_i2c_bus *i2c)
{
	int ret;
	u8 gc_res_tserr;

	if ((ret = sqc6100_readreg(i2c, GC_RES_TSERR, &gc_res_tserr)) < 0)
		return ret;

	if ((gc_res_tserr & 0xe0) != 0)
		return -ENODEV;

	return dvb_register_frontend(sqc6100_ioctl, i2c, NULL, &sqc6100_info);
}

static void sqc6100_detach(struct dvb_i2c_bus *i2c)
{
	dvb_unregister_frontend(sqc6100_ioctl, i2c);
}

static int __init sqc6100_module_init(void)
{
	return dvb_register_i2c_device(THIS_MODULE, sqc6100_attach, sqc6100_detach);
}

static void __exit sqc6100_module_exit(void)
{
	dvb_unregister_i2c_device(sqc6100_attach);
}

module_init(sqc6100_module_init);
module_exit(sqc6100_module_exit);

MODULE_AUTHOR("Andreas Oberritter <obi@saftware.de>");
MODULE_DESCRIPTION("Infineon SQC6100 DVB-T Frontend Driver");
MODULE_LICENSE("GPL");
