/* 
 * dvb_demux.c - DVB kernel demux API
 *
 * Copyright (C) 2000-2001 Ralph  Metzler <ralph@convergence.de>
 *                       & Marcus Metzler <marcus@convergence.de>
 *                         for convergence integrated media GmbH
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
#include <linux/version.h>
#include <asm/uaccess.h>

#include "compat.h"
#include "dvb_demux.h"

#define NOBUFS  
#define DVB_CRC_SEED (~0)

LIST_HEAD(dmx_muxs);

int dmx_register_demux(dmx_demux_t *demux) 
{
	struct list_head *pos, *head=&dmx_muxs;
	
	if (!(demux->id && demux->vendor && demux->model)) 
	        return -EINVAL;
	list_for_each(pos, head) 
	{
	        if (!strcmp(DMX_DIR_ENTRY(pos)->id, demux->id))
		        return -EEXIST;
	}
	demux->users=0;
	list_add(&(demux->reg_list), head);
	MOD_INC_USE_COUNT;
	return 0;
}

int dmx_unregister_demux(dmx_demux_t* demux)
{
	struct list_head *pos, *n, *head=&dmx_muxs;

	list_for_each_safe (pos, n, head) 
	{
	        if (DMX_DIR_ENTRY(pos)==demux) 
		{
		        if (demux->users>0)
			        return -EINVAL;
		        list_del(pos);
		        MOD_DEC_USE_COUNT;
			return 0;
		}
	}
	return -ENODEV;
}


struct list_head *dmx_get_demuxes(void)
{
        if (list_empty(&dmx_muxs))
	        return NULL;

	return &dmx_muxs;
}

/******************************************************************************
 * static inlined helper functions
 ******************************************************************************/

static inline u16 
section_length(const u8 *buf)
{
        return 3+((buf[1]&0x0f)<<8)+buf[2];
}

static inline u16 
ts_pid(const u8 *buf)
{
        return ((buf[1]&0x1f)<<8)+buf[2];
}

static inline int
payload(const u8 *tsp)
{
        if (!(tsp[3]&0x10)) // no payload?
                return 0;
        if (tsp[3]&0x20) {  // adaptation field?
		if (tsp[4]>183)    // corrupted data?
			return 0;
		else
			return 184-1-tsp[4];
	}
        return 184;
}

void dvb_set_crc32(u8 *data, int length)
{
        u32 crc;

        crc = crc32_le(DVB_CRC_SEED, data, length);

	data[length]   = (crc >> 24) & 0xff;
        data[length+1] = (crc >> 16) & 0xff;
        data[length+2] = (crc >>  8) & 0xff;
        data[length+3] = (crc)       & 0xff;
}


static
u32 dvb_dmx_crc32 (struct dvb_demux_feed *dvbdmxfeed,
		   const u8 *src, size_t len)
{
	return (dvbdmxfeed->feed.sec.crc_val = crc32_le (dvbdmxfeed->feed.sec.crc_val, src, len));
}


static
void dvb_dmx_memcopy (struct dvb_demux_feed *dvbdmxfeed, u8 *dst,
		      const u8 *src, size_t len)
{
	memcpy (dst, src, len);
}


/******************************************************************************
 * Software filter functions
 ******************************************************************************/

static inline int
dvb_dmx_swfilter_payload(struct dvb_demux_feed *dvbdmxfeed, const u8 *buf) 
{
        int p, count;
	//int ccok;
	//u8 cc;

        if (!(count=payload(buf)))
                return -1;
        p=188-count;
        /*
	cc=buf[3]&0x0f;
        ccok=((dvbdmxfeed->cc+1)&0x0f)==cc ? 1 : 0;
        dvbdmxfeed->cc=cc;
        if (!ccok)
          printk("missed packet!\n");
        */
        if (buf[1]&0x40)  // PUSI ?
                dvbdmxfeed->peslen=0xfffa;
        dvbdmxfeed->peslen+=count;

        return dvbdmxfeed->cb.ts((u8 *)&buf[p], count, 0, 0, 
                                 &dvbdmxfeed->feed.ts, DMX_OK); 
}


static int
dvb_dmx_swfilter_sectionfilter(struct dvb_demux_feed *dvbdmxfeed, 
                            struct dvb_demux_filter *f)
{
        dmx_section_filter_t *filter=&f->filter;
        int i;
	u8 xor, neq=0;

        for (i=0; i<DVB_DEMUX_MASK_MAX; i++) {
		xor=filter->filter_value[i]^dvbdmxfeed->feed.sec.secbuf[i];
		if (f->maskandmode[i]&xor)
			return 0;
		neq|=f->maskandnotmode[i]&xor;
	}

	if (f->doneq & !neq)
		return 0;

        return dvbdmxfeed->cb.sec(dvbdmxfeed->feed.sec.secbuf,
				  dvbdmxfeed->feed.sec.seclen, 
                                  0, 0, filter, DMX_OK); 
}

