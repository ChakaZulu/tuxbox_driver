/* 
 * dmxdev.c - DVB demultiplexer device 
 *
 * Copyright (C) 2000 Ralph  Metzler <ralph@convergence.de>
 *                  & Marcus Metzler <marcus@convergence.de>
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

#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <asm/uaccess.h>

#include "dmxdev.h"

inline dmxdev_filter_t *
DmxDevFile2Filter(dmxdev_t *dmxdev, struct file *file)
{
        return (dmxdev_filter_t *) file->private_data;
}

static inline void 
DmxDevBufferInit(dmxdev_buffer_t *buffer) 
{
        buffer->data=0;
        buffer->size=1024*1024;
        buffer->pread=0;
        buffer->pwrite=0;
        buffer->error=0;
        init_waitqueue_head(&buffer->queue);
}

static inline int 
DmxDevBufferWrite(dmxdev_buffer_t *buf, uint8_t *src, int len) 
{
        int split;
        int free;
        int todo;

	if (!len || !buf->data)
	        return 0;
        free=buf->pread-buf->pwrite;
        split=0;
        if (free<=0) {
                free+=buf->size;
                split=buf->size-buf->pwrite;
        }
        if (len>=free)
        {
                printk("buffer overflow. len: %d free %d\n", len, free);
                return -1;
        }
        if (split>=len)
                split=0;
        todo=len;
        if (split) {
                memcpy(buf->data + buf->pwrite, src, split);
                todo-=split;
                buf->pwrite=0;
        }
        memcpy(buf->data + buf->pwrite, src+split, todo);
        buf->pwrite=(buf->pwrite+todo)%buf->size;
        return len;
}

static ssize_t 
DmxDevBufferRead(dmxdev_buffer_t *src, int non_blocking, 
		 char *buf, size_t count, loff_t *ppos)
{
        unsigned long todo=count;
        int split, avail, error;
	
	if (!src->data)
	        return 0;
       	if ((error=src->error)) {
	        src->error=0;
		return error; 
	}

	// if non-blocking and no data return -EWOULDBLOCK
	if (non_blocking && (src->pwrite==src->pread))
	        return -EWOULDBLOCK;

        while (todo>0) {
	        // if blocking and no data return -EWOULDBLOCK
	        if (non_blocking && (src->pwrite==src->pread))
		        return (count-todo) ? (count-todo) : -EWOULDBLOCK;

		// wait for queue 
	        if (wait_event_interruptible(src->queue,
					     (src->pread!=src->pwrite) ||
					     (src->error))<0)
		        return count-todo;

		if ((error=src->error)) {
		        src->error=0;
			return error; 
		}
		
                split=src->size;
                avail=src->pwrite - src->pread;
                if (avail<0) {
                        avail+=src->size;
                        split=src->size - src->pread;
                }
                if (avail>todo)
                        avail=todo;
                if (split<avail) {
                        if (copy_to_user(buf, src->data+src->pread, split))
                                  return -EFAULT;
                        buf+=split;
                        src->pread=0;
                        todo-=split;
                        avail-=split;
                }
                if (avail) {
                        if (copy_to_user(buf, src->data+src->pread, avail))
                                return -EFAULT;
                        src->pread = (src->pread + avail) % src->size;
                        todo-=avail;
                        buf+=avail;
                }
        }
        return count;
}

static dmx_frontend_t *
get_fe(dmx_demux_t *demux, int type)
{
        struct list_head *head, *pos;

        head=demux->get_frontends(demux);
	if (!head)
	        return 0;
	list_for_each(pos, head)
	        if (DMX_FE_ENTRY(pos)->source==type)
		        return DMX_FE_ENTRY(pos);
	
	return 0;
}

int 
DmxDevDVROpen(dmxdev_t *dmxdev, struct file *file)
{
        dmx_frontend_t *front;

	if ((file->f_flags&O_ACCMODE)==O_RDWR) {
	        if (!(dmxdev->capabilities&DMXDEV_CAP_DUPLEX))
		        return -EOPNOTSUPP;
	  // not supported by Siemens card
	}

	if ((file->f_flags&O_ACCMODE)==O_RDONLY) {
	      dmxdev->dvr_buffer.size=DVR_BUFFER_SIZE;
	      dmxdev->dvr_buffer.data=vmalloc(DVR_BUFFER_SIZE);
	      if (!dmxdev->dvr_buffer.data)
	              return -ENOMEM;
	}

	if ((file->f_flags&O_ACCMODE)==O_WRONLY) {
	        dmxdev->dvr_orig_fe=dmxdev->demux->frontend;
		
		if (!dmxdev->demux->write)
		        return -EOPNOTSUPP;
		
		front=get_fe(dmxdev->demux, DMX_MEMORY_FE);
		
		if (!front)
		        return -EINVAL;
		dmxdev->demux->disconnect_frontend(dmxdev->demux);	
		dmxdev->demux->connect_frontend(dmxdev->demux, front);	
	}
        return 0;
}

int 
DmxDevDVRClose(dmxdev_t *dmxdev, struct file *file)
{
	if ((file->f_flags&O_ACCMODE)==O_WRONLY) {
	        dmxdev->demux->disconnect_frontend(dmxdev->demux);	
		dmxdev->demux->connect_frontend(dmxdev->demux, 	dmxdev->dvr_orig_fe);
	}
	if ((file->f_flags&O_ACCMODE)==O_RDONLY) {
		if (dmxdev->dvr_buffer.data) {
		        // delete data pointer first to prevent races
		        void *mem=dmxdev->dvr_buffer.data;
			dmxdev->dvr_buffer.data=0;
		        vfree(mem);
		}
	}
	return 0;
}


ssize_t 
DmxDevDVRWrite(dmxdev_t *dmxdev, struct file *file, 
             const char *buf, size_t count, loff_t *ppos)
{
        if (!dmxdev->demux->write)
	        return -EOPNOTSUPP;
        return dmxdev->demux->write(dmxdev->demux, buf, count);
}

ssize_t 
DmxDevDVRRead(dmxdev_t *dmxdev, struct file *file, 
	      char *buf, size_t count, loff_t *ppos)
{
        return DmxDevBufferRead(&dmxdev->dvr_buffer, 
				file->f_flags&O_NONBLOCK, 
				buf, count, ppos);
}

static int 
DmxDevFilterStop(dmxdev_filter_t *dmxdevfilter)
{
        if (dmxdevfilter->state<DMXDEV_STATE_GO)
                return 0;

	switch (dmxdevfilter->type) {
	case DMXDEV_TYPE_SEC:
	        del_timer(&dmxdevfilter->timer);
	        dmxdevfilter->feed.sec->stop_filtering(dmxdevfilter->feed.sec);
		break;
	case DMXDEV_TYPE_PES:
	        dmxdevfilter->feed.ts->stop_filtering(dmxdevfilter->feed.ts);
		break;
	default:
	        return -EINVAL;
	}
	dmxdevfilter->state=DMXDEV_STATE_READY;
        return 0;
}

static int 
DmxDevFilterReset(dmxdev_filter_t *dmxdevfilter)
{
	int i;
	dmxdev_t *dmxdev=dmxdevfilter->dev;

	DmxDevFilterStop(dmxdevfilter);

	switch (dmxdevfilter->type) {
	case DMXDEV_TYPE_SEC:
	        if (dmxdevfilter->filter.sec)
		        dmxdevfilter->feed.sec->
			  release_filter(dmxdevfilter->feed.sec,
					 dmxdevfilter->filter.sec);
		if (!dmxdevfilter->feed.sec)
		        break;
		for (i=0; i<dmxdev->filternum; i++)
		        if (dmxdev->filter[i].state>=DMXDEV_STATE_SET &&
			    &dmxdev->filter[i]!=dmxdevfilter &&
			    dmxdev->filter[i].pid==dmxdevfilter->pid)
				break;
		if (i==dmxdev->filternum)
		        dmxdevfilter->dev->demux->
			  release_section_feed(dmxdevfilter->dev->demux,
					       dmxdevfilter->feed.sec);
		dmxdevfilter->feed.sec=0;
		break;
	case DMXDEV_TYPE_PES:
	        if (!dmxdevfilter->feed.ts)
		        break;
	        dmxdevfilter->dev->demux->release_ts_feed(dmxdevfilter->dev->demux,
							  dmxdevfilter->feed.ts);
		dmxdevfilter->feed.ts=0;
		break;
	default:
	        return -EINVAL;
	}
	dmxdevfilter->buffer.pwrite=dmxdevfilter->buffer.pread=0;    
	dmxdevfilter->state=DMXDEV_STATE_ALLOCATED;
        return 0;
}

static int 
DmxDevSetBufferSize(dmxdev_filter_t *dmxdevfilter, unsigned long size)
{
        DmxDevFilterStop(dmxdevfilter);

        if (dmxdevfilter->buffer.data)
	        vfree(dmxdevfilter->buffer.data);
	
	dmxdevfilter->buffer.data=0;
	dmxdevfilter->buffer.size=size;
	dmxdevfilter->buffer.pwrite=dmxdevfilter->buffer.pread=0;    
		
        if (dmxdevfilter->state==DMXDEV_STATE_READY) {
                dmxdevfilter->buffer.data=vmalloc(dmxdevfilter->buffer.size);
		if (!dmxdevfilter->buffer.data)
		        return -ENOMEM;
	}

	return 0;
}



static void
DmxDevFilterTimeout(unsigned long data)
{
        dmxdev_filter_t *dmxdevfilter=(dmxdev_filter_t *)data;
	
	dmxdevfilter->buffer.error=-ETIMEDOUT;
	DmxDevFilterStop(dmxdevfilter);  //FIXME: think about semaphores
	wake_up(&dmxdevfilter->buffer.queue);
}

static void
DmxDevFilterTimer(dmxdev_filter_t *dmxdevfilter)
{
        struct dmxSctFilterParams *para=&dmxdevfilter->params.sec;
  
	del_timer(&dmxdevfilter->timer);
	if (para->timeout) {
	        dmxdevfilter->timer.function=DmxDevFilterTimeout;
		dmxdevfilter->timer.data=(unsigned long) dmxdevfilter;
		dmxdevfilter->timer.expires=jiffies+1+(HZ/2+HZ*para->timeout)/1000;
		add_timer(&dmxdevfilter->timer);
	}
}

static int 
DmxDevFilterStart(dmxdev_filter_t *dmxdevfilter)
{

        //printk ("function : %s\n", __FUNCTION__);

        if (dmxdevfilter->state<DMXDEV_STATE_SET) 
	        return -EINVAL;
        if (dmxdevfilter->state>=DMXDEV_STATE_GO)
	        DmxDevFilterStop(dmxdevfilter); //or return 0 ??
        if (dmxdevfilter->state==DMXDEV_STATE_SET) {
	        if (dmxdevfilter->buffer.data)
		        vfree(dmxdevfilter->buffer.data);
                dmxdevfilter->buffer.data=vmalloc(dmxdevfilter->buffer.size);
                if (!dmxdevfilter->buffer.data)
                        return -ENOMEM;
                dmxdevfilter->state=DMXDEV_STATE_READY;
        }

	switch (dmxdevfilter->type) {
	case DMXDEV_TYPE_SEC:
	        dmxdevfilter->todo=0;
	        if (dmxdevfilter->feed.sec->is_filtering)
		        dmxdevfilter->feed.sec->
			  stop_filtering(dmxdevfilter->feed.sec);
	        dmxdevfilter->feed.sec->
		  start_filtering(dmxdevfilter->feed.sec);
	        DmxDevFilterTimer(dmxdevfilter);
		break;
	case DMXDEV_TYPE_PES:
	        dmxdevfilter->feed.ts->start_filtering(dmxdevfilter->feed.ts);
		break;
	default:
	        return -EINVAL;
	}
        dmxdevfilter->state=DMXDEV_STATE_GO;
        return 0;
}

int 
DmxDevFilterAlloc(dmxdev_t *dmxdev, struct file *file)
{
        int i;
        dmxdev_filter_t *dmxdevfilter;
        
        for (i=0; i<dmxdev->filternum; i++)
                if (dmxdev->filter[i].state==DMXDEV_STATE_FREE)
                        break;
        if (i==dmxdev->filternum)
                return -EMFILE;
        dmxdevfilter=&dmxdev->filter[i];
	file->private_data=dmxdevfilter;

	DmxDevBufferInit(&dmxdevfilter->buffer);
        dmxdevfilter->type=0;
        dmxdevfilter->state=DMXDEV_STATE_ALLOCATED;
	dmxdevfilter->feed.ts=0;
	init_timer(&dmxdevfilter->timer);

        return 0;
}

int 
DmxDevFilterFree(dmxdev_t *dmxdev, struct file *file)
{
        dmxdev_filter_t *dmxdevfilter;
        
        //printk ("function : %s\n", __FUNCTION__);
        dmxdevfilter=DmxDevFile2Filter(dmxdev, file);
        if (!dmxdevfilter) 
                return -EINVAL;
        
        DmxDevFilterReset(dmxdevfilter);
        
        if (dmxdevfilter->buffer.data) {
	        void *mem=dmxdevfilter->buffer.data;
		dmxdevfilter->buffer.data=0;
		vfree(mem);
	}
        dmxdevfilter->state=DMXDEV_STATE_FREE;

        return 0;
}


static int 
DmxDevSectionCallback(__u8 *buffer1, size_t buffer1_len,
		      __u8 *buffer2, size_t buffer2_len,
		      dmx_section_filter_t *filter,
		      dmx_success_t success)
{
        dmxdev_filter_t *dmxdevfilter=(dmxdev_filter_t *) filter->priv;
        int ret;
        
	if (dmxdevfilter->buffer.error)
	        return 0;
	del_timer(&dmxdevfilter->timer);
        ret=DmxDevBufferWrite(&dmxdevfilter->buffer, buffer1, buffer1_len);
        if (ret==buffer1_len) {
	        ret=DmxDevBufferWrite(&dmxdevfilter->buffer, buffer2, buffer2_len);
	}
        if (ret<0) {
	        dmxdevfilter->buffer.pwrite=dmxdevfilter->buffer.pread;    
	        dmxdevfilter->buffer.error=-EOVERFLOW;  //FIXME: EBUFFEROVERFLOW=??
	}
	if (dmxdevfilter->params.sec.flags&DMX_ONESHOT)
	        DmxDevFilterStop(dmxdevfilter); //FIXME: semaphores, locks?
	wake_up(&dmxdevfilter->buffer.queue);
	return 0;
}

static int 
DmxDevFilterSet(dmxdev_t *dmxdev, 
                dmxdev_filter_t *dmxdevfilter)
{
        struct dmxSctFilterParams *para=&dmxdevfilter->params.sec;
        dmx_section_filter_t **secfilter=&dmxdevfilter->filter.sec;
        dmx_section_feed_t **secfeed=&dmxdevfilter->feed.sec;
        int ret, i;
                
        if (dmxdevfilter->state>=DMXDEV_STATE_SET)
	        DmxDevFilterReset(dmxdevfilter);
        
        dmxdevfilter->type=DMXDEV_TYPE_SEC;
        dmxdevfilter->pid=para->pid;
	dmxdevfilter->filter.sec=0;
	dmxdevfilter->feed.sec=0;

        for (i=0; i<dmxdev->filternum; i++) 
	        if (dmxdev->filter[i].state>=DMXDEV_STATE_SET &&
		    dmxdev->filter[i].pid==para->pid) {
		        *secfeed=dmxdev->filter[i].feed.sec;
			break;
		}
        dmxdevfilter->state=DMXDEV_STATE_SET;
	if (i==dmxdev->filternum) {
	        ret=dmxdev->demux->allocate_section_feed(dmxdev->demux, 
							 secfeed, 
							 DmxDevSectionCallback);
		if (ret<0)
		        return ret;
	}
	ret=(*secfeed)->set(*secfeed, para->pid, 32768, 0, 
			    (para->flags & DMX_CHECK_CRC) ? 1 : 0);
	if (ret<0)
	        return ret;

        ret=(*secfeed)->allocate_filter(*secfeed, secfilter);
        if (ret<0)
                return ret;

        (*secfilter)->priv=(void *) dmxdevfilter;
        
        memcpy(&((*secfilter)->filter_value[3]), 
	       &(para->filter.filter[1]), DMX_FILTER_SIZE-1);
        memcpy(&(*secfilter)->filter_mask[3], 
	       &para->filter.mask[1], DMX_FILTER_SIZE-1);
        (*secfilter)->filter_value[0]=para->filter.filter[0];
        (*secfilter)->filter_mask[0]=para->filter.mask[0];
        (*secfilter)->filter_mask[1]=0;
        (*secfilter)->filter_mask[2]=0;

        if (para->flags&DMX_IMMEDIATE_START) 
                return DmxDevFilterStart(dmxdevfilter);

        return 0;
}

int 
DmxDevInit(dmxdev_t *dmxdev)
{
        int i;

        if (dmxdev->filternum>DMXDEV_FILTER_MAX)
	        dmxdev->filternum=DMXDEV_FILTER_MAX;

	for (i=0; i<dmxdev->filternum; i++) {
                dmxdev->filter[i].state=DMXDEV_STATE_FREE;
                dmxdev->filter[i].dev=dmxdev;
                dmxdev->filter[i].buffer.data=0;
	}

	DmxDevBufferInit(&dmxdev->dvr_buffer);
	return 0;
}

void 
DmxDevRelease(dmxdev_t *dmxdev)
{
  
}

static int 
DmxDevTSCallback(__u8 *buffer1, size_t buffer1_len,
		  __u8 *buffer2, size_t buffer2_len,
		  dmx_ts_feed_t *feed,
		  dmx_success_t success)
{
        dmxdev_filter_t *dmxdevfilter=(dmxdev_filter_t *) feed->priv;
	dmxdev_buffer_t *buffer;
        int ret;
        
        if ((buffer1_len + buffer2_len)%188)
          printk("strange, callback with %d bytes (%d+%d)\n", buffer1_len + buffer2_len, buffer1_len, buffer2_len);
        
        //printk ("function : %s\n", __FUNCTION__);
	// Hmm, this should not happen
	if (dmxdevfilter->params.pes.output==DMX_OUT_DECODER)
	        return 0;

	if (dmxdevfilter->params.pes.output==DMX_OUT_TAP)
	        buffer=&dmxdevfilter->buffer;
	else
	        buffer=&dmxdevfilter->dev->dvr_buffer;

	if (buffer->error)
	        return 0;
        ret=DmxDevBufferWrite(buffer, buffer1, buffer1_len);
        if (ret==buffer1_len) 
	        ret=DmxDevBufferWrite(buffer, buffer2, buffer2_len);
        if (ret<0) {
	        buffer->pwrite=buffer->pread;    
	        buffer->error=-EOVERFLOW;  //FIXME: EBUFFEROVERFLOW=??
	}
	wake_up(&buffer->queue);
	return 0;
}

static int 
DmxDevPesFilterSet(dmxdev_t *dmxdev,
                   dmxdev_filter_t *dmxdevfilter,
                   struct dmxPesFilterParams *npara)
{
        struct timespec timeout = {0 };
        struct dmxPesFilterParams *para=&dmxdevfilter->params.pes;
	dmxOutput_t otype;
	int ret;
	int ts_type;
	dmx_ts_pes_t ts_pes;
	dmx_ts_feed_t **tsfeed=&dmxdevfilter->feed.ts;
        
        //printk ("function : %s\n", __FUNCTION__);

        if (dmxdevfilter->state>=DMXDEV_STATE_SET)
	        DmxDevFilterReset(dmxdevfilter);

        dmxdevfilter->type=DMXDEV_TYPE_PES;
	dmxdevfilter->feed.ts=0;

        memcpy(para, npara, sizeof(struct dmxPesFilterParams));
	otype=para->output;

	if (para->pesType>DMX_PES_OTHER || para->pesType<0)
	        return -EINVAL;

	ts_pes=(dmx_ts_pes_t) para->pesType;
	
	// only send PES types !=DMX_PES_OTHER to decoder
	if (ts_pes<DMX_PES_OTHER) 
	        ts_type=TS_DECODER;
	else
	        ts_type=0;
	
	// in case of TS_TAP send TS packets
	if (otype==DMX_OUT_TS_TAP) 
	        ts_type|=TS_PACKET;

	// in case of simple TAP only send the payload (is this as intended by API??)
	if (otype==DMX_OUT_TAP) 
	        ts_type|=TS_PAYLOAD_ONLY|TS_PACKET;

	ret=dmxdev->demux->allocate_ts_feed(dmxdev->demux, 
					    tsfeed, 
					    DmxDevTSCallback, ts_type, ts_pes);
	if (ret<0)
	  return ret;

	(*tsfeed)->priv=(void *) dmxdevfilter;
	ret=(*tsfeed)->set(*tsfeed, para->pid, 188, 32768, 0, timeout);
	if (ret<0)
	        return ret;
		
	if ((ts_type&TS_DECODER) && (*tsfeed)->set_type)
	        (*tsfeed)->set_type(*tsfeed, ts_type, ts_pes); 
	
        dmxdevfilter->pid=para->pid;
	dmxdevfilter->state=DMXDEV_STATE_SET;
        if (para->flags&DMX_IMMEDIATE_START) 
                return DmxDevFilterStart(dmxdevfilter);

        return 0;
}

static ssize_t 
DmxDevReadSec(dmxdev_filter_t *dfil, struct file *file, 
	      char *buf, size_t count, loff_t *ppos)
{
        int result, hcount;
	int done=0;
	
	/* the following is necessary because the API states sections should be 
	   read one section at a time, even if the target buffer could hold more */
	if (dfil->todo<=0) {
	        hcount=3+dfil->todo;
	        if (hcount>count)
		        hcount=count;
		result=DmxDevBufferRead(&dfil->buffer, file->f_flags&O_NONBLOCK, 
					buf, hcount, ppos);
		if (result<0)
		        return result;
		// read back section header from user, ugly isn´t it ...
		if (copy_from_user(dfil->secheader-dfil->todo, buf, result)) 
		        return -EFAULT;
		buf+=result;
		done=result;
		count-=result;
		dfil->todo-=result;
		if (dfil->todo>-3)
		        return done;
		dfil->todo=((dfil->secheader[1]<<8)|dfil->secheader[2])&0xfff;
		//printk("got section with len %d\n", dfil->todo);
		if (!count)
		        return done;
	}
	if (count>dfil->todo)
	        count=dfil->todo;
        result=DmxDevBufferRead(&dfil->buffer, file->f_flags&O_NONBLOCK, 
				buf, count, ppos);
	if (result<0)
	        return result;
	dfil->todo-=result;
	return (result+done);
}


