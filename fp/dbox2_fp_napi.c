/*
 * $Id: dbox2_fp_napi.c,v 1.12 2003/09/30 05:45:38 obi Exp $
 *
 * Copyright (C) 2002-2003 Andreas Oberritter <obi@tuxbox.org>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/dvb/frontend.h>

#include <avia/avia_napi.h>
#include <dbox/dbox2_fp_core.h>
#include <dbox/dbox2_fp_sec.h>
#include <dbox/dbox2_fp_tuner.h>
#include <dvb-core/dvb_frontend.h>

enum {
	DEMOD_VES1820 = 0,
	DEMOD_VES1893 = 1,
	DEMOD_VES1993 = 2
};

static struct dvb_adapter *dvb_adapter;

static inline
u32 unsigned_round_div(u32 n, u32 d)
{
	return (n + (d / 2)) / d;
}

/*
 * mitel sp5659
 * http://assets.zarlink.com/products/datasheets/zarlink_SP5659_MAY_02.pdf
 */

static
int sp5659_set_tv_freq(const struct dvb_frontend_parameters *p)
{
	u32 freq = p->frequency;
	u32 div;
	u8 buf[4];

	div = unsigned_round_div(freq + 36125000, 62500);

	buf[0] = (div >> 8) & 0x7f;
	buf[1] = div & 0xff;
	buf[2] = 0x85 | ((div >> 10) & 0x60);
	buf[3] = (freq < 174000000 ? 0x02 :
		  freq < 470000000 ? 0x01 : 0x04);

	if (dbox2_fp_tuner_write_qam(buf, sizeof(buf)) < 0)
		return -EREMOTEIO;

	return -EOPNOTSUPP;
}

/*
 * mitel sp5668
 * http://assets.zarlink.com/products/datasheets/zarlink_SP5668_JAN_01.pdf
 * 
 * [31:27] (= 0)
 * [26:24] port control bits (= 1)
 * [23:23] test mode enable (= 0)
 * [22:22] drive output disable switch (= 0)
 * [21:21] charge pump current select (= 1)
 * [20:18] reference divider ratio
 * [17:17] prescaler enable
 * [16:00] divider ratio
 */

static
int sp5668_set_tv_freq(const struct dvb_frontend_parameters *p)
{
	static const u32 sp5668_ratios[] =
		{ 2000000, 1000000, 500000, 250000, 125000, 62500, 31250, 15625 };

	u32 freq = (p->frequency + 479500) * 1000;
	u32 div;
	u8 buf[4];

	int sel, pe;

	if (p->frequency > 3815467) {
		printk(KERN_ERR "frequency would lead to an integer overflow and is too high anyway\n");
		return -EINVAL;
	}

	if (freq >= 2000000000)
		pe = 1;
	else
		pe = 0;

	/*
	 * even the highest possible frequency which fits
	 * into 32 bit divided by the highest ratio
	 * is as low as 0x863, so no check for a negative
	 * sel value is needed below.
	 */
	for (sel = 7; sel >= 0; sel--)
		if ((div = unsigned_round_div(freq, sp5668_ratios[sel])) <= 0x3fff)
			break;

	printk(KERN_DEBUG "freq=%u ratio=%u div=%x pe=%hu\n",
			freq, sp5668_ratios[sel], div, pe);

	/* port control */
	buf[0] = 0x01;
	/* charge pump, ref div ratio, prescaler, div[16] */
	buf[1] = 0x20 | ((sel + pe) << 2) | (pe << 1) | ((div >> 16) & 0x01);
	/* div[15:8] */
	buf[2] = (div >> 8) & 0xff;
	/* div[7:0] */
	buf[3] = div & 0xff;

	if (dbox2_fp_tuner_write_qpsk(buf, sizeof(buf)) < 0)
		return -EREMOTEIO;

	return -EOPNOTSUPP;
}

