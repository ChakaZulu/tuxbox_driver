
/*
 * $Id: dummyfe.c,v 1.1 2002/10/22 19:43:44 Jolt Exp $
 *
 * Dummy Frontend Driver 
 *
 * Homepage: http://dbox2.elxsi.de
 *
 * Copyright (C) 2002 Florian Schirmer <jolt@tuxbox.org>
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

#include <asm/bitops.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include "../dvb-core/dvb_frontend.h"

//#define USE_DVB_I2C

static struct dvb_frontend_info dummyfe_info = {

	.name = "Dummy FE",
	.type = FE_QAM,
	.frequency_min = 48250000,
	.frequency_max = 863250000,
	.frequency_stepsize = 62500,
/*	.frequency_tolerance = *///FIXME: 12% of SR
	.symbol_rate_min = 0,	//FIXME
	.symbol_rate_max = 9360000,	//FIME
	.symbol_rate_tolerance = 4000,
	.notifier_delay = 0,
	.caps = FE_CAN_INVERSION_AUTO |
		FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
		FE_CAN_FEC_4_5 | FE_CAN_FEC_5_6 | FE_CAN_FEC_6_7 |
		FE_CAN_FEC_7_8 | FE_CAN_FEC_8_9 | FE_CAN_FEC_AUTO |
		FE_CAN_QAM_16 | FE_CAN_QAM_32 | FE_CAN_QAM_64 | FE_CAN_QAM_128 | FE_CAN_QAM_256 |
		/* FE_CAN_QAM_512 | FE_CAN_QAM_1024 |  */
		FE_CAN_MUTE_TS,

};

static int dummyfe_ioctl(struct dvb_frontend *fe, unsigned int cmd, void *arg)
{

	switch (cmd) {

		case FE_GET_INFO:

			printk("dummyfe: FE_GET_INFO\n");

			memcpy(arg, &dummyfe_info, sizeof(struct dvb_frontend_info));

			break;

		case FE_READ_STATUS:
			{

				fe_status_t *status = (fe_status_t *)arg;

				printk("dummyfe: FE_READ_STATUS\n");

				*status = FE_HAS_SIGNAL | FE_HAS_VITERBI | FE_HAS_SYNC | FE_HAS_CARRIER | FE_HAS_LOCK;

				break;
				
			}

		case FE_READ_BER:
			{
			
				u32 *ber = (u32 *)arg;

				printk("dummyfe: FE_READ_STATUS\n");

				*ber = 0;
				
				break;
			}

		case FE_READ_SIGNAL_STRENGTH:
			{

				printk("dummyfe: FE_READ_SIGNAL_STRENGTH\n");

				*(s32 *) arg = 0xFF;
				
				break;
			}

		case FE_READ_SNR:

			printk("dummyfe: FE_READ_SNR\n");

			*(s32 *) arg = 0;

			break;

		case FE_READ_UNCORRECTED_BLOCKS:

			printk("dummyfe: FE_READ_UNCORRECTED_BLOCKS\n");

			*(u32 *) arg = 0;
			
			break;

		case FE_SET_FRONTEND:
		
			{
			
				struct dvb_frontend_parameters *p = (struct dvb_frontend_parameters *)arg;
				
				printk("dummyfe: FE_SET_FRONTEND\n");

				printk("            frequency->%d\n", p->frequency);
				printk("            symbol_rate->%d\n", p->u.qam.symbol_rate);
				printk("            inversion->%d\n", p->inversion);

				break;
				
			}

		case FE_GET_FRONTEND:

			printk("dummyfe: FE_GET_FRONTEND\n");

			break;

		case FE_SLEEP:

			printk("dummyfe: FE_SLEEP\n");
			
			break;

		case FE_INIT:

			printk("dummyfe: FE_INIT\n");

			break;

		case FE_RESET:

			printk("dummyfe: FE_RESET\n");
			
			break;

		default:
		
			printk("dummyfe: unknown IOCTL (0x%X)\n", cmd);

			return -EINVAL;
			
	}

	return 0;

}

static int dummyfe_attach(struct dvb_i2c_bus *i2c)
{

	printk("dummyfe: attach\n");

	dvb_register_frontend(dummyfe_ioctl, i2c, NULL, &dummyfe_info);

	return 0;

}

static
void dummyfe_detach(struct dvb_i2c_bus *i2c)
{

	printk("dummyfe: detach\n");

	dvb_unregister_frontend(dummyfe_ioctl, i2c);

}

static
int __init dummyfe_init(void)
{

	printk("$Id: dummyfe.c,v 1.1 2002/10/22 19:43:44 Jolt Exp $\n");

#ifdef USE_DVB_I2C
	return dvb_register_i2c_device(THIS_MODULE, dummyfe_attach, dummyfe_detach);
#else
#endif

}

static
void __exit dummyfe_exit(void)
{

#ifdef USE_DVB_I2C
	dvb_unregister_i2c_device(dummyfe_attach);
#else
#endif

}

#ifdef MODULE
module_init(dummyfe_init);
module_exit(dummyfe_exit);
MODULE_DESCRIPTION("dummyfe dvb-c frontend driver");
MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#endif
