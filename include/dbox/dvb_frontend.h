/*
 * $Id: dvb_frontend.h,v 1.4 2002/06/16 19:49:35 Homar Exp $
 *
 * dvb_frontend.h
 *
 * Copyright (C) 2001 Ralph  Metzler <ralph@convergence.de>
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
 * $Log: dvb_frontend.h,v $
 * Revision 1.4  2002/06/16 19:49:35  Homar
 * AutoInversion eingefügt
 *
 * Revision 1.3  2002/02/24 16:09:56  woglinde
 * 2 files for new-api, were not added
 *
 * Revision 1.1.2.1  2002/01/22 23:44:56  fnbrd
 * Id und Log reingemacht.
 *
 *
 */

#ifndef _DVB_FRONTEND_H_
#define _DVB_FRONTEND_H_

#include <ost/frontend.h>
#include <ost/sec.h>
#include <dbox/dvb.h>

#include <linux/sched.h>
#include <linux/i2c.h>
#include <linux/ioctl.h>
#include <linux/videodev.h>

#ifndef I2C_DRIVERID_STV0299
#define I2C_DRIVERID_STV0299 I2C_DRIVERID_EXP0
#endif

#ifndef I2C_DRIVERID_TDA8083
#define I2C_DRIVERID_TDA8083 I2C_DRIVERID_EXP1
#endif

#ifndef I2C_DRIVERID_L64781
#define I2C_DRIVERID_L64781 I2C_DRIVERID_EXP2
#endif

#ifndef I2C_DRIVERID_VES1993
#define I2C_DRIVERID_VES1993 I2C_DRIVERID_EXP3
#endif

#ifndef I2C_DRIVERID_TDA8044
#define I2C_DRIVERID_TDA8044 I2C_DRIVERID_EXP4
#endif

#ifndef I2C_DRIVERID_AT76C651
#define I2C_DRIVERID_AT76C651 I2C_DRIVERID_EXP5
#endif

#ifndef I2C_DRIVERID_EXP4
#define I2C_DRIVERID_EXP4		0xF4
#endif

#ifndef I2C_DRIVERID_EXP5
#define I2C_DRIVERID_EXP5		0xF5
#endif

#define FE_STATE_IDLE   0
#define FE_STATE_TUNE   1
#define FE_STATE_ZIGZAG 2
#define FE_STATE_AFC    3


#define MAX_EVENT 8

typedef struct {
	FrontendEvent     events[MAX_EVENT];
	int               eventw;
	int               eventr;
	int               overflow;
	wait_queue_head_t eventq;
	spinlock_t        eventlock;
} DVBFEEvents;

typedef struct dvb_frontend {
	int                    type;
#define DVB_NONE 0
#define DVB_S    1
#define DVB_C    2
#define DVB_T    3
	int capabilities;

	void *priv;
	void (*complete_cb)(void *);

	struct i2c_adapter     *i2cbus;

		/* tuner  and sec is set via demod too */
	struct i2c_client      *demod;
	int demod_type; /* demodulator type */
#define DVB_DEMOD_NONE    0
#define DVB_DEMOD_VES1893 1
#define DVB_DEMOD_VES1820 2
#define DVB_DEMOD_STV0299 3
#define DVB_DEMOD_TDA8083 4
#define DVB_DEMOD_L64781  5
#define DVB_DEMOD_VES1993 6
#define DVB_DEMOD_TDA8044 7
#define DVB_DEMOD_AT76C651 8

	struct task_struct     *thread;
	wait_queue_head_t       wait;
	struct semaphore        sem;
	int                     tuning;
	int                     exit;
	int                     zz_state;
	unsigned long           delay;
	int                     lock;

	u32 curfreq;
	FrontendParameters      param;
	FrontendParameters      new_param;
	DVBFEEvents             events;
} dvb_front_t;

#define FE_INIT      _IOR('v',  BASE_VIDIOCPRIVATE+0x12, void)
#define FE_RESET     _IOR('v',  BASE_VIDIOCPRIVATE+0x13, void)
#define FE_WRITEREG  _IOR('v',  BASE_VIDIOCPRIVATE+0x14, u8 *)
#define FE_READREG   _IOR('v',  BASE_VIDIOCPRIVATE+0x15, u8 *)
#define FE_READ_AFC  _IOR('v',  BASE_VIDIOCPRIVATE+0x16, s32 *)
#define FE_SET_INVERSION  _IOR('v',  BASE_VIDIOCPRIVATE+0x17, u32 *)

int dvb_frontend_init(dvb_front_t *fe);
void dvb_frontend_exit(dvb_front_t *fe);
void dvb_frontend_tune(dvb_front_t *fe, FrontendParameters *new_param);
void dvb_frontend_sec_set_tone(dvb_front_t *fe, secToneMode mode);
void dvb_frontend_sec_set_voltage(dvb_front_t *fe, secVoltage voltage);
void dvb_frontend_sec_mini_command(dvb_front_t *fe, secMiniCmd command);
void dvb_frontend_sec_command(dvb_front_t *fe, struct secCommand *command);
void dvb_frontend_sec_get_status(dvb_front_t *fe, struct secStatus *status);
void dvb_frontend_stop(dvb_front_t *fe);
int dvb_frontend_get_event(dvb_front_t *fe, FrontendEvent *event, int nonblocking);
int dvb_frontend_poll(dvb_front_t *fe, struct file *file, poll_table * wait);

extern int register_frontend(dvb_front_t *frontend);
extern int unregister_frontend(dvb_front_t *frontend);

#endif
