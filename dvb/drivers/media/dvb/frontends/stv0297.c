/*
    Driver for STV0297/TSA5512 based DVB QAM frontends

    Copyright (C) 2003-2004 Dennis Noermann <dennis.noermann@noernet.de>

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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/delay.h>

#include "dvb_frontend.h"
#include "dvb_functions.h"

#if 0
#define dprintk(x...) printk(x)
#else
#define dprintk(x...)
#endif

#define STV0297_CLOCK   28900
#define TunerZF         36150

static struct dvb_frontend_info stv0297_info = {
	.name			= "STV0297/TSA5512 based DVB-C frontend",
	.type			= FE_QAM,
	.frequency_min		= 64000000,
	.frequency_max		= 1300000000,
	.frequency_stepsize	= 62500,
	.symbol_rate_min	= 870000,
	.symbol_rate_max	= 11700000,
#if 0
	.frequency_tolerance	= ???,
	.symbol_rate_tolerance	= ???,
	.notifier_delay		= ???,
#endif
	.caps = FE_CAN_QAM_16 | FE_CAN_QAM_32 | FE_CAN_QAM_64 |
                FE_CAN_QAM_128 | FE_CAN_QAM_256 |
		FE_CAN_FEC_AUTO | FE_CAN_INVERSION_AUTO |
		FE_CAN_CLEAN_SETUP | FE_CAN_RECOVER 
};


static u8 init_tab [] = {
	0x80, 0x01, /* soft_reset */
	0x80, 0x00, /* cleared soft_reset */
	0x81, 0x01, /* deinterleaver descrambler reset */
	0x81, 0x00, /* cleared deinterleaver descrambler reset */
	0x83, 0x10, /* the Reed-Solomon block reset*/ 
	0x83, 0x00, /* cleared the Reed-Solomon block reset */
	0x84, 0x2b, /* clears the equalizer and also reinitializes the Reg. 00through 04. */
	0x84, 0x2a, /* cleares it .. */
	0x03, 0x00,
	0x25, 0x88,
	0x30, 0x97, 
	0x31, 0x4C,
	0x32, 0xFF,
	0x33, 0x55,
	0x34, 0x00,
	0x35, 0x65,
	0x36, 0x80,
	0x40, 0x1C, 
	0x42, 0x3C, 
	0x43, 0x00,
	0x52, 0x28,
	0x5A, 0x1E,
	0x5B, 0x05,
	0x62, 0x06, 
	0x6A, 0x02, 
	0x70, 0xFF,
	0x71, 0x84,
	0x83, 0x10, 
	0x84, 0x25,
	0x85, 0x00,
	0x86, 0x78,
	0x87, 0x73,
	0x88, 0x08,
	0x89, 0x00,
	0x90, 0x05,
	0xA0, 0x00,
	0xB0, 0x91,
	0xB1, 0x0B, 
	0xC0, 0x4B,
	0xC1, 0x01,
	0xC2, 0x00,
	0xDE, 0x00,
	0xDF, 0x03,
	0x87, 0x73
};


static int stv0297_writereg (struct dvb_i2c_bus *i2c, u8 reg, u8 data)
{
	int ret;
	u8 buf [] = { reg, data };
	struct i2c_msg msg = { .addr = 0x1C, .flags = 0, .buf = buf, .len = 2 };

	ret = i2c->xfer (i2c, &msg, 1);

	if (ret != 1) 
		dprintk("%s: writereg error (reg == 0x%02x, val == 0x%02x, "
			"ret == %i)\n", __FUNCTION__, reg, data, ret);

	return (ret != 1) ? -1 : 0;
}

static int stv0297_writeregs (struct dvb_i2c_bus *i2c, u8 *data, int len)
{
        int ret;
        struct i2c_msg msg = { .addr = 0x1C , .flags = 0, .buf = data, .len = len };
        
        ret = i2c->xfer (i2c, &msg, 1);
                
        if (ret != 1)
                dprintk("%s: writeregs error\n ", __FUNCTION__);

        return (ret != 1) ? -1 : 0;
}

