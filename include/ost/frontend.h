/*
 * frontend.h
 *
 * Copyright (C) 2000 Marcus Metzler <marcus@convergence.de>
 *                  & Ralph  Metzler <ralph@convergence.de>
 *                    for convergence integrated media GmbH
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

#ifndef _FRONTEND_H_
#define _FRONTEND_H_

#include <asm/types.h>


#define ENOSIGNAL 768
#ifndef EBUFFEROVERFLOW
#define EBUFFEROVERFLOW 769
#endif


typedef __u32 FrontendStatus;
typedef uint32_t feStatus;

/* bit definitions for FrontendStatus */
#define FE_HAS_POWER         1
#define FE_HAS_SIGNAL        2
#define FE_SPECTRUM_INV      4
#define FE_HAS_LOCK          8
#define FE_HAS_CARRIER      16
#define FE_HAS_VITERBI      32
#define FE_HAS_SYNC         64
#define FE_TUNER_HAS_LOCK  128

/* possible values for spectral inversion */
typedef enum {
        INVERSION_OFF,
        INVERSION_ON,
        INVERSION_AUTO
} SpectralInversion;

/* possible values for FEC_inner/FEC_outer */
typedef enum {
        FEC_AUTO,
        FEC_1_2,
        FEC_2_3,
        FEC_3_4,
        FEC_5_6,
        FEC_7_8,
        FEC_NONE
} CodeRate;

typedef enum {
        QPSK,
        QAM_16,
        QAM_32,
        QAM_64,
        QAM_128,
        QAM_256
} Modulation;

typedef enum {
        TRANSMISSION_MODE_2K,
        TRANSMISSION_MODE_8K
} TransmitMode;

typedef enum {
        BANDWIDTH_8_MHZ,
        BANDWIDTH_7_MHZ,
        BANDWIDTH_6_MHZ
} BandWidth;


typedef enum {
        GUARD_INTERVAL_1_32,
        GUARD_INTERVAL_1_16,
        GUARD_INTERVAL_1_8,
        GUARD_INTERVAL_1_4
} GuardInterval;


typedef enum {
        HIERARCHY_NONE,
        HIERARCHY_1,
        HIERARCHY_2,
        HIERARCHY_4
} Hierarchy;

typedef struct {
        __u32     SymbolRate; /* symbol rate in Symbols per second */
        CodeRate  FEC_inner;  /* forward error correction (see above) */
} QPSKParameters;

typedef struct {
        __u32      SymbolRate; /* symbol rate in Symbols per second */
        CodeRate   FEC_inner;  /* forward error correction (see above) */
        Modulation QAM;        /* modulation type (see above) */
} QAMParameters;

typedef struct {
        BandWidth     bandWidth;
        CodeRate      HP_CodeRate;          /* high priority stream code rate */
        CodeRate      LP_CodeRate;          /* low priority stream code rate */
        Modulation    Constellation;        /* modulation type (see above) */
        TransmitMode  TransmissionMode;
        GuardInterval guardInterval;
        Hierarchy     HierarchyInformation;
} OFDMParameters;

typedef enum {
        FE_QPSK,
        FE_QAM,
        FE_OFDM
} FrontendType;


typedef struct {
        __u32 Frequency;                /* (absolute) frequency in Hz for QAM/OFDM */
                                        /* intermediate frequency in kHz for QPSK */
	SpectralInversion Inversion;    /* spectral inversion */
        union {
                QPSKParameters qpsk;
                QAMParameters  qam;
                OFDMParameters ofdm;
        } u;
} FrontendParameters;


typedef enum {
        FE_UNEXPECTED_EV, /* unexpected event (e.g. loss of lock) */
        FE_COMPLETION_EV, /* completion event, tuning succeeded */
        FE_FAILURE_EV     /* failure event, we couldn't tune */
} EventType;

typedef struct {
        EventType type; /* type of event, FE_UNEXPECTED_EV, ... */

        long timestamp; /* time in seconds since 1970-01-01 */

        union {
                struct {
                        FrontendStatus previousStatus; /* status before event */
                        FrontendStatus currentStatus;  /* status during event */
                } unexpectedEvent;
                FrontendParameters completionEvent;    /* parameters for which the
                                                          tuning succeeded */
                FrontendStatus failureEvent;           /* status at failure (e.g. no lock) */
        } u;
} FrontendEvent;

typedef struct {
        FrontendType type;
        __u32        minFrequency;
        __u32        maxFrequency;
        __u32        maxSymbolRate;
        __u32        minSymbolRate;
        __u32        hwType;
        __u32        hwVersion;
} FrontendInfo;

