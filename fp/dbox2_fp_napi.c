/*
 * $Id: dbox2_fp_napi.c,v 1.10 2003/05/26 03:21:03 obi Exp $
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/dvb/frontend.h>

#include <avia/avia_napi.h>
#include <dbox/dbox2_fp_core.h>
#include <dbox/dbox2_fp_sec.h>
#include <dbox/dbox2_fp_tuner.h>
#include <dvb-core/dvb_frontend.h>

static int
dbox2_fp_napi_before_ioctl (struct dvb_frontend *frontend, unsigned int cmd, void *arg)
{
	static int demod;

	if (!demod)
	{
		struct dvb_frontend_info info;
		frontend->ioctl (frontend, FE_GET_INFO, &info);
		if (!strncmp (info.name, "VES1820", 7))
			demod = 1;
		else if (!strncmp (info.name, "VES1893", 7))
			demod = 2;
		else if (!strncmp (info.name, "VES1993", 7))
			demod = 3;
		else
			demod = -1;
	}

	if (demod == -1)
		return -EOPNOTSUPP;

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

	case FE_SET_FRONTEND:
	{
		u32 freq = ((struct dvb_frontend_parameters *) arg)->frequency;
		u32 div;
		u8 buf[4];

		switch (demod) {
			case 1: /* VES1820 */
			{
				/*
				 * mitel sp5659
				 * http://assets.zarlink.com/products/datasheets/zarlink_SP5659_MAY_02.pdf
				 */

				div = (freq + 36125000 + 31250) / 62500;
				buf[0] = (div >> 8) & 0x7f;
				buf[1] = div & 0xff;
				buf[2] = 0x85 | ((div >> 10) & 0x60);
				buf[3] = (freq < 174000000 ? 0x02 :
					  freq < 470000000 ? 0x01 : 0x04);

				if (dbox2_fp_tuner_write_qam(buf, sizeof(buf)))
					return 0;
				break;
			}

			case 2:	/* VES1893 */
			{
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

				static const u32 ratio[] = { 2000000, 1000000, 500000, 250000, 125000, 62500, 31250, 15625 };
				u8 sel, pe;

				freq = (freq + 479500) * 1000;

				if (freq >= 2000000000)
					pe = 1;
				else
					pe = 0;

				for (sel = 7; sel > 0; sel--)
					if ((freq / ratio[sel]) < 0x3fff)
						break;

				div = freq / ratio[sel];

				printk("freq: %u, ratio: %u, div: %x, pe: %hu\n", freq, ratio[sel], div, pe);

				/* port control */
				buf[0] = 0x01;
				/* charge pump, ref div ratio, prescaler, div[16] */
				buf[1] = 0x20 | ((sel + pe) << 2) | (pe << 1) | ((div >> 16) & 0x01);
				/* div[15:8] */
				buf[2] = (div >> 8) & 0xff;
				/* div[7:0] */
				buf[3] = div & 0xff;

				if (dbox2_fp_tuner_write_qpsk(buf, sizeof(buf)))
					return 0;
				break;
			}

			default:
				break;
		}

		/*
		 * looks strange, but is a good thing[tm]:
		 * returning zero will skip next ioctl handler,
		 * but symbolrate etc. still has to be set by
		 * the demodulator driver after tuning.
		 */
		return -EOPNOTSUPP;
	}

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


int __init
dbox2_fp_napi_init(void)
{
	return dvb_add_frontend_ioctls(avia_napi_get_adapter(), dbox2_fp_napi_before_ioctl, NULL, NULL);
}

void __exit
dbox2_fp_napi_exit(void)
{
	dvb_remove_frontend_ioctls(avia_napi_get_adapter(), dbox2_fp_napi_before_ioctl, NULL);
}


module_init(dbox2_fp_napi_init);
module_exit(dbox2_fp_napi_exit);

MODULE_AUTHOR("Andreas Oberritter <obi@saftware.de>");
MODULE_DESCRIPTION("dbox2 fp dvb api driver");
MODULE_LICENSE("GPL");