static inline int
dvb_dmx_swfilter_section_feed(struct dvb_demux_feed *dvbdmxfeed)
{
	struct dvb_demux *demux = dvbdmxfeed->demux;
	dmx_section_feed_t *sec = &dvbdmxfeed->feed.sec;
        u8 *buf = sec->secbuf;
        struct dvb_demux_filter *f;

        if (sec->secbufp != sec->seclen)
                return -1;

	if (!sec->is_filtering)
                return 0;

	if (!(f = dvbdmxfeed->filter))
                return 0;

	if (sec->check_crc &&
	    demux->check_crc32(dvbdmxfeed, sec->secbuf, sec->seclen))
		return -1;

	do {
                if (dvb_dmx_swfilter_sectionfilter(dvbdmxfeed, f)<0)
                        return -1;
	} while ((f = f->next) && sec->is_filtering);

        sec->secbufp = sec->seclen = 0;

	memset(buf, 0, DVB_DEMUX_MASK_MAX);
 
	return 0;
}


static
int dvb_dmx_swfilter_section_packet(struct dvb_demux_feed *feed, const u8 *buf) 
{
	struct dvb_demux *demux = feed->demux;
	dmx_section_feed_t *sec = &feed->feed.sec;
        int p, count;
        int ccok, rest;
	u8 cc;

        if (!(count = payload(buf)))
                return -1;

	p = 188-count;

	cc = buf[3]&0x0f;
        ccok = ((feed->cc+1)&0x0f)==cc ? 1 : 0;
        feed->cc = cc;

        if (buf[1] & 0x40) { // PUSI set
                // offset to start of first section is in buf[p] 
		if (p+buf[p]>187) // trash if it points beyond packet
			return -1;

		if (buf[p] && ccok) { // rest of previous section?
                        // did we have enough data in last packet to calc length?
			int tmp = 3 - sec->secbufp;

			if (tmp > 0 && tmp != 3) {
				if (p + tmp >= 187)
					return -1;

                                demux->memcopy (feed, sec->secbuf+sec->secbufp,
					       buf+p+1, tmp);

				sec->seclen = section_length(sec->secbuf);

				if (sec->seclen > 4096) 
					return -1;
                        }

                        rest = sec->seclen - sec->secbufp;

                        if (rest == buf[p] && sec->seclen) {
				demux->memcopy (feed, sec->secbuf + sec->secbufp,
	                                       buf+p+1, buf[p]);
                                sec->secbufp += buf[p];
                                dvb_dmx_swfilter_section_feed(feed);
                        }
                }

                p += buf[p] + 1; 		// skip rest of last section
                count = 188 - p;

		while (count) {

			sec->crc_val = DVB_CRC_SEED;

                        if ((count>2) && // enough data to determine sec length?
                            ((sec->seclen = section_length(buf+p)) <= count)) {
				if (sec->seclen>4096) 
					return -1;
					
				demux->memcopy (feed, sec->secbuf, buf+p,
					       sec->seclen);

				sec->secbufp = sec->seclen;
                                p += sec->seclen;
                                count = 188 - p;

				dvb_dmx_swfilter_section_feed(feed);

                                // filling bytes until packet end?
                                if (count && buf[p]==0xff) 
                                        count=0;

			} else { // section continues to following TS packet
                                demux->memcopy(feed, sec->secbuf, buf+p, count);
                                sec->secbufp+=count;
                                count=0;
                        }
                }

		return 0;
	}

	// section continued below
	if (!ccok)
		return -1;

	if (!sec->secbufp) // any data in last ts packet?
		return -1;

	// did we have enough data in last packet to calc section length?
	if (sec->secbufp<3) {
		int tmp = 3 - sec->secbufp;
		
		if (tmp>count)
			return -1;

		sec->crc_val = DVB_CRC_SEED;

		demux->memcopy (feed, sec->secbuf + sec->secbufp, buf+p, tmp);

		sec->seclen = section_length(sec->secbuf);

		if (sec->seclen > 4096) 
			return -1;
	}

	rest = sec->seclen - sec->secbufp;

	if (rest < 0)
		return -1;

	if (rest <= count) {	// section completed in this TS packet
		demux->memcopy (feed, sec->secbuf + sec->secbufp, buf+p, rest);
		sec->secbufp += rest;
		dvb_dmx_swfilter_section_feed(feed);
	} else 	{	// section continues in following ts packet
		demux->memcopy (feed, sec->secbuf + sec->secbufp, buf+p, count);
		sec->secbufp += count;
	}

        return 0;
}


