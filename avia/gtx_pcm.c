/*
 *   gtx_pcm.c - AViA GTX pcm driver (dbox-II-project)
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
 *   $Log: gtx_pcm.c,v $
 *   Revision 1.1  2002/04/02 15:05:28  Jolt
 *   Untested gtx_pcm import 8-)
 *
 *   Revision 1.1  2002/04/01 22:23:22  Jolt
 *   Basic PCM driver for gtx - more to come later
 *
 *
 *
 *   $Revision: 1.1 $
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

#include <dbox/gtx.h>
#include <dbox/avia_pcm.h>

DECLARE_WAIT_QUEUE_HEAD(gtx_pcm_wait);

#define MAX_BUF 2
int buf_size = (GTX_PCM_SIZE / MAX_BUF);
int buf_ptr[MAX_BUF];
unsigned char buf_busy[MAX_BUF];
int buf_playing = 0;
int buf_last = 1;

#define dprintk(...)

static void gtx_pcm_irq(int reg, int bit)
{

    dprintk("gtx_pcm: irq (reg=%d, bit=%d)\n", reg, bit);

    dprintk("gtx_pcm: irq begin (buf_busy[0]=%d buf_busy[1]=%d buf_playing=%d)\n", buf_busy[0], buf_busy[1], buf_playing);
    
    if (!buf_playing) {
    
	printk("gtx_pcm: doh - not playing?!\n");
    
    } else {
    
	buf_busy[buf_playing - 1] = 0;
	
	if (buf_busy[!(buf_playing - 1)])
	    buf_playing = (!(buf_playing - 1)) + 1;
	else
	    buf_playing = 0;
    }

    dprintk("gtx_pcm: irq end (buf_busy[0]=%d buf_busy[1]=%d buf_playing=%d)\n", buf_busy[0], buf_busy[1], buf_playing);
    
    wake_up_interruptible(&gtx_pcm_wait);

}

unsigned int gtx_pcm_calc_sample_count(unsigned int buffer_size)
{

    if (gtx_reg_s(PCMC)->W)
	buffer_size /= 2;

    if (gtx_reg_s(PCMC)->C)
	buffer_size /= 2;

    return buffer_size;
    
}

void gtx_pcm_set_mpeg_attenuation(unsigned char left, unsigned char right)
{

    gtx_reg_s(PCMN)->MPEGAL = left >> 1;
    gtx_reg_s(PCMN)->MPEGAR = right >> 1;
    
}

void gtx_pcm_set_pcm_attenuation(unsigned char left, unsigned char right)
{

    gtx_reg_s(PCMN)->PCMAL = left >> 1;
    gtx_reg_s(PCMN)->PCMAR = right >> 1;
    
}

int gtx_pcm_set_mode(unsigned short rate, unsigned char width, unsigned char channels, unsigned char signed_samples)
{

    printk("gtx_pcm: setting mode (rate=%d, width=%d, channels=%d, signed_samples=%d)\n", rate, width, channels, signed_samples);

    switch(rate) {
    
	case 44100:
	
	    gtx_reg_s(PCMC)->R = 3;
	    
	break;
	
	case 22050:
	
	    gtx_reg_s(PCMC)->R = 2;
	    
	break;
	
	case 11025:
	
	    gtx_reg_s(PCMC)->R = 1;
	    
	break;
	
	default:
	
	    return -EINVAL;
	    
	break;
	
    }
    
    if ((width == 8) || (width == 16))
	gtx_reg_s(PCMC)->W = (width == 16);
    else
	return -EINVAL;
	
    if ((channels == 1) || (channels == 2))
        gtx_reg_s(PCMC)->C = channels - 1;
    else
	return -EINVAL;
	
    gtx_reg_s(PCMC)->S = (signed_samples != 0);
    

    return 0;
    
}

int gtx_pcm_play_buffer(void *buffer, unsigned int buffer_size, unsigned char block) {

    int buf_nr, i;
    unsigned short *swap_target;
    unsigned short swap_buffer[buf_size];

    if (buffer_size > buf_size)
	buffer_size = buf_size;

    dprintk("gtx_pcm: playing buffer (addr=0x%X, size=%d)\n", (unsigned int)buffer, buffer_size);

    if (!gtx_reg_s(PCMA)->W) {
    
	if (block) {

	    dprintk("gtx_pcm: pcm unit busy - blocking\n");
	    
    	    if (wait_event_interruptible(gtx_pcm_wait, gtx_reg_s(PCMA)->W))
	    	return -ERESTARTSYS;

	    dprintk("gtx_pcm: blocking done\n");
	    
	} else {

	    dprintk("gtx_pcm: pcm unit busy - returning\n");
	
	    return -EWOULDBLOCK;
	    
	}
	
    }
    
    buf_nr = !buf_last;

    if (buf_busy[buf_nr]) {

	if (block) {

	    dprintk("gtx_pcm: buffer queue busy - blocking\n");
	    
    	    if (wait_event_interruptible(gtx_pcm_wait, !(buf_busy[buf_nr])))
	    	return -ERESTARTSYS;

	    dprintk("gtx_pcm: blocking done\n");
	    
	} else {

	    dprintk("gtx_pcm: buffer queue busy - returning\n");
	
	    return -EWOULDBLOCK;
	    
	}
    
    }
    
    dprintk("gtx_pcm: play_buffer (buf_busy[0]=%d buf_busy[1]=%d buf_nr=%d buf_playing=%d buf_ptr[buf_nr]=0x%X)\n", buf_busy[0], buf_busy[1], buf_nr, buf_playing, (unsigned int)buf_ptr[buf_nr]);
    
    if (gtx_reg_s(PCMC)->W) {

	copy_from_user(swap_buffer, buffer, buffer_size);
	swap_target = (unsigned short *)(gtx_get_mem_addr() + buf_ptr[buf_nr]);
	
	for (i = 0; i < buffer_size / 2; i++)
	    swap_target[i] = swab16(swap_buffer[i]);
    
    } else {
    
	copy_from_user(gtx_get_mem_addr() + buf_ptr[buf_nr], buffer, buffer_size);
	
    }
    
    buf_busy[buf_nr] = 1;
    buf_last = buf_nr;
    
    if (!buf_playing)
	buf_playing = buf_nr + 1;

    gtx_reg_s(PCMA)->NSAMP = gtx_pcm_calc_sample_count(buffer_size);
    gtx_reg_s(PCMA)->Addr = buf_ptr[buf_nr] >> 1;
    gtx_reg_s(PCMA)->W = 0;

    return buffer_size;

}

void gtx_pcm_stop(void)
{

    printk("gtx_pcm: stopping playmode\n");

//    gtx_reg_s(PCMC)->T = 1;

}

static sAviaPcmOps gtx_pcm_ops = {

    name: "AViA GTX soundcore",
    play_buffer: gtx_pcm_play_buffer,
    set_mode:  gtx_pcm_set_mode,
    set_mpeg_attenuation: gtx_pcm_set_mpeg_attenuation,
    set_pcm_attenuation: gtx_pcm_set_pcm_attenuation,
    stop: gtx_pcm_stop,
    
};
    
static int __init gtx_pcm_init(void)
{

    printk("gtx_pcm: $Id: gtx_pcm.c,v 1.1 2002/04/02 15:05:28 Jolt Exp $\n");

    buf_ptr[0] = GTX_PCM_OFFSET;
    buf_busy[0] = 0;
    buf_ptr[1] = GTX_PCM_OFFSET + buf_size;
    buf_busy[1] = 0;
    
    // Reset PCM module
    //gtx_reg_s(RSTR0)->PCM = 1;
    gtx_reg_16(RR0) |= (1 << 9);
    
    if (gtx_allocate_irq(GTX_IRQ_PCM_AD, gtx_pcm_irq) != 0) {

	printk("gtx_pcm: unable to get pcm-ad interrupt\n");
	
	return -EIO;
	
    }
		
    if (gtx_allocate_irq(GTX_IRQ_PCM_PF, gtx_pcm_irq) != 0) {

	printk("gtx_pcm: unable to get pcm-pf interrupt\n");
	
	gtx_free_irq(GTX_IRQ_PCM_AD);
	
	return -EIO;
	
    }

    // Get PCM module out of reset state
    //gtx_reg_s(RSTR0)->PCM = 0;
    gtx_reg_16(RR0) &= ~(1 << 9);

    //gtx_reg_s(PCMC)->ACD = 1;
    //gtx_reg_s(PCMC)->BCD = 2;
    gtx_reg_s(PCMC)->I = 0;
    
    // Set a default mode
    gtx_pcm_set_mode(44100, 16, 2, 1);
    
    // Enable inter_module access
    inter_module_register(IM_AVIA_PCM_OPS, THIS_MODULE, &gtx_pcm_ops);
        
    return 0;
    
}

static void __exit gtx_pcm_cleanup(void)
{

    printk("gtx_pcm: cleanup\n");
    
    inter_module_unregister(IM_AVIA_PCM_OPS);

    gtx_free_irq(GTX_IRQ_PCM_AD);
    gtx_free_irq(GTX_IRQ_PCM_PF);
    
    gtx_pcm_stop();
    
    //gtx_reg_s(RSTR0)->PCM = 1;
    gtx_reg_16(RR0) |= (1 << 9);

}

#ifdef MODULE
module_init(gtx_pcm_init);
module_exit(gtx_pcm_cleanup);
#endif