static u8 stv0297_readreg (struct dvb_i2c_bus *i2c, u8 reg)
{

        int ret;
        u8 b0[] = { reg };
        u8 b1[] = { 0 };
        struct i2c_msg msg1[] = { {.addr =  0x38 >> 1, .flags =  0, .buf =  b0, .len = 1} };
        struct i2c_msg msg2[] = { {.addr =  0x38 >> 1, .flags = I2C_M_RD, .buf =  b1, .len = 1} };

        ret = i2c->xfer(i2c, msg1, 1);

        if (ret != 1)
                dprintk("%s: readreg error (ret == %i)\n", __FUNCTION__, ret);

	ret = i2c->xfer(i2c, msg2, 1);

        if (ret != 1)
                dprintk("%s: readreg error (ret == %i)\n", __FUNCTION__, ret);

	//dprintk("readreg: reg == 0x%02x, val == 0x%02x\n", reg, b1[0]); 
        return b1[0];
}



static int stv0297_readregs (struct dvb_i2c_bus *i2c, u8 reg1, u8 *b, u8 len)
{
        int ret;
        struct i2c_msg msg1[] = { { .addr = 0x38 >> 1, .flags = 0, .buf = &reg1, .len = 1 } };
        struct i2c_msg msg2[] = { { .addr = 0x38 >> 1, .flags = I2C_M_RD, .buf = b, .len = len } };

        ret = i2c->xfer (i2c, msg1, 1);

        if (ret != 1)
                dprintk("%s: readreg error (ret == %i)\n", __FUNCTION__, ret);

        ret = i2c->xfer(i2c, msg2, 1);
        
        if (ret != 1)
                dprintk("%s: readreg error (ret == %i)\n", __FUNCTION__, ret);

        return ret == 1 ? 0 : ret;
}


static int pll_write (struct dvb_i2c_bus *i2c, u8 *data, int len)
{
	int ret;
	stv0297_writereg (i2c, 0x86, 0xF8);	

	struct i2c_msg msg = { .addr = 0xC0 >> 1, .flags = 0, .buf = data, .len = len };

        ret = i2c->xfer (i2c, &msg, 1);

        if (ret != 1)  
                printk("%s: i/o error (ret == %i)\n", __FUNCTION__, ret);

	return (ret != 1) ? -1 : 0;
}


static int tsa5512_read_status	(struct dvb_i2c_bus *i2c)
{
	int ret;
	u8 stat [] = { 0 };

	stv0297_writereg (i2c, 0x86, 0xF8);

	struct i2c_msg msg [] = {{ .addr = 0xC0 >> 1, .flags = I2C_M_RD, .buf = stat, .len = 1 }};

	ret = i2c->xfer (i2c, msg, 1);

	if (ret != 1)
		dprintk("%s: readreg error (ret == %i)\n", __FUNCTION__, ret);

	return stat[0];
}


static int tsa5512_set_tv_freq	(struct dvb_i2c_bus *i2c, u32 freq)
{
	u8 buf[4];
	u8 pll[] = { 0x8E };
	u32 div, ifreq;
	int i = 0;

        ifreq = 36125000;
 
        div = (freq + ifreq + 31250) / 62500;

	buf[0] = (div >> 8) & 0x7f;
	buf[1] = div & 0xff;
	buf[2] = 0xCE; 

	if (freq < 166000000)
		buf[3] = 0xA2; // 41 Mhz bis 166 Mhz 
	else
		buf[3] = 0x92; // 166 Mhz bis 451 Mhz 
			       // > 451 fix me   

	dprintk("%s:  div = %d 0 == 0x%02x, 1 == 0x%02x 3 == 0x%02x \n",__FUNCTION__,div,buf[0],buf[1],buf[3]);

	pll_write (i2c, buf, sizeof(buf));

	while ( (tsa5512_read_status(i2c) >> 6 == 0) ){
        	dprintk ("%s:  wait for tuner... %x\n", __FUNCTION__,tsa5512_read_status(i2c));
                i++;
		if (i == 4) { dprintk ("%s:  break\n", __FUNCTION__); break; }
	}
        
        pll_write (i2c, pll, sizeof(pll));

	return 0;
}


