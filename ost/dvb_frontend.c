/*
 * $Id: dvb_frontend.c,v 1.4 2002/02/28 19:25:14 obi Exp $
 *
 * dvb_frontend.c: DVB frontend driver module
 *
 * Copyright (C) 1999-2001 Ralph  Metzler 
 *		       & Marcus Metzler for convergence integrated media GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 * $Log: dvb_frontend.c,v $
 * Revision 1.4  2002/02/28 19:25:14  obi
 * replaced get_fast_time by do_gettimeofday for kernel 2.4.18 compatibility
 *
 * Revision 1.3  2002/02/24 16:09:56  woglinde
 * 2 files for new-api, were not added
 *
 * Revision 1.1.2.1  2002/01/22 23:44:56  fnbrd
 * Id und Log reingemacht.
 *
 *
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/smp_lock.h>
#include <linux/string.h>
#include <linux/module.h>
#include <dbox/dvb_frontend.h>

#ifdef MODULE
MODULE_DESCRIPTION("");
MODULE_AUTHOR("Ralph Metzler");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#endif

#define dprintk	if(0) printk

static inline void ddelay(int i) 
{
	current->state=TASK_INTERRUPTIBLE;
	schedule_timeout((HZ*i)/100);
}

static void 
fe_add_event(DVBFEEvents *events, FrontendEvent *ev)
{
	int wp;
	struct timeval tv;

	do_gettimeofday(&tv);
	ev->timestamp=tv.tv_sec;
	
	spin_lock(&events->eventlock);
	wp=events->eventw;
	wp=(wp+1)%MAX_EVENT;
	if (wp==events->eventr) {
		events->overflow=1;
		events->eventr=(events->eventr+1)%MAX_EVENT;
	}
	memcpy(&events->events[events->eventw], ev, sizeof(FrontendEvent));
	events->eventw=wp;
	spin_unlock(&events->eventlock);
	wake_up(&events->eventq);
}

static int 
demod_command(dvb_front_t *fe, unsigned int cmd, void *arg)
{
	if (!fe->demod)
		return -1;
	return fe->demod->driver->command(fe->demod, cmd, arg);
}

static int
fe_lock(dvb_front_t *fe)
{
	int lock;
	FrontendStatus status;

	if (demod_command(fe, FE_READ_STATUS, &status))
		return 0;
	lock=(status&FE_HAS_LOCK) ? 1 : 0;
	fe->lock=lock;
	return lock;
}

static unsigned long 
fe_afc(dvb_front_t *fe)
{
	s32 afc;

	demod_command(fe, FE_READ_STATUS, &afc);

	if (!fe_lock(fe)) {
		fe->tuning=FE_STATE_TUNE;   // re-tune
		fe->delay=HZ/2;
		return -1;
	}
	fe->delay=HZ*60;
	if (fe->type==DVB_C)
		return 0;
#if 0
	/* this does not work well for me, after the first adjustment I
	   lose sync in most cases */

	if (!afc) {
		fe->delay=HZ*60;
		return 0;  
	}
	fe->delay=HZ*10;
	
	fe->curfreq -= (afc/2); 
	demod_command(fe, FE_SETFREQ, &fe->curfreq); 

	/* urghh, this prevents loss of lock but is visible as artifacts */
	demod_command(fe, FE_RESET, 0); 
#endif
	return 0;
}

static int
fe_complete(dvb_front_t *fe)
{
	FrontendEvent ev;

	ev.type=FE_COMPLETION_EV;
	memcpy (&ev.u.completionEvent, &fe->param,
		sizeof(FrontendParameters));
	
	ev.u.completionEvent.Frequency=fe->param.Frequency;

	fe_add_event(&fe->events, &ev);

	fe->tuning=FE_STATE_AFC;
	fe->delay=HZ*10;
	fe->complete_cb(fe->priv);
	return 0;
}

static int
fe_fail(dvb_front_t *fe)
{
	FrontendEvent ev;

	fe->tuning=FE_STATE_IDLE;
	ev.type=FE_FAILURE_EV;
	fe_add_event(&fe->events, &ev);

	return -1;
}