static inline void 
dvb_dmx_swfilter_packet_type(struct dvb_demux_feed *dvbdmxfeed, const u8 *buf)
{
        switch(dvbdmxfeed->type) {
        case DMX_TYPE_TS:
                if (!dvbdmxfeed->feed.ts.is_filtering)
                        break;
                if (dvbdmxfeed->ts_type & TS_PACKET) {
                        if (dvbdmxfeed->ts_type & TS_PAYLOAD_ONLY)
                                dvb_dmx_swfilter_payload(dvbdmxfeed, buf);
                        else
                                dvbdmxfeed->cb.ts((u8 *)buf, 188, 0, 0, 
                                                  &dvbdmxfeed->feed.ts, DMX_OK); 
                }
                if (dvbdmxfeed->ts_type & TS_DECODER) 
                        if (dvbdmxfeed->demux->write_to_decoder)
                                dvbdmxfeed->demux->
                                  write_to_decoder(dvbdmxfeed, (u8 *)buf, 188);
                break;

        case DMX_TYPE_SEC:
                if (!dvbdmxfeed->feed.sec.is_filtering)
                        break;
                if (dvb_dmx_swfilter_section_packet(dvbdmxfeed, buf)<0)
                        dvbdmxfeed->feed.sec.seclen=dvbdmxfeed->feed.sec.secbufp=0;
                break;

        default:
                break;
        }
}

void
dvb_dmx_swfilter_packet(struct dvb_demux *dvbdmx, const u8 *buf)
{
        struct dvb_demux_feed *dvbdmxfeed;

        if (!(dvbdmxfeed=dvbdmx->pid2feed[ts_pid(buf)]))
                return;

        dvb_dmx_swfilter_packet_type(dvbdmxfeed, buf);
}

void
dvb_dmx_swfilter_packets(struct dvb_demux *dvbdmx, const u8 *buf, int count)
{
        struct dvb_demux_feed *dvbdmxfeed;

	spin_lock(&dvbdmx->lock);
	if ((dvbdmxfeed=dvbdmx->pid2feed[0x2000]))
		dvbdmxfeed->cb.ts((u8 *)buf, count*188, 0, 0, 
				  &dvbdmxfeed->feed.ts, DMX_OK); 
        while (count) {
		dvb_dmx_swfilter_packet(dvbdmx, buf);
		count--;
		buf+=188;
	}
	spin_unlock(&dvbdmx->lock);
}

static inline void
dvb_dmx_swfilter(struct dvb_demux *dvbdmx, const u8 *buf, size_t count)
{
        int p=0,i, j;
        
        if ((i=dvbdmx->tsbufp)) {
                if (count<(j=188-i)) {
                        memcpy(&dvbdmx->tsbuf[i], buf, count);
                        dvbdmx->tsbufp+=count;
                        return;
                }
                memcpy(&dvbdmx->tsbuf[i], buf, j);
                dvb_dmx_swfilter_packet(dvbdmx, dvbdmx->tsbuf);
                dvbdmx->tsbufp=0;
                p+=j;
        }
        
        while (p<count) {
                if (buf[p]==0x47) {
                        if (count-p>=188) {
                                dvb_dmx_swfilter_packet(dvbdmx, buf+p);
                                p+=188;
                        } else {
                                i=count-p;
                                memcpy(dvbdmx->tsbuf, buf+p, i);
                                dvbdmx->tsbufp=i;
				return;
                        }
                } else 
                        p++;
        }
}


/******************************************************************************
 ******************************************************************************
 * DVB DEMUX API LEVEL FUNCTIONS
 ******************************************************************************
 ******************************************************************************/

static struct dvb_demux_filter *
dvb_dmx_filter_alloc(struct dvb_demux *dvbdmx)
{
        int i;

        for (i=0; i<dvbdmx->filternum; i++)
                if (dvbdmx->filter[i].state==DMX_STATE_FREE)
                        break;
        if (i==dvbdmx->filternum)
                return 0;
        dvbdmx->filter[i].state=DMX_STATE_ALLOCATED;
        return &dvbdmx->filter[i];
}

static struct dvb_demux_feed *
dvb_dmx_feed_alloc(struct dvb_demux *dvbdmx)
{
        int i;

        for (i=0; i<dvbdmx->feednum; i++)
                if (dvbdmx->feed[i].state==DMX_STATE_FREE)
                        break;
        if (i==dvbdmx->feednum)
                return 0;
        dvbdmx->feed[i].state=DMX_STATE_ALLOCATED;
        return &dvbdmx->feed[i];
}


/******************************************************************************
 * dmx_ts_feed API calls
 ******************************************************************************/

static int 
dmx_pid_set(u16 pid, struct dvb_demux_feed *dvbdmxfeed)
{
        struct dvb_demux *dvbdmx=dvbdmxfeed->demux;
	struct dvb_demux_feed **pid2feed=dvbdmx->pid2feed;

	if (pid>DMX_MAX_PID)
                return -EINVAL;
        if (dvbdmxfeed->pid!=0xffff) {
                if (dvbdmxfeed->pid<=DMX_MAX_PID)
                        pid2feed[dvbdmxfeed->pid]=0;
                dvbdmxfeed->pid=0xffff;
        }
        if (pid2feed[pid]) {
                return -EBUSY;
	}
        pid2feed[pid]=dvbdmxfeed;
        dvbdmxfeed->pid=pid;
	return 0;
}