static int stv0297_init (struct dvb_i2c_bus *i2c)
{
	int i;

	dprintk("stv0297: init chip\n");

	for (i=0; i<sizeof(init_tab); i+=2) {
		stv0297_writereg (i2c, init_tab[i], init_tab[i+1]);
		}

	return 0;
}


static int stv0297_set_symbolrate (struct dvb_i2c_bus *i2c, u32 srate) 
{
/*
	Betanova sniff : 690000 
	stv0297_writereg (i2c, 0x55, 0x4E);
	stv0297_writereg (i2c, 0x56, 0x00);
	stv0297_writereg (i2c, 0x57, 0x1F);
	stv0297_writereg (i2c, 0x58, 0x3D);
*/
long tmp, ExtClk;

        ExtClk = (long)(STV0297_CLOCK) / 4;	/* 1/4 = 2^-2 */
	tmp = 131072L * srate;			/* 131072 = 2^17  */
        tmp = tmp /ExtClk;
        tmp = tmp * 8192L;			/* 8192 = 2^13 */

        stv0297_writereg (i2c, 0x55,(unsigned char)(tmp & 0xFF));  
        stv0297_writereg (i2c, 0x56,(unsigned char)(tmp>> 8));     
        stv0297_writereg (i2c, 0x57,(unsigned char)(tmp>>16));     
        stv0297_writereg (i2c, 0x58,(unsigned char)(tmp>>24));     
 
	return 0;
}

void stv0297_set_sweeprate(struct dvb_i2c_bus *i2c, short _FShift, long _SymbolRate)
{
long  long_tmp;
short FShift ;
unsigned char carrier;
int   RegSymbolRate;

        FShift = _FShift;  /* in mS .. +/- 5,100 S von 6975 S */
        RegSymbolRate = _SymbolRate;     //RegGetSRate() ;  /* in KHz */
        if(RegSymbolRate <= 0) return ;

        long_tmp = (long)FShift * 262144L ;  // 262144 = 2*18
        long_tmp /= RegSymbolRate ;
        long_tmp *= 1024 ;                   // 1024 = 2*10

        if(long_tmp >= 0)
              long_tmp += 500000 ;
        else  long_tmp -= 500000 ;
        long_tmp /= 1000000 ;

        stv0297_writereg (i2c,0x60,(unsigned char)(long_tmp & 0xFF));

        carrier = stv0297_readreg(i2c,0x69) & ~ 0xF0;       
        carrier |= (unsigned char)((long_tmp>>4) & 0xF0);   
        stv0297_writereg (i2c,0x69, carrier);               

        return;
}


void stv0297_set_frequencyoffset(struct dvb_i2c_bus *i2c, long _CarrierOffset)
{
long long_tmp;
unsigned char sweep;

        long_tmp = _CarrierOffset * 26844L ; /* (2**28)/10000 */
        if(long_tmp < 0) long_tmp += 0x10000000 ;
        long_tmp &= 0x0FFFFFFF ;

        stv0297_writereg (i2c,0x66,(unsigned char)(long_tmp & 0xFF));    // iphase0
        stv0297_writereg (i2c,0x67,(unsigned char)(long_tmp>>8));        // iphase1
        stv0297_writereg (i2c,0x68,(unsigned char)(long_tmp>>16));       // iphase2

        sweep = stv0297_readreg(i2c,0x69) & 0xF0;          
        sweep |= ((unsigned char)(long_tmp>>24) &  0x0F);  
        stv0297_writereg (i2c,0x69,sweep);                 

        return;
}