static int
fe_zigzag(dvb_front_t *fe)
{
	int i=fe->zz_state;
	u32 sfreq=fe->param.Frequency;
	u32 soff;

	if (fe_lock(fe)) 
		return fe_complete(fe);
	if (i==20) {
		/* return to requested frequency, maybe it locks when the user
		 * retries the tuning operation
		 */
		demod_command(fe, FE_SETFREQ, &sfreq);
		demod_command(fe, FE_RESET, 0);
		return fe_fail(fe);
	}

	if (fe->type == DVB_S)
		soff=fe->param.u.qpsk.SymbolRate/16000;
	else if (fe->type == DVB_C)
		soff=fe->param.u.qam.SymbolRate/16000;
	else if (fe->type == DVB_T)
		#warning finish fe_zigzag for ofdm
		soff=0;
	else 
		soff=0;
	if (i&1) 
		sfreq=fe->param.Frequency+soff*(i/2);
	else
		sfreq=fe->param.Frequency-soff*(i/2);
	dprintk("fe_zigzag: i=%.2d freq=%d\n", i, sfreq);

	demod_command(fe, FE_SETFREQ, &sfreq); 
	demod_command(fe, FE_RESET, 0);
	fe->curfreq=sfreq;
	fe->zz_state++;

	if (fe->type == DVB_S)
		fe->delay=HZ/20;
	else if (fe->type == DVB_C || fe->type == DVB_T)
		fe->delay=HZ/2;

	if (fe->demod_type==DVB_DEMOD_STV0299) {
		fe->delay=HZ/20;
		demod_command(fe, FE_RESET, 0); 
	}
	return i;
}

static int 
fe_tune(dvb_front_t *fe)
{
	int kickit = 0, locked = fe_lock(fe), do_set_front = 0;
	FrontendParameters *param, *new_param;
	
	param=&fe->param;
	new_param=&fe->new_param;
	
	if (!locked
	    || param->Frequency != new_param->Frequency) {
	  demod_command(fe, FE_SETFREQ,
			      &new_param->Frequency);
		kickit = 1;
	}
	/* if locked == 1, dvb->front.fec is guaranteed to be != FEC_AUTO,
	 * so using FEC_AUTO will always reset the demod
	 */
	switch (fe->type) {
	case DVB_S:
		do_set_front =
			param->u.qpsk.SymbolRate != new_param->u.qpsk.SymbolRate
			|| param->u.qpsk.FEC_inner != new_param->u.qpsk.FEC_inner
			|| param->Inversion != new_param->Inversion;
		break;
	case DVB_C:
		do_set_front =
			param->u.qam.SymbolRate != new_param->u.qam.SymbolRate
			|| param->u.qam.FEC_inner != new_param->u.qam.FEC_inner
			|| param->Inversion != new_param->Inversion;
		break;
	case DVB_T:
		#warning FIXME optimize fe_tune for ofdm
		do_set_front = 1;
		break;
	default:
		break;
	}

	memcpy(param, new_param, sizeof(FrontendParameters));

	if (!locked || do_set_front) {
		demod_command(fe, FE_SET_FRONTEND, param);
		kickit = 1;
	}
	
	if (kickit)
		demod_command(fe, FE_RESET, 0);

	if (fe->type == DVB_S) {
		mdelay(10);
		fe->zz_state = 1;
	}
	else if (fe->type == DVB_C || fe->type == DVB_T) {
		#warning FIXME optimize fe_tune for ofdm
		mdelay(30);
		fe->zz_state=0;
	}
	fe->curfreq=param->Frequency;

	if (fe_lock(fe)) {
		dprintk("mon_tune: locked on 1st try\n");
		return fe_complete(fe);
	}
	dprintk("need zigzag, freq=%d\n", new_param->Frequency);
	fe->tuning=FE_STATE_ZIGZAG;
	if (fe->type == DVB_S)
		fe->delay = HZ/10;
	else if (fe->type == DVB_C || fe->type == DVB_T)
		#warning FIXME optimize fe_tune for ofdm
		fe->delay = HZ/2;

	return 0;
}


static int 
fe_thread(void *data)
{
	dvb_front_t *fe = (dvb_front_t *) data;
    
	lock_kernel();
	daemonize();
	sigfillset(&current->blocked);
	strcpy(current->comm,"fe_thread");
	fe->thread = current;
	unlock_kernel();

	for (;;) {
		interruptible_sleep_on_timeout(&fe->wait, fe->delay);
		if (fe->exit || signal_pending(current))
			break;

		down_interruptible(&fe->sem);
		switch (fe->tuning) {
		case FE_STATE_TUNE:
			fe_tune(fe);
			break;
#if 0
		case FE_STATE_ZIGZAG:
			fe_zigzag(fe);
			break;
		case FE_STATE_AFC:
			fe_afc(fe);
			break;
#endif
		default:
			fe->delay=HZ;
			break;
		}
		up(&fe->sem);
	}
	fe->thread = NULL;
	return 0;
}