static
int dbox2_fp_napi_sec_ioctl(struct dvb_frontend *frontend, unsigned int cmd, void *arg)
{
	switch (cmd) {
	case FE_DISEQC_SEND_MASTER_CMD:
	{
		struct dvb_diseqc_master_cmd *cmd = arg;
		dbox2_fp_sec_diseqc_cmd(cmd->msg, cmd->msg_len);
		break;
	}

	case FE_DISEQC_SEND_BURST:
		switch ((fe_sec_mini_cmd_t) arg) {
		case SEC_MINI_A:
			dbox2_fp_sec_diseqc_cmd("\x00\x00\x00\x00", 4);
			break;
		case SEC_MINI_B:
			dbox2_fp_sec_diseqc_cmd("\xff", 1);
			break;
		default:
			return -EINVAL;
		}
		break;

	case FE_SET_TONE:
		switch ((fe_sec_tone_mode_t) arg) {
		case SEC_TONE_OFF:
			dbox2_fp_sec_set_tone(0);
			break;
		case SEC_TONE_ON:
			dbox2_fp_sec_set_tone(1);
			break;
		default:
			return -EINVAL;
		}
		break;

	case FE_SET_VOLTAGE:
		switch ((fe_sec_voltage_t) arg) {
		case SEC_VOLTAGE_13:
			dbox2_fp_sec_set_voltage(0);
			break;
		case SEC_VOLTAGE_18:
			dbox2_fp_sec_set_voltage(1);
			break;
		default:
			return -EINVAL;
		}
		break;

	case FE_ENABLE_HIGH_LNB_VOLTAGE:
		dbox2_fp_sec_set_high_voltage(((int) arg) ? 1 : 0);
		break;

	case FE_SLEEP:
		dbox2_fp_sec_set_power(0);
		return -EOPNOTSUPP;

	case FE_INIT:
		dbox2_fp_sec_set_power(1);
		return -EOPNOTSUPP;

	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static
int dbox2_fp_napi_before_ioctl(struct dvb_frontend *frontend, unsigned int cmd, void *arg)
{
	static int demod = -1;

	if (demod < 0) {
		struct dvb_frontend_info info;

		frontend->ioctl(frontend, FE_GET_INFO, &info);

		if (strstr(info.name, "VES1820"))
			demod = DEMOD_VES1820;
		else if (strstr(info.name, "VES1893"))
			demod = DEMOD_VES1893;
		else if (strstr(info.name, "VES1993"))
			demod = DEMOD_VES1993;
		else {
			/* ioctl handler is not needed for other frontend types, so unregister it */
			dvb_remove_frontend_ioctls(dvb_adapter, dbox2_fp_napi_before_ioctl, NULL);
			dvb_adapter = NULL;
			return -EOPNOTSUPP;
		}
	}

	if (cmd == FE_SET_FRONTEND) {
		if (demod == DEMOD_VES1820)
			return sp5659_set_tv_freq(arg);
		else if (demod == DEMOD_VES1893)
			return sp5668_set_tv_freq(arg);
	}
	else if ((demod == DEMOD_VES1893) || (demod == DEMOD_VES1993)) {
		return dbox2_fp_napi_sec_ioctl(frontend, cmd, arg);
	}

	/*
	 * looks strange, but is a good thing[tm]:
	 * returning zero will skip next ioctl handler,
	 * but symbolrate etc. still has to be set by
	 * the demodulator driver after tuning.
	 */

	return -EOPNOTSUPP;
}

int __init dbox2_fp_napi_init(void)
{
	dvb_adapter = avia_napi_get_adapter();

	if (!dvb_adapter)
		return -EINVAL;

	return dvb_add_frontend_ioctls(dvb_adapter, dbox2_fp_napi_before_ioctl, NULL, NULL);
}

void __exit dbox2_fp_napi_exit(void)
{
	if (dvb_adapter)
		dvb_remove_frontend_ioctls(dvb_adapter, dbox2_fp_napi_before_ioctl, NULL);
}

module_init(dbox2_fp_napi_init);
module_exit(dbox2_fp_napi_exit);

MODULE_AUTHOR("Andreas Oberritter <obi@saftware.de>");
MODULE_DESCRIPTION("dbox2 fp dvb api driver");
MODULE_LICENSE("GPL");
