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
 *   Revision 1.4  2002/04/05 23:15:13  Jolt
 *   Improved buffer management - MP3 is rocking solid now
 *
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
 *   $Revision: 1.4 $
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
sPcmBuffer pcm_buffer_array[ENX_PCM_BUFFER_COUNT];

// Warning - result is _per_ channel
unsigned int enx_pcm_calc_sample_count(unsigned int buffer_size)
{

    if (enx_reg_s(PCMC)->W)
	buffer_size /= 2;

    if (enx_reg_s(PCMC)->C)
	buffer_size /= 2;

    return buffer_size;
    
}

// Warning - if stereo result is for _both_ channels
unsigned int enx_pcm_calc_buffer_size(unsigned int sample_count)
{

    if (enx_reg_s(PCMC)->W)
	sample_count *= 2;

    if (enx_reg_s(PCMC)->C)
	sample_count *= 2;

    return sample_count;
    
}

void enx_pcm_queue_buffer(void)
{

    sPcmBuffer *pcm_buffer;
    struct list_head *ptr;

    if (!enx_reg_s(PCMA)->W)
        return;
    
    list_for_each(ptr, &pcm_busy_buffer_list) {
    
	pcm_buffer = list_entry(ptr, sPcmBuffer, List);
	    
	if (!pcm_buffer->Queued) {
	
    	    enx_reg_s(PCMS)->NSAMP = pcm_buffer->SampleCount;
	    enx_reg_s(PCMA)->Addr = pcm_buffer->Offset >> 1;
	    enx_reg_s(PCMA)->W = 0;

	    pcm_buffer->Queued = 1;

	    return;

	}

    }

}

static void enx_pcm_irq(int reg, int bit)
{

    sPcmBuffer *pcm_buffer;

    if (!list_empty(&pcm_busy_buffer_list)) {
    
	pcm_buffer = list_entry(pcm_busy_buffer_list.next, sPcmBuffer, List);
	list_del(&pcm_buffer->List);
	
	pcm_buffer->Queued = 0;
	
	list_add_tail(&pcm_buffer->List, &pcm_free_buffer_list);

    }

    enx_pcm_queue_buffer();

    wake_up_interruptible(&enx_pcm_wait);

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

    switch(rate) {
    
	case 48000:
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

    if ((width == 8) || (width == 16))
	enx_reg_s(PCMC)->W = (width == 16);
    else
	return -EINVAL;

    return 0;
    
}

int enx_pcm_set_channels(unsigned char channels)
{

    if ((channels == 1) || (channels == 2))
        enx_reg_s(PCMC)->C = (channels == 2);
    else
	return -EINVAL;
	
    return 0;
    
}

int enx_pcm_set_signed(unsigned char signed_samples)
{

    if ((signed_samples == 0) || (signed_samples == 1))
	enx_reg_s(PCMC)->S = (signed_samples == 1);
    else
	return -EINVAL;
	
    return 0;
    
}

int enx_pcm_set_endian(unsigned char be)
{

    if ((be == 0) || (be == 1))
	swab_samples = (be == 0);
    else
	return -EINVAL;
	
    return 0;
    
}

int enx_pcm_play_buffer(void *buffer, unsigned int buffer_size, unsigned char block) {

    int i;
    unsigned short *swab_target;
    unsigned short swab_buffer[ENX_PCM_BUFFER_SIZE];
    unsigned int SampleCount;
    sPcmBuffer *pcm_buffer;

    SampleCount = enx_pcm_calc_sample_count(buffer_size);

    if (SampleCount > ENX_PCM_MAX_SAMPLES)
        SampleCount = ENX_PCM_MAX_SAMPLES;
	
    // If 8-bit mono then sample count has to be even
    if ((!enx_reg_s(PCMC)->W) && (!enx_reg_s(PCMC)->C))
	SampleCount &= ~1;

    while (list_empty(&pcm_free_buffer_list)) {

	if (block) {

    	    if (wait_event_interruptible(enx_pcm_wait, !list_empty(&pcm_busy_buffer_list)))
	    	return -ERESTARTSYS;

	} else {

	    return -EWOULDBLOCK;
	    
	}
    
    }

    pcm_buffer = list_entry(pcm_free_buffer_list.next, sPcmBuffer, List);
    list_del(&pcm_buffer->List);
	    
    if ((enx_reg_s(PCMC)->W) && (swab_samples)) {

	copy_from_user(swab_buffer, buffer, enx_pcm_calc_buffer_size(SampleCount));
	swab_target = (unsigned short *)(enx_get_mem_addr() + pcm_buffer->Offset);
	
	for (i = 0; i < enx_pcm_calc_buffer_size(SampleCount) / 2; i++)
	    swab_target[i] = swab16(swab_buffer[i]);
    
    } else {
    
	copy_from_user(enx_get_mem_addr() + pcm_buffer->Offset, buffer, enx_pcm_calc_buffer_size(SampleCount));
	
    }
    
    pcm_buffer->SampleCount = SampleCount;
    
    list_add_tail(&pcm_buffer->List, &pcm_busy_buffer_list);
    
    enx_pcm_queue_buffer();

    return enx_pcm_calc_buffer_size(SampleCount);

}

void enx_pcm_stop(void)
{

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

    unsigned char buf_nr;

    printk("enx_pcm: $Id: enx_pcm.c,v 1.4 2002/04/05 23:15:13 Jolt Exp $\n");

    for (buf_nr = 0; buf_nr < ENX_PCM_BUFFER_COUNT; buf_nr++) {
    
	pcm_buffer_array[buf_nr].Id = buf_nr + 1;
	pcm_buffer_array[buf_nr].Offset = ENX_PCM_MEM_OFFSET + (ENX_PCM_BUFFER_SIZE * buf_nr);
	pcm_buffer_array[buf_nr].Queued = 0;
	
	list_add_tail(&pcm_buffer_array[buf_nr].List, &pcm_free_buffer_list);
	
    }
    
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
    
    enx_reg_s(RSTR0)->PCM = 1;

}

#ifdef MODULE
module_init(enx_pcm_init);
module_exit(enx_pcm_cleanup);
#endif