void
dvb_frontend_stop(dvb_front_t *fe)
{
	if (fe->tuning==FE_STATE_IDLE)
		return;
	down_interruptible(&fe->sem);
	fe->tuning=FE_STATE_IDLE;
	wake_up_interruptible(&fe->wait);
	up(&fe->sem);
}

void
dvb_frontend_tune(dvb_front_t *fe, FrontendParameters *new_param)
{
	memcpy(&fe->new_param, new_param, 
	       sizeof(FrontendParameters));
	down_interruptible(&fe->sem);
	fe->tuning=FE_STATE_TUNE;
	wake_up_interruptible(&fe->wait);
	up(&fe->sem);
}

void
dvb_frontend_sec_set_tone(dvb_front_t *fe, secToneMode mode)
{
	demod_command(fe, FE_SEC_SET_TONE, (void*)mode);
}

void
dvb_frontend_sec_set_voltage(dvb_front_t *fe, secVoltage voltage)
{
	demod_command(fe, FE_SEC_SET_VOLTAGE, (void*)voltage);
}

void
dvb_frontend_sec_mini_command(dvb_front_t *fe, secMiniCmd command)
{
	demod_command(fe, FE_SEC_MINI_COMMAND, (void*)command);
}

void
dvb_frontend_sec_command(dvb_front_t *fe, struct secCommand *command)
{
	demod_command(fe, FE_SEC_COMMAND, (void*)command);
}

void
dvb_frontend_sec_get_status(dvb_front_t *fe, struct secStatus *status)
{
	demod_command(fe, FE_SEC_GET_STATUS, (void*)status);
}

int
dvb_frontend_get_event(dvb_front_t *fe, FrontendEvent *event, int nonblocking)
{
	int ret;
	DVBFEEvents *events=&fe->events;
	
	if (events->overflow) {
		events->overflow=0;
		return -EBUFFEROVERFLOW;
	}
	if (events->eventw==events->eventr) {
		if (nonblocking) 
			return -EWOULDBLOCK;
		
		ret=wait_event_interruptible(events->eventq,
					     events->eventw!=
					     events->eventr);
		if (ret<0)
			return ret;
	}
	
	spin_lock(&events->eventlock);
	memcpy(event, 
	       &events->events[events->eventr],
	       sizeof(FrontendEvent));
	events->eventr=(events->eventr+1)%MAX_EVENT;
	spin_unlock(&events->eventlock);
	return 0;
}

int dvb_frontend_poll(dvb_front_t *fe, struct file *file, poll_table * wait)
{
	if (fe->events.eventw!=fe->events.eventr)
		return (POLLIN | POLLRDNORM | POLLPRI);
	
	poll_wait(file, &fe->events.eventq, wait);
	
	if (fe->events.eventw!=fe->events.eventr)
		return (POLLIN | POLLRDNORM | POLLPRI);
	return 0;
}

int 
dvb_frontend_init(dvb_front_t *fe)
{
	init_waitqueue_head(&fe->wait);
	sema_init(&fe->sem, 1);
	fe->tuning=FE_STATE_IDLE;
	fe->exit=0;
	fe->delay=HZ;
	
	init_waitqueue_head(&fe->events.eventq);
	spin_lock_init (&fe->events.eventlock);
	fe->events.eventw=fe->events.eventr=0;
	fe->events.overflow=0;
	
	demod_command(fe, FE_INIT, 0); 
	kernel_thread(fe_thread, fe, 0);
	return 0;
}

void
dvb_frontend_exit(dvb_front_t *fe)
{
	if (!fe->thread)
		return;
	fe->exit=1;
	wake_up_interruptible(&fe->wait);
	while (fe->thread)
		ddelay(1);
}

#ifdef MODULE
#ifdef EXPORT_SYMTAB
EXPORT_SYMBOL(dvb_frontend_init);
EXPORT_SYMBOL(dvb_frontend_exit);
EXPORT_SYMBOL(dvb_frontend_tune);
EXPORT_SYMBOL(dvb_frontend_sec_set_tone);
EXPORT_SYMBOL(dvb_frontend_sec_mini_command);
EXPORT_SYMBOL(dvb_frontend_sec_command);
EXPORT_SYMBOL(dvb_frontend_sec_set_voltage);
EXPORT_SYMBOL(dvb_frontend_stop);
EXPORT_SYMBOL(dvb_frontend_get_event);
EXPORT_SYMBOL(dvb_frontend_poll);
#endif
#endif