static int 
dmx_ts_feed_set(struct dmx_ts_feed_s* feed,
                u16 pid,
		int ts_type, 
		dmx_ts_pes_t pes_type,
		size_t callback_length, 
                size_t circular_buffer_size, 
                int descramble, 
                struct timespec timeout
                )
{
        struct dvb_demux_feed *dvbdmxfeed=(struct dvb_demux_feed *) feed;
        struct dvb_demux *dvbdmx=dvbdmxfeed->demux;
	int ret;
	
        if (down_interruptible (&dvbdmx->mutex))
		return -ERESTARTSYS;

	if (ts_type & TS_DECODER) {
                if (pes_type >= DMX_TS_PES_OTHER) {
			up(&dvbdmx->mutex);
                        return -EINVAL;
		}
                if (dvbdmx->pesfilter[pes_type] && 
                    (dvbdmx->pesfilter[pes_type]!=dvbdmxfeed)) {
			up(&dvbdmx->mutex);
                        return -EINVAL;
		}
		if ((pes_type != DMX_TS_PES_PCR0) && 
		    (pes_type != DMX_TS_PES_PCR1) && 
		    (pes_type != DMX_TS_PES_PCR2) && 
		    (pes_type != DMX_TS_PES_PCR3)) {
			if ((ret=dmx_pid_set(pid, dvbdmxfeed))<0) {
				up(&dvbdmx->mutex);
				return ret;
			}
		} else
			dvbdmxfeed->pid=pid;
				
                dvbdmx->pesfilter[pes_type]=dvbdmxfeed;
	        dvbdmx->pids[pes_type]=dvbdmxfeed->pid;
        } else
		if ((ret=dmx_pid_set(pid, dvbdmxfeed))<0) {
			up(&dvbdmx->mutex);
			return ret;
		}

        dvbdmxfeed->buffer_size=circular_buffer_size;
        dvbdmxfeed->descramble=descramble;
        dvbdmxfeed->timeout=timeout;
        dvbdmxfeed->cb_length=callback_length;
        dvbdmxfeed->ts_type=ts_type;
        dvbdmxfeed->pes_type=pes_type;

        if (dvbdmxfeed->descramble) {
		up(&dvbdmx->mutex);
                return -ENOSYS;
	}

        if (dvbdmxfeed->buffer_size) {
#ifdef NOBUFS
                dvbdmxfeed->buffer=0;
#else
                dvbdmxfeed->buffer=vmalloc(dvbdmxfeed->buffer_size);
                if (!dvbdmxfeed->buffer) {
			up(&dvbdmx->mutex);
			return -ENOMEM;
		}
#endif
        }
        dvbdmxfeed->state=DMX_STATE_READY;
        up(&dvbdmx->mutex);
        return 0;
}

static int 
dmx_ts_feed_start_filtering(struct dmx_ts_feed_s* feed)
{
        struct dvb_demux_feed *dvbdmxfeed=(struct dvb_demux_feed *) feed;
        struct dvb_demux *dvbdmx=dvbdmxfeed->demux;
	int ret;

        if (down_interruptible (&dvbdmx->mutex))
		return -ERESTARTSYS;

        if (dvbdmxfeed->state!=DMX_STATE_READY ||
	    dvbdmxfeed->type!=DMX_TYPE_TS) {
		up(&dvbdmx->mutex);
                return -EINVAL;
	}
        if (!dvbdmx->start_feed) {
		up(&dvbdmx->mutex);
                return -1;
	}
        ret=dvbdmx->start_feed(dvbdmxfeed); 
        if (ret<0) {
		up(&dvbdmx->mutex);
		return ret;
	}
	spin_lock_irq(&dvbdmx->lock);
        feed->is_filtering=1;
        dvbdmxfeed->state=DMX_STATE_GO;
	spin_unlock_irq(&dvbdmx->lock);
        up(&dvbdmx->mutex);
	return 0;
}
 
static int 
dmx_ts_feed_stop_filtering(struct dmx_ts_feed_s* feed)
{
        struct dvb_demux_feed *dvbdmxfeed=(struct dvb_demux_feed *) feed;
        struct dvb_demux *dvbdmx=dvbdmxfeed->demux;
	int ret;

        if (down_interruptible (&dvbdmx->mutex))
		return -ERESTARTSYS;

        if (dvbdmxfeed->state<DMX_STATE_GO) {
		up(&dvbdmx->mutex);
                return -EINVAL;
	}
        if (!dvbdmx->stop_feed) {
		up(&dvbdmx->mutex);
                return -1;
	}
        ret=dvbdmx->stop_feed(dvbdmxfeed); 
	spin_lock_irq(&dvbdmx->lock);
        feed->is_filtering=0;
        dvbdmxfeed->state=DMX_STATE_ALLOCATED;

	spin_unlock_irq(&dvbdmx->lock);
        up(&dvbdmx->mutex);
        return ret;
}

