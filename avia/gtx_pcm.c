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
 *   Revision 1.3  2002/04/05 23:27:16  Jolt
 *   Backport of improved eNX driver
 *
 *   Revision 1.2  2002/04/02 20:13:22  Jolt
 *   GTX fixes
 *
 *   Revision 1.1  2002/04/02 15:05:28  Jolt
 *   Untested gtx_pcm import 8-)
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

#include <dbox/gtx.h>
#include <dbox/avia_pcm.h>

DECLARE_WAIT_QUEUE_HEAD(gtx_pcm_wait);

typedef struct {

    struct list_head List;
    unsigned int Id;
    unsigned int Offset;
    unsigned int SampleCount;
    unsigned char Queued;
		
} sPcmBuffer;
		
LIST_HEAD(pcm_busy_buffer_list);
LIST_HEAD(pcm_free_buffer_list);
		
int swab_samples;
sPcmBuffer pcm_buffer_array[GTX_PCM_BUFFER_COUNT];

// Warning - result is _per_ channel
unsigned int gtx_pcm_calc_sample_count(unsigned int buffer_size)
{

    if (gtx_reg_s(PCMC)->W)
	buffer_size /= 2;

    if (gtx_reg_s(PCMC)->C)
	buffer_size /= 2;

    return buffer_size;
    
}

// Warning - if stereo result is for _both_ channels
unsigned int gtx_pcm_calc_buffer_size(unsigned int sample_count)
{

    if (gtx_reg_s(PCMC)->W)
	sample_count *= 2;

    if (gtx_reg_s(PCMC)->C)
	sample_count *= 2;

    return sample_count;
    
}

void gtx_pcm_queue_buffer(void)
{

    sPcmBuffer *pcm_buffer;
    struct list_head *ptr;

    if (!gtx_reg_s(PCMA)->W)
        return;
    
    list_for_each(ptr, &pcm_busy_buffer_list) {
    
	pcm_buffer = list_entry(ptr, sPcmBuffer, List);
	    
	if (!pcm_buffer->Queued) {
	
    	    gtx_reg_s(PCMA)->NSAMP = pcm_buffer->SampleCount;
	    gtx_reg_s(PCMA)->Addr = pcm_buffer->Offset >> 1;
	    gtx_reg_s(PCMA)->W = 0;

	    pcm_buffer->Queued = 1;

	    return;

	}

    }

}

