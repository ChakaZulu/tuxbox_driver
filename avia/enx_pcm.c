/*
 *   enx_pcm.c - AViA eNX pcm driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2002 Florian Schirmer (jolt@tuxbox.org)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *   $Log: enx_pcm.c,v $
 *   Revision 1.3  2002/04/02 18:14:10  Jolt
 *   Further features/bugfixes. MP3 works very well now 8-)
 *
 *   Revision 1.2  2002/04/02 13:56:50  Jolt
 *   Dependency fixes
 *
 *   Revision 1.1  2002/04/01 22:23:22  Jolt
 *   Basic PCM driver for eNX - more to come later
 *
 *
 *
 *   $Revision: 1.3 $
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/byteorder/swab.h>

#include <dbox/enx.h>
#include <dbox/avia_pcm.h>

DECLARE_WAIT_QUEUE_HEAD(enx_pcm_wait);

#define MAX_BUF 2
int buf_size = (ENX_PCM_SIZE / MAX_BUF);
int buf_ptr[MAX_BUF];
unsigned char buf_busy[MAX_BUF];
int buf_playing = 0;
int buf_last = 1;
int swab_samples;

#define dprintk(...)
//#define dprintk printk

static void enx_pcm_irq(int reg, int bit)
{

    dprintk("enx_pcm: irq (reg=%d, bit=%d)\n", reg, bit);

    dprintk("enx_pcm: irq begin (buf_busy[0]=%d buf_busy[1]=%d buf_playing=%d)\n", buf_busy[0], buf_busy[1], buf_playing);
    
    if (!buf_playing) {
    
	printk("enx_pcm: doh - not playing?!\n");
    
    } else {
    
	buf_busy[buf_playing - 1] = 0;
	
	if (buf_busy[!(buf_playing - 1)])
	    buf_playing = (!(buf_playing - 1)) + 1;
	else
	    buf_playing = 0;
    }

    dprintk("enx_pcm: irq end (buf_busy[0]=%d buf_busy[1]=%d buf_playing=%d)\n", buf_busy[0], buf_busy[1], buf_playing);
    
    wake_up_interruptible(&enx_pcm_wait);

}

unsigned int enx_pcm_calc_sample_count(unsigned int buffer_size)
{

    if (enx_reg_s(PCMC)->W)
	buffer_size /= 2;

    if (enx_reg_s(PCMC)->C)
	buffer_size /= 2;

    return buffer_size;
    
}

void enx_pcm_set_mpeg_attenuation(unsigned char left, unsigned char right)
{

    enx_reg_s(PCMN)->MPEGAL = left >> 1;
    enx_reg_s(PCMN)->MPEGAR = right >> 1;
    
}

void enx_pcm_set_pcm_attenuation(unsigned char left, unsigned char right)
{

    enx_reg_s(PCMN)->PCMAL = left >> 1;
    enx_reg_s(PCMN)->PCMAR = right >> 1;
    
}

int enx_pcm_set_rate(unsigned short rate)
{

    dprintk("enx_pcm: setting rate (rate=%d)\n", rate);

    switch(rate) {
    
	case 44100:
	
	    enx_reg_s(PCMC)->R = 3;
	    
	break;
	
	case 22050:
	
	    enx_reg_s(PCMC)->R = 2;
	    
	break;
	
	case 11025:
	
	    enx_reg_s(PCMC)->R = 1;
	    
	break;
	
	default:
	
	    return -EINVAL;
	    
	break;
	
    }
    
    return 0;
    
}

int enx_pcm_set_width(unsigned char width)
{

    dprintk("enx_pcm: setting width (width=%d)\n", width);

    if ((width == 8) || (width == 16))
	enx_reg_s(PCMC)->W = (width == 16);
    else
	return -EINVAL;

    return 0;
    
}

int enx_pcm_set_channels(unsigned char channels)
{

    dprintk("enx_pcm: setting channels (channels=%d)\n", channels);

    if ((channels == 1) || (channels == 2))
        enx_reg_s(PCMC)->C = (channels == 2);
    else
	return -EINVAL;
	
    return 0;
    
}

int enx_pcm_set_signed(unsigned char signed_samples)
{

    dprintk("enx_pcm: setting signed (signed=%d)\n", signed_samples);

    if ((signed_samples == 0) || (signed_samples == 1))
	enx_reg_s(PCMC)->S = (signed_samples == 1);
    else
	return -EINVAL;
	
    return 0;
    
}

int enx_pcm_set_endian(unsigned char be)
{

    dprintk("enx_pcm: setting endian (be=%d)\n", be);

    if ((be == 0) || (be == 1))
	swab_samples = (be == 0);
    else
	return -EINVAL;
	
    return 0;
    
}

int enx_pcm_play_buffer(void *buffer, unsigned int buffer_size, unsigned char block) {

    int buf_nr, i;
    unsigned short *swap_target;
    unsigned short swap_buffer[buf_size];

    if (buffer_size > buf_size)
	buffer_size = buf_size;

    dprintk("enx_pcm: playing buffer (addr=0x%X, size=%d)\n", (unsigned int)buffer, buffer_size);

    if (!enx_reg_s(PCMA)->W) {
    
	if (block) {

	    dprintk("enx_pcm: pcm unit busy - blocking\n");
	    
    	    if (wait_event_interruptible(enx_pcm_wait, enx_reg_s(PCMA)->W))
	    	return -ERESTARTSYS;

	    dprintk("enx_pcm: blocking done\n");
	    
	} else {

	    dprintk("enx_pcm: pcm unit busy - returning\n");
	
	    return -EWOULDBLOCK;
	    
	}
	
    }
    
    buf_nr = !buf_last;

    if (buf_busy[buf_nr]) {

	if (block) {

	    dprintk("enx_pcm: buffer queue busy - blocking\n");
	    
    	    if (wait_event_interruptible(enx_pcm_wait, !(buf_busy[buf_nr])))
	    	return -ERESTARTSYS;

	    dprintk("enx_pcm: blocking done\n");
	    
	} else {

	    dprintk("enx_pcm: buffer queue busy - returning\n");
	
	    return -EWOULDBLOCK;
	    
	}
    
    }
    
    dprintk("enx_pcm: play_buffer (buf_busy[0]=%d buf_busy[1]=%d buf_nr=%d buf_playing=%d buf_ptr[buf_nr]=0x%X)\n", buf_busy[0], buf_busy[1], buf_nr, buf_playing, (unsigned int)buf_ptr[buf_nr]);
    
    if ((enx_reg_s(PCMC)->W) && (swab_samples)) {

	copy_from_user(swap_buffer, buffer, buffer_size);
	swap_target = (unsigned short *)(enx_get_mem_addr() + buf_ptr[buf_nr]);
	
	for (i = 0; i < buffer_size / 2; i++)
	    swap_target[i] = swab16(swap_buffer[i]);
    
    } else {
    
	copy_from_user(enx_get_mem_addr() + buf_ptr[buf_nr], buffer, buffer_size);
	
    }
    
    buf_busy[buf_nr] = 1;
    buf_last = buf_nr;
    
    if (!buf_playing)
	buf_playing = buf_nr + 1;

    enx_reg_s(PCMS)->NSAMP = enx_pcm_calc_sample_count(buffer_size);
    enx_reg_s(PCMA)->Addr = buf_ptr[buf_nr] >> 1;
    enx_reg_s(PCMA)->W = 0;

    return buffer_size;

}

void enx_pcm_stop(void)
{

    printk("enx_pcm: stopping playmode\n");

//    enx_reg_s(PCMC)->T = 1;

}

static sAviaPcmOps enx_pcm_ops = {

    name: "AViA eNX soundcore",
    play_buffer: enx_pcm_play_buffer,
    set_rate:  enx_pcm_set_rate,
    set_width:  enx_pcm_set_width,
    set_channels:  enx_pcm_set_channels,
    set_signed:  enx_pcm_set_signed,
    set_endian:  enx_pcm_set_endian,
    set_mpeg_attenuation: enx_pcm_set_mpeg_attenuation,
    set_pcm_attenuation: enx_pcm_set_pcm_attenuation,
    stop: enx_pcm_stop,
    
};
    
static int __init enx_pcm_init(void)
{

    printk("enx_pcm: $Id: enx_pcm.c,v 1.3 2002/04/02 18:14:10 Jolt Exp $\n");

    buf_ptr[0] = ENX_PCM_OFFSET;
    buf_busy[0] = 0;
    buf_ptr[1] = ENX_PCM_OFFSET + buf_size;
    buf_busy[1] = 0;
    
    // Reset PCM module
    enx_reg_s(RSTR0)->PCM = 1;
    
    if (enx_allocate_irq(ENX_IRQ_PCM_AD, enx_pcm_irq) != 0) {

	printk("enx_pcm: unable to get pcm-ad interrupt\n");
	
	return -EIO;
	
    }
		
    if (enx_allocate_irq(ENX_IRQ_PCM_PF, enx_pcm_irq) != 0) {

	printk("enx_pcm: unable to get pcm-pf interrupt\n");
	
	enx_free_irq(ENX_IRQ_PCM_AD);
	
	return -EIO;
	
    }

    // Get PCM module out of reset state
    enx_reg_s(RSTR0)->PCM = 0;

    // Use external clock from AViA 500/600
    enx_reg_s(PCMC)->I = 0;
    
    // Set a default mode
    enx_pcm_set_rate(44100);
    enx_pcm_set_width(16);
    enx_pcm_set_channels(2);
    enx_pcm_set_signed(1);
    enx_pcm_set_endian(1);
    
    // Enable inter_module access
    inter_module_register(IM_AVIA_PCM_OPS, THIS_MODULE, &enx_pcm_ops);
        
    return 0;
    
}

static void __exit enx_pcm_cleanup(void)
{

    printk("enx_pcm: cleanup\n");
    
    inter_module_unregister(IM_AVIA_PCM_OPS);

    enx_free_irq(ENX_IRQ_PCM_AD);
    enx_free_irq(ENX_IRQ_PCM_PF);
    
    enx_pcm_stop();
    
    enx_reg_s(RSTR0)->PCM = 1;

}

#ifdef MODULE
module_init(enx_pcm_init);
module_exit(enx_pcm_cleanup);
#endif