static int dvbdmx_allocate_ts_feed(dmx_demux_t *demux,
                                   dmx_ts_feed_t **feed, 
                                   dmx_ts_cb callback)
{
        struct dvb_demux *dvbdmx=(struct dvb_demux *) demux;
        struct dvb_demux_feed *dvbdmxfeed;

        if (down_interruptible (&dvbdmx->mutex))
		return -ERESTARTSYS;

        if (!(dvbdmxfeed=dvb_dmx_feed_alloc(dvbdmx))) {
		up(&dvbdmx->mutex);
                return -EBUSY;
	}
        dvbdmxfeed->type=DMX_TYPE_TS;
        dvbdmxfeed->cb.ts=callback;
        dvbdmxfeed->demux=dvbdmx;
        dvbdmxfeed->pid=0xffff;
        dvbdmxfeed->peslen=0xfffa;
        dvbdmxfeed->buffer=0;

        (*feed)=&dvbdmxfeed->feed.ts;
        (*feed)->is_filtering=0;
        (*feed)->parent=demux;
        (*feed)->priv=0;
        (*feed)->set=dmx_ts_feed_set;
        (*feed)->start_filtering=dmx_ts_feed_start_filtering;
        (*feed)->stop_filtering=dmx_ts_feed_stop_filtering;


        if (!(dvbdmxfeed->filter=dvb_dmx_filter_alloc(dvbdmx))) {
                dvbdmxfeed->state=DMX_STATE_FREE;
		up(&dvbdmx->mutex);
                return -EBUSY;
        }

        dvbdmxfeed->filter->type=DMX_TYPE_TS;
        dvbdmxfeed->filter->feed=dvbdmxfeed;
        dvbdmxfeed->filter->state=DMX_STATE_READY;
        
        up(&dvbdmx->mutex);
        return 0;
}

static int dvbdmx_release_ts_feed(dmx_demux_t *demux, dmx_ts_feed_t *feed)
{
        struct dvb_demux *dvbdmx=(struct dvb_demux *) demux;
        struct dvb_demux_feed *dvbdmxfeed=(struct dvb_demux_feed *) feed;

        if (down_interruptible (&dvbdmx->mutex))
		return -ERESTARTSYS;

        if (dvbdmxfeed->state==DMX_STATE_FREE) {
		up(&dvbdmx->mutex);
                return -EINVAL;
	}
#ifndef NOBUFS
        if (dvbdmxfeed->buffer) { 
                vfree(dvbdmxfeed->buffer);
                dvbdmxfeed->buffer=0;
        }
#endif
        dvbdmxfeed->state=DMX_STATE_FREE;
        dvbdmxfeed->filter->state=DMX_STATE_FREE;
	if (dvbdmxfeed->pid<=DMX_MAX_PID) {
		dvbdmxfeed->demux->pid2feed[dvbdmxfeed->pid]=0;
                dvbdmxfeed->pid=0xffff;
        }
	
	if ((dvbdmxfeed->ts_type & TS_DECODER) &&
	    (dvbdmx->pesfilter[dvbdmxfeed->pes_type] == dvbdmxfeed))
		dvbdmx->pesfilter[dvbdmxfeed->pes_type] = NULL;

        up(&dvbdmx->mutex);
        return 0;
}


/******************************************************************************
 * dmx_section_feed API calls
 ******************************************************************************/

static int 
dmx_section_feed_allocate_filter(struct dmx_section_feed_s* feed, 
                                 dmx_section_filter_t** filter) 
{
        struct dvb_demux_feed *dvbdmxfeed=(struct dvb_demux_feed *) feed;
        struct dvb_demux *dvbdemux=dvbdmxfeed->demux;
        struct dvb_demux_filter *dvbdmxfilter;

	if (down_interruptible (&dvbdemux->mutex))
		return -ERESTARTSYS;

        dvbdmxfilter=dvb_dmx_filter_alloc(dvbdemux);
        if (!dvbdmxfilter) {
		up(&dvbdemux->mutex);
                return -ENOSPC;
	}
	spin_lock_irq(&dvbdemux->lock);
        *filter=&dvbdmxfilter->filter;
        (*filter)->parent=feed;
        (*filter)->priv=0;
        dvbdmxfilter->feed=dvbdmxfeed;
        dvbdmxfilter->type=DMX_TYPE_SEC;
        dvbdmxfilter->state=DMX_STATE_READY;

        dvbdmxfilter->next=dvbdmxfeed->filter;
        dvbdmxfeed->filter=dvbdmxfilter;
	spin_unlock_irq(&dvbdemux->lock);
        up(&dvbdemux->mutex);
        return 0;
}