ssize_t 
DmxDevRead(dmxdev_t *dmxdev, struct file *file, 
	   char *buf, size_t count, loff_t *ppos)
{
        dmxdev_filter_t *dmxdevfilter=DmxDevFile2Filter(dmxdev, file);

	if (dmxdevfilter->type==DMXDEV_TYPE_SEC)
	        return DmxDevReadSec(dmxdevfilter, file, buf, count, ppos);

        return DmxDevBufferRead(&dmxdevfilter->buffer, file->f_flags&O_NONBLOCK, 
				buf, count, ppos);
}


int DmxDevIoctl(dmxdev_t *dmxdev, struct file *file, 
		unsigned int cmd, unsigned long arg)
{
        void *parg=(void *)arg;

        dmxdev_filter_t *dmxdevfilter=DmxDevFile2Filter(dmxdev, file);
  
	if (!dmxdevfilter)
	        return -EINVAL;

	switch (cmd) {
	case DMX_START: 
	{
	        if (dmxdevfilter->state<2)
                                return -EINVAL;

		return DmxDevFilterStart(dmxdevfilter);
	}

	case DMX_STOP: 
		return DmxDevFilterStop(dmxdevfilter);

	case DMX_SET_FILTER: 
	{
	        if(copy_from_user(&dmxdevfilter->params.sec,
				  parg, sizeof(struct dmxSctFilterParams)))
		        return -EFAULT;
                        
		return DmxDevFilterSet(dmxdev, dmxdevfilter);
	}

	case DMX_SET_PES_FILTER: 
	{
	        struct dmxPesFilterParams npara;
		
		if(copy_from_user(&npara, parg, sizeof(npara)))
		        return -EFAULT;

		return DmxDevPesFilterSet(dmxdev, dmxdevfilter, &npara);
	}

	case DMX_SET_BUFFER_SIZE: 
	        return DmxDevSetBufferSize(dmxdevfilter, arg);
        
        case DMX_GET_EVENT: 
	        break;;
		
	default:
	        return -EINVAL;
	}
	return 0;
}

