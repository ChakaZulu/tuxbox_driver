/* 
 * frontend.h
 *
 * Copyright (C) 2000 Marcus Metzler <marcus@convergence.de>
 *                  & Ralph  Metzler <ralph@convergence.de>
                      for convergence integrated media GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef _OST_FRONTEND_H_
#define _OST_FRONTEND_H_

typedef uint32_t feStatus;

#define ENOSIGNAL 768
//#define EBUFFEROVERFLOW 769

#define FE_HAS_POWER       1
#define FE_HAS_SIGNAL      2
#define QPSK_SPECTRUM_INV  4

struct qpskParameters {
	uint32_t iFrequency;
	uint32_t SymbolRate;
	uint8_t FEC_inner;
};

enum {
        QPSK_UNEXPECTED_EV, 
	QPSK_COMPLETION_EV,
	QPSK_FAILURE_EV
};

struct qpskEvent {
	int32_t type;
	time_t timestamp;
	union {
		struct {
			feStatus previousStatus;
			feStatus currentStatus;
		} unexpectedEvent;
		struct qpskParameters completionEvent;
		feStatus failureEvent;
	} u;
};


struct qpskRegister {
	uint8_t chipId;
	uint8_t address;
	uint8_t value;
};


struct qpskFrontendInfo {
	uint32_t minFrequency;
	uint32_t maxFrequency;
	uint32_t maxSymbolRate;
	uint32_t minSymbolRate;
	uint32_t hwType;
	uint32_t hwVersion;
};

typedef enum {
	OST_POWER_ON, 
	OST_POWER_STANDBY, 
	OST_POWER_SUSPEND, 
	OST_POWER_OFF
} powerState_t;


#define OST_SELFTEST                   _IOWR('o',61,void)
#define OST_SET_POWER_STATE            _IOW('o',62,uint32_t)
#define OST_GET_POWER_STATE            _IOR('o',63,uint32_t *)
#define FE_READ_STATUS                 _IOR('o',64,feStatus *)
#define FE_READ_BER                    _IOW('o',65,uint32_t *)
#define FE_READ_SIGNAL_STRENGTH        _IOR('o',66,int32_t *)
#define FE_READ_SNR                    _IOR('o',67,int32_t *)
#define FE_READ_UNCORRECTED_BLOCKS     _IOW('o',68,uint32_t *)
#define FE_GET_NEXT_FREQUENCY          _IOW('o',69,uint32_t *)
#define FE_GET_NEXT_SYMBOL_RATE        _IOW('o',70,uint32_t *)

#define QPSK_TUNE                      _IOW('o',71,struct qpskParameters *)
#define QPSK_GET_EVENT                 _IOR('o',72,struct qpskEvent *)
#define QPSK_FE_INFO                   _IOR('o',73,struct qpskFrontendInfo *)
#define QPSK_WRITE_REGISTER            _IOW('o',74,struct qpskRegister *)
#define QPSK_READ_REGISTER             _IOR('o',75,struct qpskRegister *)

#endif /*_OST_FRONTEND_H_*/

