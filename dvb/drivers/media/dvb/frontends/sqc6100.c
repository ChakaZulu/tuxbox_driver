/*
 * $Id: sqc6100.c,v 1.4 2003/12/14 15:38:33 wjoost Exp $
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
#include <linux/delay.h>
#include <dbox/dbox2_fp_tuner.h>

#include "dvb_frontend.h"
#include "sqc6100.h"

#define I2C_ADDR_SQC6100	0x0d

#define SQC6100_DEBUG		1
#define SQC6100_PROC_INTERFACE

#ifdef SQC6100_PROC_INTERFACE
#include <linux/proc_fs.h>
static unsigned char sqc6100_proc_registered = 0;
#endif

#define CLK_SYS			115000000UL	/* 115.0 MHz */
#define XTAL_FRQ		 28900000UL	/*  28.9 MHz */
#define IFCLK			(XTAL_FRQ)

static u32 frequency = 0;
static fe_bandwidth_t bandwidth;

static struct dvb_frontend_info sqc6100_info = {
	.name = "Infineon SQC6100",
	.type = FE_OFDM,
	.frequency_min = 174000000,		/* 174 MHz - 300 MHz */
	.frequency_max = 862000000,		/* 474 MHz - 862 MHz */
	.frequency_stepsize = 166667,	/* 166.666 2/3 */
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

//#if SQC6100_DEBUG
#if 0
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
	*snr = 0;
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

static int sqc6100_set_frequency(u32 frequency)
{
	u32 tw;

	/*
	 * calculate Programmable divider ratio control bits
	 */
	tw = ((frequency * 3) / 50000 + 2165) / 10;

	/*
	 * Set other parameters of SP5668
	 *  prescaler off
	 *  charge pump 0,9 mA
	 *  drive output disable switch 1
	 *  no test mode
	 *
	 * I don't know wether the following  has something to do with
	 * bandwidth switching or band selection
	 */
	if ( frequency <  300000000 )
	{
		tw |= 0x03340000;
	}
	else
	{
		tw |= 0x04340000;
	}
#ifdef SQC6100_DEBUG
	printk(KERN_INFO "sqc6100: tuner-word: 0x%08X\n",tw);
#endif

#ifdef __LITTLE_ENDIAN
	tw = __cpu_to_be32(tw);
#endif

	return dbox2_fp_tuner_write_qpsk((u8 *) &tw,sizeof(tw));
}

static int sqc6100_init(struct dvb_i2c_bus *i2c)
{
	int ret;

	/* fix default values */
	if ((ret = sqc6100_writereg(i2c, FS_MODE1, 0x24)) < 0)
		return ret;
	if ((ret = sqc6100_writereg(i2c, FS_MODE2, 0x4f)) < 0)
		return ret;

	/* recommended but not in bn driver */
	if ((ret = sqc6100_writereg(i2c, AFCACQ_MODE, 0x08)) < 0)
		return ret;
	if ((ret = sqc6100_writereg(i2c, CT_HEAD, 0x02)) < 0)
		return ret;

	/* disable switching output pins to high impedance state via pin OEQ */
	if ((ret = sqc6100_writereg(i2c, OUTINF_MODE, 0x20)) < 0)
		return ret;

	/* enable continual pilot interference detection / cancellation */
	if ((ret = sqc6100_writereg(i2c, AFCTRK_MODE, 0x10)) < 0)
		return ret;

	/* set threshold for interference detection */
	if ((ret = sqc6100_writereg(i2c, AFCTRK_IDC_THR, 0x00)) < 0)
		return ret;

	/* set number of OFDM symbols used for interference detection */
	if ((ret = sqc6100_writereg(i2c, AFCTRK_IDC_PER, 0x03)) < 0)
		return ret;

	/* set estimation period length of BER measurements */
	if ((ret = sqc6100_writereg(i2c, VD_MODE, 0x07)) < 0)
		return ret;

	/* bypass internal anti aliasing filter */
	if ((ret = sqc6100_writereg(i2c, AAF_MODE, 0x10)) < 0)
		return ret;

	/* choose upper sideband */
	if ((ret = sqc6100_writereg(i2c, IQ_MODE, 0x01)) < 0)
		return ret;

	/* set equalization/soft bit compression mode */
	if ((ret = sqc6100_writereg(i2c, ESC_MODE, 0x00)) < 0)
		return ret;

	/* set reed solomon statistic period length */
	if ((ret = sqc6100_writereg(i2c, RS_EST_PERIOD, 0xFF)) < 0)
		return ret;
	return 0;
}

static int sqc6100_set_frontend(struct dvb_i2c_bus *i2c,
				struct dvb_frontend_parameters *p)
{
	int ret;
	u8 gc_mode[3];

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
	const u8 rsu_smp_8mhz[3] =
		{ 0x66, 0x26, 0x65 };
	const u8 rsu_smp_7mhz[3] =
		{ 0x9A, 0x99, 0x73 };
	const u8 frq_sub_8mhz[2] =
		{ 0x1F, 0x0A };
	const u8 frq_sub_7mhz[2] =
		{ 0xDB, 0x08 };