static void gtx_pcm_irq(int reg, int bit)
{

    sPcmBuffer *pcm_buffer;

    if (!list_empty(&pcm_busy_buffer_list)) {
    
	pcm_buffer = list_entry(pcm_busy_buffer_list.next, sPcmBuffer, List);
	list_del(&pcm_buffer->List);
	
	pcm_buffer->Queued = 0;
	
	list_add_tail(&pcm_buffer->List, &pcm_free_buffer_list);

    }

    gtx_pcm_queue_buffer();

    wake_up_interruptible(&gtx_pcm_wait);

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

int gtx_pcm_set_rate(unsigned short rate)
{

    switch(rate) {
    
	case 48000:
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
    
    return 0;
    
}

int gtx_pcm_set_width(unsigned char width)
{

    if ((width == 8) || (width == 16))
	gtx_reg_s(PCMC)->W = (width == 16);
    else
	return -EINVAL;

    return 0;
    
}

int gtx_pcm_set_channels(unsigned char channels)
{

    if ((channels == 1) || (channels == 2))
        gtx_reg_s(PCMC)->C = (channels == 2);
    else
	return -EINVAL;
	
    return 0;
    
}

int gtx_pcm_set_signed(unsigned char signed_samples)
{

    if ((signed_samples == 0) || (signed_samples == 1))
	gtx_reg_s(PCMC)->S = (signed_samples == 1);
    else
	return -EINVAL;
	
    return 0;
    
}

int gtx_pcm_set_endian(unsigned char be)
{

    if ((be == 0) || (be == 1))
	swab_samples = (be == 0);
    else
	return -EINVAL;
	
    return 0;
    
}

int gtx_pcm_play_buffer(void *buffer, unsigned int buffer_size, unsigned char block) {

    int i;
    unsigned short *swab_target;
    unsigned short swab_buffer[GTX_PCM_BUFFER_SIZE];
    unsigned int SampleCount;
    sPcmBuffer *pcm_buffer;

    SampleCount = gtx_pcm_calc_sample_count(buffer_size);

    if (SampleCount > GTX_PCM_MAX_SAMPLES)
        SampleCount = GTX_PCM_MAX_SAMPLES;
	
    // If 8-bit mono then sample count has to be even
    if ((!gtx_reg_s(PCMC)->W) && (!gtx_reg_s(PCMC)->C))
	SampleCount &= ~1;

    while (list_empty(&pcm_free_buffer_list)) {

	if (block) {

    	    if (wait_event_interruptible(gtx_pcm_wait, !list_empty(&pcm_busy_buffer_list)))
	    	return -ERESTARTSYS;

	} else {

	    return -EWOULDBLOCK;
	    
	}
    
    }

    pcm_buffer = list_entry(pcm_free_buffer_list.next, sPcmBuffer, List);
    list_del(&pcm_buffer->List);
	    
    if ((gtx_reg_s(PCMC)->W) && (swab_samples)) {

	copy_from_user(swab_buffer, buffer, gtx_pcm_calc_buffer_size(SampleCount));
	swab_target = (unsigned short *)(gtx_get_mem_addr() + pcm_buffer->Offset);
	
	for (i = 0; i < gtx_pcm_calc_buffer_size(SampleCount) / 2; i++)
	    swab_target[i] = swab16(swab_buffer[i]);
    
    } else {
    
	copy_from_user(gtx_get_mem_addr() + pcm_buffer->Offset, buffer, gtx_pcm_calc_buffer_size(SampleCount));
	
    }
    
    pcm_buffer->SampleCount = SampleCount;
    
    list_add_tail(&pcm_buffer->List, &pcm_busy_buffer_list);
    
    gtx_pcm_queue_buffer();

    return gtx_pcm_calc_buffer_size(SampleCount);

}

void gtx_pcm_stop(void)
{

//    gtx_reg_s(PCMC)->T = 1;

}

static sAviaPcmOps gtx_pcm_ops = {

    name: "AViA GTX soundcore",
    play_buffer: gtx_pcm_play_buffer,
    set_rate:  gtx_pcm_set_rate,
    set_width:  gtx_pcm_set_width,
    set_channels:  gtx_pcm_set_channels,
    set_signed:  gtx_pcm_set_signed,
    set_endian:  gtx_pcm_set_endian,
    set_mpeg_attenuation: gtx_pcm_set_mpeg_attenuation,
    set_pcm_attenuation: gtx_pcm_set_pcm_attenuation,
    stop: gtx_pcm_stop,
    
};
    
static int __init gtx_pcm_init(void)
{

    unsigned char buf_nr;

    printk("gtx_pcm: $Id: gtx_pcm.c,v 1.3 2002/04/05 23:27:16 Jolt Exp $\n");

    for (buf_nr = 0; buf_nr < GTX_PCM_BUFFER_COUNT; buf_nr++) {
    
	pcm_buffer_array[buf_nr].Id = buf_nr + 1;
	pcm_buffer_array[buf_nr].Offset = GTX_PCM_MEM_OFFSET + (GTX_PCM_BUFFER_SIZE * buf_nr);
	pcm_buffer_array[buf_nr].Queued = 0;
	
	list_add_tail(&pcm_buffer_array[buf_nr].List, &pcm_free_buffer_list);
	
    }
    
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

    // Use external clock from AViA 500/600
    gtx_reg_s(PCMC)->I = 0;
    
    // Set a default mode
    gtx_pcm_set_rate(44100);
    gtx_pcm_set_width(16);
    gtx_pcm_set_channels(2);
    gtx_pcm_set_signed(1);
    gtx_pcm_set_endian(1);
    
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
    
    //gtx_reg_s(RSTR0)->PCM = 1;
    gtx_reg_16(RR0) |= (1 << 9); 

}

#ifdef MODULE
module_init(gtx_pcm_init);
module_exit(gtx_pcm_cleanup);
#endif