static int stv0297_ioctl (struct dvb_frontend *fe, unsigned int cmd, void *arg)
{
	struct dvb_i2c_bus *i2c = fe->i2c;

	switch (cmd) {
	case FE_GET_INFO: 
		memcpy (arg, &stv0297_info, sizeof(struct dvb_frontend_info));
		break;
	case FE_READ_STATUS: 
	{
		fe_status_t *status = (fe_status_t *) arg;
		u8 tuner = tsa5512_read_status(i2c);		
		u8 sync = stv0297_readreg (i2c, 0xDF);
		
		*status = 0;

		if (sync & 0x80)
			*status |= FE_HAS_SYNC;
		if (tuner & 0x50) 
			*status |= FE_HAS_SIGNAL;

		if (sync & 0x80)
			*status |= FE_HAS_CARRIER; 

		if (sync & 0x80)
			*status |= FE_HAS_VITERBI;

		if (sync & 0x80)
			*status |= FE_HAS_LOCK;

		break;
	}
        case FE_READ_BER:  
	{
		u32 ber;
		u8 BER[3];
		stv0297_writereg (i2c, 0xA0, 0x80);	// Start Counting bit errors for 4096 Bytes 		
		udelay(25000);				// Hopefully got 4096 Bytes 
		stv0297_readregs (i2c, 0xA0, BER, 3);
		udelay(25000);
		ber = (BER[2] << 8 | BER[1]) / ( 8 * 4096);
		*((u32*) arg) = ber;
		break;
	}

	case FE_READ_SIGNAL_STRENGTH:
	{
		u16 strength;
		u8 STRENGTH[2];
		udelay(25000);
		stv0297_readregs (i2c, 0x41, STRENGTH, 2);
		strength = (STRENGTH[1] & 0x03) << 8 | STRENGTH[0];
		*((u16*) arg) = strength;
		break;
	}
        case FE_READ_SNR:
	{
		u16 snr;
		u8 SNR[2];
		udelay(25000);
		stv0297_readregs (i2c, 0x07, SNR, 2);
		snr = SNR[1] << 8 | SNR[0];
		*((u16*) arg) = snr;
		break;
	}
	case FE_READ_UNCORRECTED_BLOCKS: 
        {
                *((u16*) arg) = (stv0297_readreg (i2c, 0xD5) << 8)
                               | stv0297_readreg (i2c, 0xD4);

                break;
        }
        case FE_SET_FRONTEND: 
        {
		struct dvb_frontend_parameters *p = arg;
		u8 buf2[] = { 0x83, 0x10, 0x25, 0x00, 0x78, 0x73 };
		u8 buf3[] = { 0xC0, 0x4B, 0x01, 0x00 };
		int CarrierOffset = -500;
		int SweepRate = 1380;
		int SpectrumInversion = 0;
        	
		if (SpectrumInversion) {
            		SweepRate     = -SweepRate ;
            		CarrierOffset = -CarrierOffset ;
        	}
        	else {
            		SweepRate     = SweepRate ;
            		CarrierOffset = CarrierOffset ;
        	}

		tsa5512_set_tv_freq (i2c, p->frequency);
		
		stv0297_writeregs (i2c, buf2, sizeof(buf2)); 
		stv0297_writereg (i2c, 0x84, 0x24); 
		stv0297_writeregs (i2c, buf3, sizeof(buf3)); 
		stv0297_writereg (i2c, 0x88, 0x08); 
		stv0297_writereg (i2c, 0x90, 0x01); 
		stv0297_writereg (i2c, 0x37, 0x20); 		
		stv0297_writereg (i2c, 0x40, 0x19); 		
		stv0297_writereg (i2c, 0x43, 0x40); 
		stv0297_writereg (i2c, 0x41, 0xE4); 
		stv0297_writereg (i2c, 0x42, 0x3C); 
		stv0297_writereg (i2c, 0x44, 0xFF); 
		stv0297_writereg (i2c, 0x49, 0x04); 
		stv0297_writereg (i2c, 0x4A, 0xFF); 
		stv0297_writereg (i2c, 0x4B, 0xFF); 
		stv0297_writereg (i2c, 0x71, 0x04); 
		stv0297_writereg (i2c, 0x53, 0x08); 
		stv0297_writereg (i2c, 0x5A, 0x3E);  //3E forces the direct path to be immediately 
		stv0297_writereg (i2c, 0x5B, 0x07); 
		stv0297_writereg (i2c, 0x5B, 0x05); 
                
		stv0297_set_symbolrate (i2c, p->u.qam.symbol_rate/1000); 
		stv0297_writereg (i2c, 0x59, 0x08); 
		stv0297_writereg (i2c, 0x61, 0x49); 
		stv0297_writereg (i2c, 0x62, 0x0E); 
		stv0297_writereg (i2c, 0x6A, 0x02); 
		stv0297_writereg (i2c, 0x00, 0x48);  // set qam-64
		 
		stv0297_writereg (i2c, 0x01, 0x58); 
		stv0297_writereg (i2c, 0x82, 0x00); 
		stv0297_writereg (i2c, 0x83, 0x08); 
		stv0297_set_sweeprate(i2c,SweepRate,  p->u.qam.symbol_rate/1000);
		stv0297_writereg (i2c, 0x20, 0x00); 
		stv0297_writereg (i2c, 0x21, 0x40); 
		stv0297_set_frequencyoffset(i2c,CarrierOffset);

		stv0297_writereg (i2c, 0x82, 0x00); 
		stv0297_writereg (i2c, 0x85, 0x04); 
		stv0297_writereg (i2c, 0x43, 0x10); 
		stv0297_writereg (i2c, 0x5A, 0x5E); 

		stv0297_writereg (i2c, 0x6A, 0x03); 
		
		stv0297_writereg (i2c, 0x85, 0x04); 
		stv0297_writereg (i2c, 0x6B, 0x00); 
		stv0297_writereg (i2c, 0x4A, 0xFF); 
		stv0297_writereg (i2c, 0x61, 0x49); 
		stv0297_writereg (i2c, 0x62, 0x0E); 
                stv0297_writereg (i2c, 0xDF, 0x02);
		stv0297_writereg (i2c, 0xDF, 0x01);

                stv0297_writereg (i2c, 0xDF, 0x02);
		stv0297_writereg (i2c, 0xDF, 0x01);
				
		stv0297_writereg (i2c, 0xDF, 0x02); 
		stv0297_writereg (i2c, 0xDF, 0x01); 
                break;
        }

        case FE_GET_FRONTEND:
                break;

        case FE_SLEEP:
		break;

        case FE_INIT:
		return stv0297_init (i2c);

	default:
		return -EOPNOTSUPP;
	};

	return 0;
}


static int stv0297_attach (struct dvb_i2c_bus *i2c, void **data)
{
	return dvb_register_frontend (stv0297_ioctl, i2c, NULL, &stv0297_info);
}


static void stv0297_detach (struct dvb_i2c_bus *i2c, void *data)
{
	dprintk ("%s\n", __FUNCTION__);
	dvb_unregister_frontend (stv0297_ioctl, i2c);
}


static int __init init_stv0297 (void)
{
	dprintk ("%s\n", __FUNCTION__);
	return dvb_register_i2c_device (NULL, stv0297_attach, stv0297_detach);
}


static void __exit exit_stv0297 (void)
{
	dprintk ("%s\n", __FUNCTION__);

	dvb_unregister_i2c_device (stv0297_attach);
}

module_init (init_stv0297);
module_exit (exit_stv0297);

MODULE_DESCRIPTION("STV0297/TSA5512 DVB-C Frontend driver");
MODULE_AUTHOR("Dennis Noermann");
MODULE_LICENSE("GPL");