static int 
dmx_section_feed_set(struct dmx_section_feed_s* feed, 
                     u16 pid, size_t circular_buffer_size, 
                     int descramble, int check_crc) 
{
        struct dvb_demux_feed *dvbdmxfeed=(struct dvb_demux_feed *) feed;
        struct dvb_demux *dvbdmx=dvbdmxfeed->demux;

        if (pid>0x1fff)
                return -EINVAL;

	if (down_interruptible (&dvbdmx->mutex))
		return -ERESTARTSYS;
	
        if (dvbdmxfeed->pid!=0xffff) {
                dvbdmx->pid2feed[dvbdmxfeed->pid]=0;
                dvbdmxfeed->pid=0xffff;
        }
        if (dvbdmx->pid2feed[pid]) {
		up(&dvbdmx->mutex);
		return -EBUSY;
	}
        dvbdmx->pid2feed[pid]=dvbdmxfeed;
        dvbdmxfeed->pid=pid;

        dvbdmxfeed->buffer_size=circular_buffer_size;
        dvbdmxfeed->descramble=descramble;
        if (dvbdmxfeed->descramble) {
		up(&dvbdmx->mutex);
                return -ENOSYS;
	}

        dvbdmxfeed->feed.sec.check_crc=check_crc;
#ifdef NOBUFS
        dvbdmxfeed->buffer=0;
#else
        dvbdmxfeed->buffer=vmalloc(dvbdmxfeed->buffer_size);
        if (!dvbdmxfeed->buffer) {
		up(&dvbdmx->mutex);
		return -ENOMEM;
	}
#endif
        dvbdmxfeed->state=DMX_STATE_READY;
        up(&dvbdmx->mutex);
        return 0;
}

static void prepare_secfilters(struct dvb_demux_feed *dvbdmxfeed)
{
	int i;
        dmx_section_filter_t *sf;
	struct dvb_demux_filter *f;
	u8 mask, mode, doneq;
		
	if (!(f=dvbdmxfeed->filter))
                return;
        do {
		sf=&f->filter;
		doneq=0;
		for (i=0; i<DVB_DEMUX_MASK_MAX; i++) {
			mode=sf->filter_mode[i];
			mask=sf->filter_mask[i];
			f->maskandmode[i]=mask&mode;
			doneq|=f->maskandnotmode[i]=mask&~mode;
		}
		f->doneq=doneq ? 1 : 0;
	} while ((f=f->next));
}


static int 
dmx_section_feed_start_filtering(dmx_section_feed_t *feed)
{
        struct dvb_demux_feed *dvbdmxfeed=(struct dvb_demux_feed *) feed;
        struct dvb_demux *dvbdmx=dvbdmxfeed->demux;
	int ret;

        if (down_interruptible (&dvbdmx->mutex))
		return -ERESTARTSYS;
	
        if (feed->is_filtering) {
		up(&dvbdmx->mutex);
		return -EBUSY;
	}
        if (!dvbdmxfeed->filter) {
		up(&dvbdmx->mutex);
                return -EINVAL;
	}
        dvbdmxfeed->feed.sec.secbufp=0;
        dvbdmxfeed->feed.sec.seclen=0;
        
        if (!dvbdmx->start_feed) {
		up(&dvbdmx->mutex);
                return -1;
	}
	prepare_secfilters(dvbdmxfeed);
        ret=dvbdmx->start_feed(dvbdmxfeed); 
	if (ret<0) {
		up(&dvbdmx->mutex);
		return ret;
	}
	spin_lock_irq(&dvbdmx->lock);
        feed->is_filtering=1;
        dvbdmxfeed->state=DMX_STATE_GO;
	spin_unlock_irq(&dvbdmx->lock);
        up(&dvbdmx->mutex);
	return 0;
}

static int 
dmx_section_feed_stop_filtering(struct dmx_section_feed_s* feed)
{
        struct dvb_demux_feed *dvbdmxfeed=(struct dvb_demux_feed *) feed;
        struct dvb_demux *dvbdmx=dvbdmxfeed->demux;
        int ret;

        if (down_interruptible (&dvbdmx->mutex))
		return -ERESTARTSYS;

        if (!dvbdmx->stop_feed) {
		up(&dvbdmx->mutex);
                return -1;
	}
	ret=dvbdmx->stop_feed(dvbdmxfeed); 
	spin_lock_irq(&dvbdmx->lock);
        dvbdmxfeed->state=DMX_STATE_READY;
        feed->is_filtering=0;
	spin_unlock_irq(&dvbdmx->lock);
        up(&dvbdmx->mutex);
	return ret;
}

static int 
dmx_section_feed_release_filter(dmx_section_feed_t *feed, 
                                dmx_section_filter_t* filter)
{
        struct dvb_demux_filter *dvbdmxfilter=(struct dvb_demux_filter *) filter, *f;
        struct dvb_demux_feed *dvbdmxfeed=(struct dvb_demux_feed *) feed;
        struct dvb_demux *dvbdmx=dvbdmxfeed->demux;

        if (down_interruptible (&dvbdmx->mutex))
		return -ERESTARTSYS;

        if (dvbdmxfilter->feed!=dvbdmxfeed) {
		up(&dvbdmx->mutex);
                return -EINVAL;
	}
        if (feed->is_filtering) 
                feed->stop_filtering(feed);
	
	spin_lock_irq(&dvbdmx->lock);
        f=dvbdmxfeed->filter;
        if (f==dvbdmxfilter)
                dvbdmxfeed->filter=dvbdmxfilter->next;
        else {
                while(f->next!=dvbdmxfilter)
                        f=f->next;
                f->next=f->next->next;
        }
        dvbdmxfilter->state=DMX_STATE_FREE;
	spin_unlock_irq(&dvbdmx->lock);
        up(&dvbdmx->mutex);
        return 0;
}

