/*
 * $Id: dbox2_fp_napi.c,v 1.4 2003/01/19 19:26:32 obi Exp $
 *
 * Copyright (C) 2002 by Andreas Oberritter <obi@tuxbox.org>
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


static int demod;


static int
dbox2_fp_napi_before_ioctl (struct dvb_frontend *frontend, unsigned int cmd, void *arg)
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
		dbox2_fp_sec_enable_high_voltage(((int) arg) ? 1 : 0);
		break;

	case FE_SET_FRONTEND:
		if (demod != DBOX_DEMOD_VES1993) {

			u32 div;
			u8 buf[4];

			switch (demod) {
			case DBOX_DEMOD_VES1820:
			{
				/*
				 * mitel sp5659
				 * http://assets.zarlink.com/products/datasheets/zarlink_SP5659_MAY_02.pdf
				 */
				
				div = (((struct dvb_frontend_parameters *) arg)->frequency + 36125000) / 125000;
				buf[0] = (div >> 8) & 0x7f;
				buf[1] = div & 0xff;
				buf[2] = 0x80 | (((div >> 15) & 0x03) << 6) | 0x04;
				buf[3] = div > 4017 ? 0x04 : div < 2737 ? 0x02 : 0x01;
				break;
			}

			case DBOX_DEMOD_VES1893:
			{
				/*
				 * mitel sp5668
				 * http://assets.zarlink.com/products/datasheets/zarlink_SP5668_JAN_01.pdf
				 * 
				 * [31:27] (= 0)
				 * [26:24] port control bits (= 0)
				 * [23:23] test mode enable (= 0)
				 * [22:22] drive output disable switch (= 0)
				 * [21:21] charge pump current select (= 1)
				 * [20:18] reference divider ratio
				 * [17:17] prescaler enable
				 * [16:00] divider ratio
				 */

				static const u32 ratio[] = { 2000000, 1000000, 500000, 250000, 125000, 62500, 31250, 15625 };
				u32 freq = (((struct dvb_frontend_parameters *) arg)->frequency + 479500) * 1000;
				u8 sel, pe;

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
				buf[0] = 0x00;
				/* charge pump, ref div ratio, prescaler, div[16] */
				buf[1] = 0x20 | ((sel + pe) << 2) | (pe << 1) | ((div >> 16) & 0x01);
				/* div[15:8] */
				buf[2] = (div >> 8) & 0xff;
				/* div[7:0] */
				buf[3] = div & 0xff;
				break;
			}

			default:
				break;
			}

			if (dbox2_fp_tuner_write(buf, sizeof(buf)))
				return 0;
		}

		/*
		 * looks strange, but is a good thing[tm]:
		 * returning zero will skip next ioctl handler,
		 * but symbolrate etc. still has to be set by
		 * the demodulator driver after tuning.
		 */
		return -EOPNOTSUPP;

	case FE_SLEEP:
		/* TODO: enable lnb loop through */
		return -EOPNOTSUPP;

	default:
		return -EOPNOTSUPP;
	}

	return 0;
}


int __init
dbox2_fp_napi_init(void)
{
	demod = fp_get_info()->demod;

	switch (demod) {
	case DBOX_DEMOD_VES1820:
	case DBOX_DEMOD_VES1893:
	case DBOX_DEMOD_VES1993:
		return dvb_add_frontend_ioctls(avia_napi_get_adapter(), dbox2_fp_napi_before_ioctl, NULL, NULL);
	default:
		return 0;
	}
}


void __exit
dbox2_fp_napi_exit(void)
{
	switch (demod) {
	case DBOX_DEMOD_VES1820:
	case DBOX_DEMOD_VES1893:
	case DBOX_DEMOD_VES1993:
		dvb_remove_frontend_ioctls(avia_napi_get_adapter(), dbox2_fp_napi_before_ioctl, NULL);
		break;
	default:
		break;
	}
}


module_init(dbox2_fp_napi_init);
module_exit(dbox2_fp_napi_exit);

MODULE_AUTHOR("Andreas Oberritter <obi@saftware.de>");
MODULE_DESCRIPTION("dbox2 fp dvb api driver");
MODULE_LICENSE("GPL");

