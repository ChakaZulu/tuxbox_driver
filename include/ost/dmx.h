/* 
 * dmx.h
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

#ifndef _OST_DMX_H_
#define _OST_DMX_H_

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#include <sys/time.h>
#endif

#ifndef EBUFFEROVERFLOW
#define EBUFFEROVERFLOW 769
#endif

#undef OSTNET		/* define this for nokia ostnet */

/* pid_t conflicts with linux/include/linux/types.h !!!*/

#ifdef OSTNET
typedef uint16_t    PID_t;      /* ostnet Packet identifier */
#else
typedef uint16_t dvb_pid_t;
#endif

#define DMX_FILTER_SIZE 16

typedef enum
{
	DMX_IN_FRONTEND, /* Input from a front-end device.  */
	DMX_IN_DVR       /* Input from the logical DVR device.  */
} dmxInput_t;

typedef enum
{
	DMX_OUT_DECODER, /* Streaming directly to decoder. */
	DMX_OUT_TAP,     /* Output going to a memory buffer */
	                 /* (to be retrieved via the read command).*/
	DMX_OUT_TS_TAP   /* Output multiplexed into a new TS  */
	                 /* (to be retrieved by reading from the */
	                 /* logical DVR device).                 */
} dmxOutput_t;

typedef enum
{
	DMX_PES_USER,
        DMX_PES_AUDIO,
	DMX_PES_VIDEO,
	DMX_PES_TELETEXT,
	DMX_PES_SUBTITLE,
	DMX_PES_PCR,
	DMX_PES_OTHER
} dmxPesType_t;


typedef enum
{
        DMX_SCRAMBLING_EV,
        DMX_FRONTEND_EV
} dmxEvent_t;


typedef enum
{
	DMX_SCRAMBLING_OFF,
	DMX_SCRAMBLING_ON
} dmxScramblingStatus_t;


typedef struct dmxFilter
{
	uint8_t         filter[DMX_FILTER_SIZE];
	uint8_t         mask[DMX_FILTER_SIZE];
} dmxFilter_t;


struct dmxFrontEnd
{
  //TBD             tbd;
};

/**
 * The dmxSctFilterParams structure defines the input parameters for
 * the set section filter function.
 */
struct dmxSctFilterParams
{
#ifdef OSTNET
	struct dmxFilter	     filter;	/* new api from ostdev (nokia) */
#else
	dmxFilter_t                  filter;
#endif
#ifdef OSTNET
	PID_t                        PID;	/* new api from ostdev (nokia) */
#else
	dvb_pid_t                    pid;
#endif
	uint32_t                     timeout;
	uint32_t                     flags;

#define DMX_CHECK_CRC       1
#define DMX_ONESHOT         2
#define DMX_IMMEDIATE_START 4
#define DMX_KERNEL_CLIENT   0x8000
};

/**
 * The dmxPesFilterParams structure defines the input parameters for
 * the set PES filter function.
 */
struct dmxPesFilterParams
{
#ifdef OSTNET
	struct dmxPesType_t          pesType;
#else
	dmxPesType_t                 pesType;
#endif
#ifdef OSTNET
	PID_t                        PID;
#else
	dvb_pid_t                    pid;
#endif
	dmxInput_t                   input;
	dmxOutput_t                  output;
	uint32_t                     flags;
};


struct dmxEvent
{
	dmxEvent_t                  event;
	time_t                      timeStamp;
	union
	{
		dmxScramblingStatus_t scrambling;
	} u;
};

/*
 * ioctl command numbers
 */
#ifdef OSTNET
#define DMX_IOCTL0       3
#define DMX_IOCTL1       4
#define DMX_IOCTL2       5
#define DMX_IOCTL3       6
#define DMX_IOCTL4       7
#define DMX_IOCTL5       8
#else
#define DMX_IOCTL0       41
#define DMX_IOCTL1       42
#define DMX_IOCTL2       43
#define DMX_IOCTL3       44
#define DMX_IOCTL4       45
#define DMX_IOCTL5       46
#define DMX_IOCTL6       47
#endif

/** Start filtering */
#ifdef OSTNET
#define DMX_START           _IO(OST_IOCTL,  DMX_IOCTL1)
#else
#define DMX_START           _IOW('o',DMX_IOCTL1,int)
#endif
/** Change buffer size */
#ifdef OSTNET
#define DMX_SET_BUFFER_SIZE _IOW(OST_IOCTL, DMX_IOCTL4, unsigned long)
#else
#define DMX_SET_BUFFER_SIZE _IOW('o',DMX_IOCTL4,unsigned long)
#endif
/** Set a new section filter */
#ifdef OSTNET
#define DMX_SET_FILTER      _IOW(OST_IOCTL, DMX_IOCTL2, struct dmxSctFilterParams)
#else
#define DMX_SET_FILTER      _IOW('o',DMX_IOCTL2,struct dmxSctFilterParams *)
#endif
/** Stop filtering */
#ifdef OSTNET
#define DMX_STOP            _IO(OST_IOCTL,  DMX_IOCTL0)
#else
#define DMX_STOP            _IOW('o',DMX_IOCTL0,int)
#endif
/** Set a new PES filter */
#ifdef OSTNET
#define DMX_SET_PES_FILTER  _IOW(OST_IOCTL, DMX_IOCTL3, struct dmxPesFilterParams)
#else
#define DMX_SET_PES_FILTER  _IOW('o',DMX_IOCTL3,struct dmxPesFilterParams *)
#endif
/** Change buffer size */
#ifdef OSTNET
#define DMX_GET_EVENT       _IOR(OST_IOCTL, DMX_IOCTL5, struct dmxEvent)
#else
#define DMX_GET_EVENT       _IOR('o',DMX_IOCTL5,struct dmxEvent *)
#endif
/** Get the pes pids */
#ifdef OSTNET
#else
#define DMX_GET_PES_PIDS    _IOR('o',DMX_IOCTL6,dvb_pid_t *)
#endif

#endif /*_OST_DMX_H_*/