	if ( (p->frequency < sqc6100_info.frequency_min) ||
		 (p->frequency > sqc6100_info.frequency_max) ||
		 ( (p->frequency > 300000000) && (p->frequency < 474000000) ) )
		return -EINVAL;

	/*
	 * xxx_AUTO parameters need interrupts and manual probing
	 * in channel search mode
	 */

	if ((p->inversion < INVERSION_OFF) ||
		(p->inversion > INVERSION_AUTO))
		return -EINVAL;

	if ((p->u.ofdm.bandwidth < BANDWIDTH_8_MHZ) ||
		(p->u.ofdm.bandwidth > BANDWIDTH_7_MHZ))
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

	/*
	 * Stop demodulator (reset)
	 */

	if ((ret = sqc6100_writereg(i2c, GC_RES_TSERR, 0x03)) < 0)
		return ret;

	/*
	 * Program PLL of tuner
	 */

	if ((ret = sqc6100_set_frequency(p->frequency) < 0) )
		return ret;

	frequency = p->frequency;
	bandwidth = p->u.ofdm.bandwidth;
	udelay(100);

	/*
	 * Release reset
	 */

	if ((ret = sqc6100_writereg(i2c, GC_RES_TSERR, 0x02)) < 0)
		return ret;

	udelay(100);

	/*
	 * External bandwidth selection.
	 */

	if ((ret = sqc6100_writereg(i2c, GPIO_MODE1, 0x01)) < 0)
		return ret;

	if ((ret = sqc6100_writereg(i2c, GPIO_DATOUT, (p->u.ofdm.bandwidth == BANDWIDTH_8_MHZ) ? 0x01 : 0x00)) < 0)
		return ret;

	/*
	 * Generic init
	 */

	sqc6100_init(i2c);

	/*
	 * Programming resampling rate factor (bandwidth-switching)
	 * rsu_smp = 2^21 * (7/8) * XTRAL_FRQ / bandwidth
	 * Frontend can only do 8MHz and 7MHz
	 *
	 */

	if ( (ret = sqc6100_write(i2c, RSU_SMP1, (p->u.ofdm.bandwidth == BANDWIDTH_8_MHZ) ?
		  rsu_smp_8mhz : rsu_smp_7mhz, 3)) < 0)
		return ret;

	/*
	 * Programming subcarrier frequency spacing
	 */

	if ((ret = sqc6100_write(i2c, FRQ_SUB1, (p->u.ofdm.bandwidth == BANDWIDTH_8_MHZ) ?
		 frq_sub_8mhz : frq_sub_7mhz, 2)) < 0)
		return ret;

	/*
	 * Programming general mode registers
	 */