static int dvbdmx_allocate_section_feed(dmx_demux_t *demux, 
                                        dmx_section_feed_t **feed,
                                        dmx_section_cb callback)
{
        struct dvb_demux *dvbdmx=(struct dvb_demux *) demux;
        struct dvb_demux_feed *dvbdmxfeed;

        if (down_interruptible (&dvbdmx->mutex))
		return -ERESTARTSYS;

        if (!(dvbdmxfeed=dvb_dmx_feed_alloc(dvbdmx))) {
		up(&dvbdmx->mutex);
                return -EBUSY;
	}
        dvbdmxfeed->type=DMX_TYPE_SEC;
        dvbdmxfeed->cb.sec=callback;
        dvbdmxfeed->demux=dvbdmx;
        dvbdmxfeed->pid=0xffff;
        dvbdmxfeed->feed.sec.secbufp=0;
        dvbdmxfeed->filter=0;
        dvbdmxfeed->buffer=0;

        (*feed)=&dvbdmxfeed->feed.sec;
        (*feed)->is_filtering=0;
        (*feed)->parent=demux;
        (*feed)->priv=0;
        (*feed)->set=dmx_section_feed_set;
        (*feed)->allocate_filter=dmx_section_feed_allocate_filter;
        (*feed)->release_filter=dmx_section_feed_release_filter;
        (*feed)->start_filtering=dmx_section_feed_start_filtering;
        (*feed)->stop_filtering=dmx_section_feed_stop_filtering;

        up(&dvbdmx->mutex);
        return 0;
}

static int dvbdmx_release_section_feed(dmx_demux_t *demux, 
                                       dmx_section_feed_t *feed)
{
        struct dvb_demux_feed *dvbdmxfeed=(struct dvb_demux_feed *) feed;
        struct dvb_demux *dvbdmx=(struct dvb_demux *) demux;

        if (down_interruptible (&dvbdmx->mutex))
		return -ERESTARTSYS;

        if (dvbdmxfeed->state==DMX_STATE_FREE) {
		up(&dvbdmx->mutex);
                return -EINVAL;
	}
#ifndef NOBUFS
        if (dvbdmxfeed->buffer) {
                vfree(dvbdmxfeed->buffer);
                dvbdmxfeed->buffer=0;
        }
#endif
        dvbdmxfeed->state=DMX_STATE_FREE;
        dvbdmxfeed->demux->pid2feed[dvbdmxfeed->pid]=0;
        if (dvbdmxfeed->pid!=0xffff)
                dvbdmxfeed->demux->pid2feed[dvbdmxfeed->pid]=0;
        up(&dvbdmx->mutex);
        return 0;
}


/******************************************************************************
 * dvb_demux kernel data API calls
 ******************************************************************************/

static int dvbdmx_open(dmx_demux_t *demux)
{
        struct dvb_demux *dvbdemux=(struct dvb_demux *) demux;

        if (dvbdemux->users>=MAX_DVB_DEMUX_USERS)
                return -EUSERS;
        dvbdemux->users++;
        return 0;
}

static int dvbdmx_close(struct dmx_demux_s *demux)
{
        struct dvb_demux *dvbdemux=(struct dvb_demux *) demux;

        if (dvbdemux->users==0)
                return -ENODEV;
        dvbdemux->users--;
        //FIXME: release any unneeded resources if users==0
        return 0;
}

static int dvbdmx_write(dmx_demux_t *demux, const char *buf, size_t count)
{
        struct dvb_demux *dvbdemux=(struct dvb_demux *) demux;

        if ((!demux->frontend) ||
            (demux->frontend->source!=DMX_MEMORY_FE))
                return -EINVAL;

        if (down_interruptible (&dvbdemux->mutex))
		return -ERESTARTSYS;

        dvb_dmx_swfilter(dvbdemux, buf, count);
        up(&dvbdemux->mutex);
        return count;
}


static int dvbdmx_add_frontend(dmx_demux_t *demux, 
                               dmx_frontend_t *frontend)
{
        struct dvb_demux *dvbdemux=(struct dvb_demux *) demux;
        struct list_head *pos, *head=&dvbdemux->frontend_list;
	
	if (!(frontend->id && frontend->vendor && frontend->model)) 
	        return -EINVAL;
	list_for_each(pos, head) 
	{
	        if (!strcmp(DMX_FE_ENTRY(pos)->id, frontend->id))
		        return -EEXIST;
	}

	list_add(&(frontend->connectivity_list), head);
        return 0;
}

static int 
dvbdmx_remove_frontend(dmx_demux_t *demux, 
                       dmx_frontend_t *frontend)
{
        struct dvb_demux *dvbdemux=(struct dvb_demux *) demux;
        struct list_head *pos, *n, *head=&dvbdemux->frontend_list;

	list_for_each_safe (pos, n, head) 
	{
	        if (DMX_FE_ENTRY(pos)==frontend) 
                {
		        list_del(pos);
			return 0;
		}
	}
	return -ENODEV;
}