typedef enum {
        FE_POWER_ON,
        FE_POWER_STANDBY,
        FE_POWER_SUSPEND,
        FE_POWER_OFF
} FrontendPowerState;

#define FE_SELFTEST                   _IO('o', 61)
#define FE_SET_POWER_STATE            _IOW('o', 62, FrontendPowerState)
#define FE_GET_POWER_STATE            _IOR('o', 63, FrontendPowerState*)
#define FE_READ_STATUS                _IOR('o', 64, FrontendStatus*)
#define FE_READ_BER                   _IOW('o', 65, __u32*)
#define FE_READ_SIGNAL_STRENGTH       _IOR('o', 66, __s32*)
#define FE_READ_SNR                   _IOR('o', 67, __s32*)
#define FE_READ_UNCORRECTED_BLOCKS    _IOW('o', 68, __u32*)
#define FE_GET_NEXT_FREQUENCY         _IOW('o', 69, __u32*)
#define FE_GET_NEXT_SYMBOL_RATE       _IOW('o', 70, __u32*)
#define FE_SET_FRONTEND               _IOW('o', 71, FrontendParameters*)
#define FE_GET_FRONTEND               _IOR('o', 72, FrontendParameters*)
#define FE_GET_INFO                   _IOR('o', 73, FrontendInfo*)
#define FE_GET_EVENT                  _IOR('o', 74, FrontendEvent*)

/* use of QPSK_* is deprecated, use the generic FE_* instead */

#define QAM_TYPE Modulation

/*
typedef enum
{	QAM_16,
	QAM_32,
	QAM_64,
	QAM_128,
	QAM_256
} QAM_TYPE;
*/

struct qpskParameters {
	uint32_t iFrequency;  /* intermediate frequency in KHz */
	uint32_t SymbolRate;  /* symbol rate in Hz */
	uint8_t FEC_inner;    /* error correction (see above) */
};

struct qamParameters {
	uint32_t Frequency;   /* (absolute) frequency in KHz */
	uint32_t SymbolRate;  /* symbol rate in Hz */
	uint8_t FEC_inner;    /* error correction (see above) */
        uint8_t QAM;          /* modulation type (see above) */
};

enum {
        QPSK_UNEXPECTED_EV,
	QPSK_COMPLETION_EV,
	QPSK_FAILURE_EV
};

struct qpskEvent {
	int32_t type;      /* type of event, FE_UNEXPECTED_EV, ... */
	time_t timestamp;  /* time of event as returned by time() */
	union {
		struct {
			feStatus previousStatus; /* status before event */
			feStatus currentStatus;  /* status during event */
		} unexpectedEvent;
		struct qpskParameters completionEvent; /* parameters for which the
							  tuning succeeded */
		feStatus failureEvent;  /* status at failure (e.g. no lock) */
	} u;
};

struct qamEvent {
	int32_t type;
	time_t timestamp;
	union {
		struct {
			feStatus previousStatus;
			feStatus currentStatus;
		} unexpectedEvent;
		struct qamParameters completionEvent;
		feStatus failureEvent;
	} u;
};

struct qpskRegister {
	uint8_t chipId;
	uint8_t address;
	uint8_t value;
};

struct qamRegister {
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

struct qamFrontendInfo {
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

#define OST_SELFTEST                   _IO('o',61)
#define OST_SET_POWER_STATE            _IOW('o',62,uint32_t)
#define OST_GET_POWER_STATE            _IOR('o',63,uint32_t *)
#define QPSK_TUNE                      _IOW('o',71,struct qpskParameters *)
#define QPSK_GET_EVENT                 _IOR('o',72,struct qpskEvent *)
#define QPSK_FE_INFO                   _IOR('o',73,struct qpskFrontendInfo *)
#define QPSK_WRITE_REGISTER            _IOW('o',74,struct qpskRegister *)
#define QPSK_READ_REGISTER             _IOR('o',75,struct qpskRegister *)
#define QPSK_GET_STATUS                _IOR('o',76,struct qpskParameters *)
#define QAM_TUNE                       _IOW('o',81,struct qamParameters *)
#define QAM_GET_EVENT                  _IOR('o',82,struct qamEvent *)
#define QAM_FE_INFO                    _IOR('o',83,struct qamFrontendInfo *)
#define QAM_WRITE_REGISTER             _IOW('o',84,struct qamRegister *)
#define QAM_READ_REGISTER              _IOR('o',85,struct qamRegister *)
#define QAM_GET_STATUS                 _IOR('o',86,struct qamParameters *)

#endif /*_OST_FRONTEND_H_*/