	gc_mode[0] = code_rate_hi[p->u.ofdm.code_rate_HP] |
		     code_rate_lo[p->u.ofdm.code_rate_LP] |
		     ofdm_mode[p->u.ofdm.transmission_mode];
	gc_mode[1] = qam_constell[p->u.ofdm.constellation] |
		     guard_interval[p->u.ofdm.guard_interval] |
		     hierarc_mode[p->u.ofdm.hierarchy_information];
	gc_mode[2] = 0x4b;

	if ((ret = sqc6100_write(i2c, GC_MODE1, gc_mode, 3)) < 0)
		return ret;

	/*
	 * Set watchdog time windows
	 */

	if ((ret = sqc6100_writereg(i2c, GC_WD1_SE2, 0x16)) < 0)
		return ret;

	if ((ret = sqc6100_writereg(i2c, GC_WD1_CH2, 0x1C)) < 0)
		return ret;

	/*
	 * Start demodulator operation
	 */

	if ((ret = sqc6100_writereg(i2c, GC_START, 0x00)) < 0)
		return ret;

	if ((ret = sqc6100_writereg(i2c, GC_START, 0x02)) < 0)
		return ret;

	if ((ret = sqc6100_writereg(i2c, GC_START, 0x00)) < 0)
		return ret;

	return 0;
}

static int sqc6100_get_frontend(struct dvb_i2c_bus *i2c,
				struct dvb_frontend_parameters *p)
{
	u8 tps[5];
	u8 intr_status7;
	u8 val;
	int ret;
	static const fe_code_rate_t tps_fec2dvb[5] =
	{
		FEC_1_2,
		FEC_2_3,
		FEC_3_4,
		FEC_5_6,
		FEC_7_8
	};
	static const fe_modulation_t tps_constellation2dvb[3] =
	{
		QPSK,
		QAM_16,
		QAM_64
	};

	if ((ret = sqc6100_readreg(i2c, INTR_STATUS7, &intr_status7)) < 0)
		return ret;

	if ( !(intr_status7 & 0x02) )
		return -EINVAL;

	if ( (ret = sqc6100_read(i2c,TPS_SYNC1,tps,sizeof(tps))) < 0)
		return ret;

	if ( (((tps[0] != 0xEE) || (tps[1] != 0x35)) &&
		  ((tps[0] != 0x11) || (tps[1] != 0xCA))) ||
		  ((tps[2] & 0x3F) != 0x17) ||
		  (tps[3] & 0x10) ||
		  (tps[4] & 0x40) )
		return -EINVAL;

	p->frequency = frequency;
	p->inversion = INVERSION_OFF;
	p->u.ofdm.bandwidth = bandwidth;
	if ( (val = (tps[3] & 0xE0) >> 5) > 4)
		return -EINVAL;
	p->u.ofdm.code_rate_HP = tps_fec2dvb[val];
	if ( (val = tps[4] & 0x07) > 4)
		return -EINVAL;
	p->u.ofdm.code_rate_LP = tps_fec2dvb[val];
	if ( (val = tps[3] & 0x03) > 2)
		return -EINVAL;
	p->u.ofdm.constellation = tps_constellation2dvb[val];
	p->u.ofdm.transmission_mode = (tps[4] & 0x20) >> 5;
	p->u.ofdm.guard_interval = (tps[4] & 0x18) >> 3;
	p->u.ofdm.hierarchy_information = (tps[3] & 0x1C) >> 2;