static struct list_head *
dvbdmx_get_frontends(dmx_demux_t *demux)
{
        struct dvb_demux *dvbdemux=(struct dvb_demux *) demux;

        if (list_empty(&dvbdemux->frontend_list))
	        return NULL;
        return &dvbdemux->frontend_list;
}

static int dvbdmx_connect_frontend(dmx_demux_t *demux, 
                                   dmx_frontend_t *frontend)
{
        struct dvb_demux *dvbdemux=(struct dvb_demux *) demux;

        if (demux->frontend)
                return -EINVAL;
        
        if (down_interruptible (&dvbdemux->mutex))
		return -ERESTARTSYS;

        demux->frontend=frontend;
        up(&dvbdemux->mutex);
        return 0;
}

static int dvbdmx_disconnect_frontend(dmx_demux_t *demux)
{
        struct dvb_demux *dvbdemux=(struct dvb_demux *) demux;

        if (down_interruptible (&dvbdemux->mutex))
		return -ERESTARTSYS;

        demux->frontend=NULL;
        up(&dvbdemux->mutex);
        return 0;
}

static int dvbdmx_get_pes_pids(dmx_demux_t *demux, u16 *pids)
{
        struct dvb_demux *dvbdemux=(struct dvb_demux *) demux;

        memcpy(pids, dvbdemux->pids, 5*sizeof(u16));
        return 0;
}

int 
dvb_dmx_init(struct dvb_demux *dvbdemux)
{
        int i;
        dmx_demux_t *dmx=&dvbdemux->dmx;

        dvbdemux->users=0;
	dvbdemux->filter=vmalloc(dvbdemux->filternum*sizeof(struct dvb_demux_filter));
	if (!dvbdemux->filter)
                return -ENOMEM;

	dvbdemux->feed=vmalloc(dvbdemux->feednum*sizeof(struct dvb_demux_feed));
	if (!dvbdemux->feed) {
	        vfree(dvbdemux->filter);
                return -ENOMEM;
	}
        for (i=0; i<dvbdemux->filternum; i++) {
                dvbdemux->filter[i].state=DMX_STATE_FREE;
                dvbdemux->filter[i].index=i;
        }
        for (i=0; i<dvbdemux->feednum; i++)
                dvbdemux->feed[i].state=DMX_STATE_FREE;
        dvbdemux->frontend_list.next=
          dvbdemux->frontend_list.prev=
            &dvbdemux->frontend_list;
        for (i=0; i<DMX_TS_PES_OTHER; i++) {
                dvbdemux->pesfilter[i]=NULL;
                dvbdemux->pids[i]=0xffff;
	}
        dvbdemux->playing=dvbdemux->recording=0;
        memset(dvbdemux->pid2feed, 0, (DMX_MAX_PID+1)*sizeof(struct dvb_demux_feed *));
        dvbdemux->tsbufp=0;

	if (!dvbdemux->check_crc32)
		dvbdemux->check_crc32 = dvb_dmx_crc32;

	 if (!dvbdemux->memcopy)
		 dvbdemux->memcopy = dvb_dmx_memcopy;

        dmx->frontend=0;
        dmx->reg_list.next=dmx->reg_list.prev=&dmx->reg_list;
        dmx->priv=(void *) dvbdemux;
        //dmx->users=0;                  // reset in dmx_register_demux() 
        dmx->open=dvbdmx_open;
        dmx->close=dvbdmx_close;
        dmx->write=dvbdmx_write;
        dmx->allocate_ts_feed=dvbdmx_allocate_ts_feed;
        dmx->release_ts_feed=dvbdmx_release_ts_feed;
        dmx->allocate_section_feed=dvbdmx_allocate_section_feed;
        dmx->release_section_feed=dvbdmx_release_section_feed;

        dmx->descramble_mac_address=NULL;
        dmx->descramble_section_payload=NULL;
        
        dmx->add_frontend=dvbdmx_add_frontend;
        dmx->remove_frontend=dvbdmx_remove_frontend;
        dmx->get_frontends=dvbdmx_get_frontends;
        dmx->connect_frontend=dvbdmx_connect_frontend;
        dmx->disconnect_frontend=dvbdmx_disconnect_frontend;
        dmx->get_pes_pids=dvbdmx_get_pes_pids;
        sema_init(&dvbdemux->mutex, 1);
	spin_lock_init(&dvbdemux->lock);

        if (dmx_register_demux(dmx)<0) 
                return -1;

        return 0;
}

int 
dvb_dmx_release(struct dvb_demux *dvbdemux)
{
        dmx_demux_t *dmx=&dvbdemux->dmx;

        dmx_unregister_demux(dmx);
	if (dvbdemux->filter)
                vfree(dvbdemux->filter);
	if (dvbdemux->feed)
                vfree(dvbdemux->feed);
        return 0;
}

#if 0
MODULE_DESCRIPTION("Software MPEG Demultiplexer");
MODULE_AUTHOR("Ralph Metzler, Markus Metzler");
MODULE_LICENSE("GPL");
#endif

