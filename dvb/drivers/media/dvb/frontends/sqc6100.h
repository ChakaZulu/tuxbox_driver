/*
 * $Id: sqc6100.h,v 1.1 2003/07/29 18:30:30 obi Exp $
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

#ifndef _DVB_FRONTENDS_SQC6100_H
#define _DVB_FRONTENDS_SQC6100_H

enum sqc6100_reg_addr {
	VD_STATUS	= 0x00,
	VD_MODE		= 0x02,
	GC_RES_TSERR	= 0x03,
	CLKGEN_MODE1	= 0x04,
	CLKGEN_MODE2	= 0x05,
	VD_BER_THR12	= 0x07,
	VD_BER_THR23	= 0x08,
	VD_BER_THR34	= 0x09,
	VD_BER_THR56	= 0x0a,
	VD_BER_THR78	= 0x0b,
	VD_BER1		= 0x0f,
	VD_BER2		= 0x10,
	GPIO_PDM0	= 0x12,
	GPIO_PDM1	= 0x13,
	IQ_FRQ1		= 0x15,
	IQ_FRQ2		= 0x16,
	IQ_FRQ3		= 0x17,
	IQ_MODE		= 0x18,
	RSU_SMP1	= 0x19,
	RSU_SMP2	= 0x1a,
	RSU_SMP3	= 0x1b,
	CE_IDC_MODE	= 0x1c,
	FS_MODE1	= 0x25,
	FS_MODE2	= 0x26,
	OUTINF_MODE	= 0x2c,
	RS_EST_PERIOD	= 0x30,
	RS_BITERR1	= 0x31,
	RS_BITERR2	= 0x32,
	RS_FRMERR	= 0x33,
	RS_FRMRCV	= 0x34,
	RS_BYTERR1	= 0x35,
	RS_BYTERR2	= 0x36,
	RS_BITERR_THR1	= 0x37,
	RS_BITERR_THR2	= 0x38,
	RS_FRMERR_THR	= 0x39,
	RS_FRMRCV_THR	= 0x3a,
	RS_BYTERR_THR1	= 0x3b,
	RS_BYTERR_THR2	= 0x3c,
	IFFT_SCALE1	= 0x3d,
	IFFT_SCALE2	= 0x3e,
	IFFT_LIMIT1	= 0x3f,
	IFFT_LIMIT2	= 0x40,
	FT_WINEST1	= 0x41,
	FT_WINEST2	= 0x42,
	FFT_SCALE1	= 0x43,
	FFT_SCALE2	= 0x44,
	FFT_LIMIT1	= 0x45,
	FFT_LIMIT2	= 0x46,
	FT_PERIOD	= 0x4c,
	AFCTR_IDC_NUM	= 0x4d,
	SD_STATUS	= 0x4f,
	AGC_GAIN	= 0x50,
	SD_MAGAV	= 0x51,
	SD_MEAN		= 0x52,
	AGC_EXT_GAIN	= 0x55,
	AGC_ADC_MODE	= 0x56,
	SD_MODE		= 0x57,
	AGC_STEPSIZE	= 0x58,
	AGC_MAGAV_TOL	= 0x59,
	AGC_MAGAV_TAR	= 0x5a,
	AAF_MODE	= 0x5c,
	SD_PERIOD	= 0x5d,
	TPS_SYNC1	= 0x5e,
	TPS_SYNC2	= 0x5f,
	TPS_PARAM1	= 0x60,
	TPS_PARAM2	= 0x61,
	TPS_PARAM3	= 0x62,
	TPS_RESERV1	= 0x63,
	TPS_RESERV2	= 0x64,
	TPS_PARITY1	= 0x65,
	TPS_PARITY2	= 0x66,
	ESC_MODE	= 0x69,
	ESC_SCALE_INIT	= 0x6a,
	ESC_SCALE_TOL	= 0x6b,
	ESC_CDF_TAR	= 0x73,
	ESC_SCALE	= 0x74,
	ESC_STATUS	= 0x75,
	GC_WD2_CH1	= 0x76,
	GC_WD2_CH2	= 0x77,
	GC_WD2_THR1	= 0x78,
	GC_WD2_THR2	= 0x79,
	GC_WD1_CH2	= 0x7a,
	GC_WD1_SE2	= 0x7b,
	GC_WD1_SE1CH1	= 0x7c,
	GPIO_MODE1	= 0x7d,
	GPIO_MODE2	= 0x7e,
	GPIO_DATOUT	= 0x7f,
	GPIO_DATIN	= 0x80,
	GC_MODE1	= 0x81,
	GC_MODE2	= 0x82,
	GC_MODE3	= 0x83,
	GC_MODE4	= 0x84,
	GC_START	= 0x85,
	CT_MODE		= 0x88,
	CT_HEAD		= 0x89,
	CT_PASS		= 0x8a,
	AFCACQ_MODE	= 0x8b,
	AFCACQ_PASS	= 0x8c,
	AFCTRK_MODE	= 0x8d,
	AFCTRK_PASS	= 0x8e,
	AFCTRK_IDC_PER	= 0x8f,
	AFCTRK_IDC_THR	= 0x90,
	AFCTRK_FRQ_PI	= 0x91,
	AFCTRK_SMP_PI	= 0x92,
	AFCTRK_DETUN1	= 0x93,
	AFCTRK_DETUN2	= 0x94,
	FRQ_SUB1	= 0x95,
	FRQ_SUB2	= 0x96,
	FRQACQ_TOL1	= 0x97,
	FRQACQ_TOL2	= 0x98,
	FRQTRK_TOL1	= 0x99,
	FRQTRK_TOL2	= 0x9a,
	SMPACQ_TOL1	= 0x9b,
	SMPACQ_TOL2	= 0x9c,
	SMPTRK_TOL1	= 0x9d,
	SMPTRK_TOL2	= 0x9e,
	CT_WINEST1	= 0xa0,
	CT_WINEST2	= 0xa1,
	AFCACQ_RASTER	= 0xa2,
	AFC_FRQ_CTRL1	= 0xa3,
	AFC_FRQ_CTRL2	= 0xa4,
	AFC_SMP_CTRL1	= 0xa5,
	AFC_SMP_CTRL2	= 0xa6,
	AFCTRK_FRQ_S1	= 0xac,
	AFCTRK_FRQ_S2	= 0xad,
	AFCTRK_SMP_S1	= 0xae,
	AFCTRK_SMP_S2	= 0xaf,
	CT_STATUS	= 0xb4,
	AFCACQ_STATUS	= 0xb5,
	AFCTRK_STATUS	= 0xb6,
	INTR_MODE	= 0xb7,
	INTR_SOFTMASK1	= 0xb8,
	INTR_SOFTMASK2	= 0xb9,
	INTR_SOFTMASK3	= 0xba,
	INTR_SOFTMASK4	= 0xbb,
	INTR_SOFTMASK5	= 0xbc,
	INTR_SOFTMASK6	= 0xbd,
	INTR_SOFTMASK7	= 0xbe,
	INTR_SOFTMASK8	= 0xbf,
	INTR_HARDMASK1	= 0xc0,
	INTR_HARDMASK2	= 0xc1,
	INTR_HARDMASK3	= 0xc2,
	INTR_HARDMASK4	= 0xc3,
	INTR_HARDMASK5	= 0xc4,
	INTR_HARDMASK6	= 0xc5,
	INTR_HARDMASK7	= 0xc6,
	INTR_HARDMASK8	= 0xc7,
	INTR_STATUS1	= 0xc8,
	INTR_STATUS2	= 0xc9,
	INTR_STATUS3	= 0xca,
	INTR_STATUS4	= 0xcb,
	INTR_STATUS5	= 0xcc,
	INTR_STATUS6	= 0xcd,
	INTR_STATUS7	= 0xce,
	INTR_STATUS8	= 0xcf,
};

#endif /* _DVB_FRONTENDS_SQC6100_H */