unsigned int 
DmxDevPoll(dmxdev_t *dmxdev, struct file *file, poll_table * wait)
{
        dmxdev_filter_t *dmxdevfilter=DmxDevFile2Filter(dmxdev, file);

	if (!dmxdevfilter)
	        return -EINVAL;
	
	if (dmxdevfilter->state<DMXDEV_STATE_GO)
	        return 0;

	if (dmxdevfilter->buffer.pread!=dmxdevfilter->buffer.pwrite)
	        return (POLLIN | POLLRDNORM | POLLPRI);

	poll_wait(file, &dmxdevfilter->buffer.queue, wait);
                
	if (dmxdevfilter->buffer.pread!=dmxdevfilter->buffer.pwrite)
	        return (POLLIN | POLLRDNORM | POLLPRI);
	return 0;
}


#ifdef MODULE
#ifdef EXPORT_SYMTAB
EXPORT_SYMBOL(DmxDevInit);

EXPORT_SYMBOL(DmxDevDVROpen);
EXPORT_SYMBOL(DmxDevDVRClose);
EXPORT_SYMBOL(DmxDevDVRRead);
EXPORT_SYMBOL(DmxDevDVRWrite);

EXPORT_SYMBOL(DmxDevFilterAlloc);
EXPORT_SYMBOL(DmxDevFilterFree);
EXPORT_SYMBOL(DmxDevRead);
EXPORT_SYMBOL(DmxDevIoctl);
EXPORT_SYMBOL(DmxDevPoll);
#endif
#endif