	return 0;
}

static int sqc6100_reset(struct dvb_i2c_bus *i2c)
{
	int ret;

	/* soft reset */
	if ((ret = sqc6100_writereg(i2c, GC_RES_TSERR, 0x03)) < 0)
		return ret;
	udelay(100);
	if ((ret = sqc6100_writereg(i2c, GC_RES_TSERR, 0x02)) < 0)
		return ret;

	return sqc6100_init(i2c);
}

static int sqc6100_sleep(struct dvb_i2c_bus *i2c)
{
	return sqc6100_reset(i2c);		// This stops demod operation
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

#ifdef SQC6100_PROC_INTERFACE

static int sqc6100_proc_read(char *buf, char **start, off_t offset, int len, int *eof, void *i2c)
{
	u8 val = 0;
	int nr;
	unsigned char bit;
	unsigned count;
	unsigned bit_count;
	const unsigned char *it[8][8] =
	{
		{
			"VD_OUT_LOCK",
			"VD_IN_LOCK",
			"FS_OUT_LOCK",
			"FS_IN_LOCK",
			"RS_BYTERR",
			"RS_FRMRCV",
			"RS_FRMERR",
			"RS_BITERR"
		},
		{
			"reserved",
			"FS_2ND_OUT_LOCK",
			"FALLBACK_ACTIVE",
			"SEARCH_OK",
			"CHANGE_OK",
			"SEARCH_FAILED",
			"CHANGE_FAILED",
			"FIFO_OVERFLOW"
		},
		{
			"CTACQ_START",
			"AGC_START",
			"IFFT_LIMIT",
			"reserved",
			"reserved",
			"FFT_LIMIT",
			"TPS_RESYNC",
			"TPS_CHANGE"
		},
		{
			"reserved",
			"SYMBOL_UPDATE",
			"FTACQ_START",
			"POSTFFT_START",
			"FTTRK_START",
			"TPSACQ_START",
			"FFT_START",
			"AFCACQ_START"
		},
		{
			"AFCTRK_START",
			"FFT_FFT_READY",
			"FFT_AFC_READY",
			"FFT_CT_READY",
			"reserved",
			"reserved",
			"SD_TRK_START",
			"SD_AGC_READY"
		},
		{
			"CE_IDY_1ST_RDY",
			"CE_IFFT_START",
			"IDC_START",
			"AFC_PREREADY",
			"CT_PREREADY",
			"CE_START",
			"IFFT_START",
			"TPS_START"
		},
		{
			"CT_WIN_READY",
			"POSTACQ_READY",
			"PREACQ_READY",
			"CTAFC_CY_READY",
			"IFFT_FT_READY",
			"IFFT_READY",
			"TPS_SYNC_READY",
			"TPS_DAT_READY"
		},
		{
			"RS_FRM_START",
			"DEINT_SY_START",
			"FS_SYNC_MISS",
			"reserved",
			"reserved",
			"AFC_REQ_START",
			"AFC_SMP_READY",
			"AFC_FRQ_READY"
		}
	};

	nr = sprintf(buf,"Status of SQC6100 ofdm demodulator:\n");
	sqc6100_readreg(i2c, AGC_GAIN, &val);
	nr += sprintf(buf + nr,"AGC_GAIN: 0x%02X\n",val);
	sqc6100_readreg(i2c, SD_STATUS, &val);
	nr += sprintf(buf + nr,"SD_STATUS: 0x%02X\n",val);
	sqc6100_readreg(i2c, SD_MEAN, &val);
	nr += sprintf(buf + nr,"SD_MEAN: 0x%02X\n",val);
	sqc6100_readreg(i2c, SD_MAGAV, &val);
	nr += sprintf(buf + nr,"SD_MAGAV: 0x%02X\n",val);
	sqc6100_readreg(i2c, TPS_SYNC1, &val);
	nr += sprintf(buf + nr,"TPS_SYNC1: 0x%02X\n",val);
	sqc6100_readreg(i2c, TPS_SYNC2, &val);
	nr += sprintf(buf + nr,"TPS_SYNC2: 0x%02X\n",val);
	sqc6100_readreg(i2c, TPS_PARAM1, &val);
	nr += sprintf(buf + nr,"TPS_PARAM1: 0x%02X\n",val);
	sqc6100_readreg(i2c, TPS_PARAM2, &val);
	nr += sprintf(buf + nr,"TPS_PARAM2: 0x%02X\n",val);
	sqc6100_readreg(i2c, TPS_PARAM3, &val);
	nr += sprintf(buf + nr,"TPS_PARAM3: 0x%02X\n",val);
	sqc6100_readreg(i2c, TPS_PARITY1, &val);
	nr += sprintf(buf + nr,"TPS_PARITY1: 0x%02X\n",val);
	sqc6100_readreg(i2c, TPS_PARITY2, &val);
	nr += sprintf(buf + nr,"TPS_PARITY2: 0x%02X\n",val);
	sqc6100_readreg(i2c, TPS_PARITY2, &val);
	nr += sprintf(buf + nr,"TPS_PARITY2: 0x%02X\n",val);
	sqc6100_readreg(i2c, AFCACQ_STATUS, &val);
	nr += sprintf(buf + nr,"AFCACQ_STATUS: 0x%02X\n",val);
	sqc6100_readreg(i2c, AFCACQ_RASTER, &val);
	nr += sprintf(buf + nr,"AFCACQ_RASTER: 0x%02X\n",val);
	sqc6100_readreg(i2c, AFCTRK_STATUS, &val);
	nr += sprintf(buf + nr,"AFCTRK_STATUS: 0x%02X\n",val);
	sqc6100_readreg(i2c, VD_STATUS, &val);
	nr += sprintf(buf + nr,"VD_STATUS: 0x%02X\n",val);
	sqc6100_readreg(i2c, CT_STATUS, &val);
	nr += sprintf(buf + nr,"CT_STATUS: 0x%02X\n",val);
	nr += sprintf(buf + nr,"Stati:\n");
	for (count = 0; count < 8; count++)
	{
		sqc6100_readreg(i2c,INTR_STATUS1 + count,&val);
		bit = 0x80;
		bit_count = 0;
		while (bit_count < 8)
		{
			if (val & bit)
			{
				nr += sprintf(buf + nr,"(%d/%d) %s,",count+1,7 - bit_count,it[count][bit_count]);
			}
			bit_count++;
			bit = bit >> 1;
		}
		if (val)
		{
			nr += sprintf(buf + nr - 1,"\n") - 1;
		}
	}

	return nr;
}

static int sqc6100_register_proc(struct dvb_i2c_bus *i2c)
{
	struct proc_dir_entry *proc_bus_sqc6100;

	if (!proc_bus)
	{
		return -ENOENT;
	}

	if ( (proc_bus_sqc6100 = create_proc_read_entry("sqc6100",0,proc_bus,&sqc6100_proc_read,i2c)) == NULL)
	{
		printk(KERN_ERR "Cannot create /proc/bus/sqc6100\n");
		return -ENOENT;
	}

	sqc6100_proc_registered = 1;

	proc_bus_sqc6100->owner = THIS_MODULE;

	return 0;
}

static void sqc6100_deregister_proc(void)
{
	if (sqc6100_proc_registered)
		remove_proc_entry("sqc6100",proc_bus);

	sqc6100_proc_registered = 0;
}

#endif

static int sqc6100_attach(struct dvb_i2c_bus *i2c, void **data)
{
	int ret;
	u8 gc_res_tserr;

	if ((ret = sqc6100_readreg(i2c, GC_RES_TSERR, &gc_res_tserr)) < 0)
		return ret;

	if ((gc_res_tserr & 0xe0) != 0)
		return -ENODEV;

#ifdef SQC6100_PROC_INTERFACE
	sqc6100_register_proc(i2c);
#endif

	return dvb_register_frontend(sqc6100_ioctl, i2c, NULL, &sqc6100_info);
}

static void sqc6100_detach(struct dvb_i2c_bus *i2c, void *data)
{
#ifdef SQC6100_PROC_INTERFACE
	sqc6100_deregister_proc();
#endif

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

MODULE_AUTHOR("Andreas Oberritter <obi@saftware.de>, Wolfram Joost <dbox2@frokaschwei.de>");
MODULE_DESCRIPTION("Infineon SQC6100 DVB-T Frontend Driver");
MODULE_LICENSE("GPL");
