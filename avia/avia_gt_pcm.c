/*
 *   avia_gt_pcm.c - AViA eNX/GTX pcm driver (dbox-II-project)
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
 *   $Log: avia_gt_pcm.c,v $
 *   Revision 1.5  2002/04/10 21:53:31  Jolt
 *   Further cleanups/bugfixes
 *   More OSS API stuff
 *
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
 *   $Revision: 1.5 $
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

#include <dbox/avia_gt.h>
#include <dbox/avia_gt_pcm.h>

DECLARE_WAIT_QUEUE_HEAD(pcm_wait);

typedef struct {

    struct list_head list;
    unsigned int offset;
    unsigned int sample_count;
    unsigned char queued;
		
} sPcmBuffer;
		
LIST_HEAD(pcm_busy_buffer_list);
LIST_HEAD(pcm_free_buffer_list);

spinlock_t busy_buffer_lock = SPIN_LOCK_UNLOCKED;
spinlock_t free_buffer_lock = SPIN_LOCK_UNLOCKED;
		
int swab_samples;
sPcmBuffer pcm_buffer_array[ENX_PCM_BUFFER_COUNT];
unsigned char swab_buffer[ENX_PCM_BUFFER_SIZE];

// Warning - result is _per_ channel
unsigned int avia_gt_pcm_calc_sample_count(unsigned int buffer_size)
{

    if (enx_reg_s(PCMC)->W)
	buffer_size /= 2;

    if (enx_reg_s(PCMC)->C)
	buffer_size /= 2;

    return buffer_size;
    
}

// Warning - if stereo result is for _both_ channels
unsigned int avia_gt_pcm_calc_buffer_size(unsigned int sample_count)
{

    if (enx_reg_s(PCMC)->W)
	sample_count *= 2;

    if (enx_reg_s(PCMC)->C)
	sample_count *= 2;

    return sample_count;
    
}

void avia_gt_pcm_queue_buffer(void)
{

    unsigned long flags;
    sPcmBuffer *pcm_buffer;
    struct list_head *ptr;

    if (!enx_reg_s(PCMA)->W)
        return;
    
    spin_lock_irqsave(&busy_buffer_lock, flags);
    
    list_for_each(ptr, &pcm_busy_buffer_list) {
    
	pcm_buffer = list_entry(ptr, sPcmBuffer, list);
	    
	if (!pcm_buffer->queued) {
	
    	    enx_reg_s(PCMS)->NSAMP = pcm_buffer->sample_count;
	    enx_reg_s(PCMA)->Addr = pcm_buffer->offset >> 1;
	    enx_reg_s(PCMA)->W = 0;

	    pcm_buffer->queued = 1;

	    break;	    

	}

    }

    spin_unlock_irqrestore(&busy_buffer_lock, flags);

}

static void avia_gt_pcm_irq(int reg, int bit)
{

    unsigned long flags;
    sPcmBuffer *pcm_buffer;
//    int i = 0;
//    struct list_head *ptr;

    spin_lock_irqsave(&busy_buffer_lock, flags);
    
/*    if (bit == 4)
	printk("X");

    list_for_each(ptr, &pcm_busy_buffer_list) {
	i++;
    }
    printk("%d ", i);*/

    if (!list_empty(&pcm_busy_buffer_list)) {
    
	pcm_buffer = list_entry(pcm_busy_buffer_list.next, sPcmBuffer, list);
	list_del(&pcm_buffer->list);
	
	pcm_buffer->queued = 0;
	
	spin_lock_irqsave(&free_buffer_lock, flags);
	
	list_add_tail(&pcm_buffer->list, &pcm_free_buffer_list);

	spin_unlock_irqrestore(&free_buffer_lock, flags);

    }

    spin_unlock_irqrestore(&busy_buffer_lock, flags);

    avia_gt_pcm_queue_buffer();

    wake_up_interruptible(&pcm_wait);

}

unsigned int avia_gt_pcm_get_block_size(void)
{

    return avia_gt_pcm_calc_buffer_size(ENX_PCM_MAX_SAMPLES);

}

void avia_gt_pcm_set_mpeg_attenuation(unsigned char left, unsigned char right)
{

    enx_reg_s(PCMN)->MPEGAL = left >> 1;
    enx_reg_s(PCMN)->MPEGAR = right >> 1;
    
}

void avia_gt_pcm_set_pcm_attenuation(unsigned char left, unsigned char right)
{

    enx_reg_s(PCMN)->PCMAL = left >> 1;
    enx_reg_s(PCMN)->PCMAR = right >> 1;
    
}

int avia_gt_pcm_set_rate(unsigned short rate)
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

int avia_gt_pcm_set_width(unsigned char width)
{

    if ((width == 8) || (width == 16))
	enx_reg_s(PCMC)->W = (width == 16);
    else
	return -EINVAL;

    return 0;
    
}

int avia_gt_pcm_set_channels(unsigned char channels)
{

    if ((channels == 1) || (channels == 2))
        enx_reg_s(PCMC)->C = (channels == 2);
    else
	return -EINVAL;
	
    return 0;
    
}

int avia_gt_pcm_set_signed(unsigned char signed_samples)
{

    if ((signed_samples == 0) || (signed_samples == 1))
	enx_reg_s(PCMC)->S = (signed_samples == 1);
    else
	return -EINVAL;
	
    return 0;
    
}

int avia_gt_pcm_set_endian(unsigned char be)
{

    if ((be == 0) || (be == 1))
	swab_samples = (be == 0);
    else
	return -EINVAL;
	
    return 0;
    
}

int avia_gt_pcm_play_buffer(void *buffer, unsigned int buffer_size, unsigned char block) {

    unsigned long flags;
    unsigned int sample_nr;
    unsigned short *swab_dest;
    unsigned short *swab_src;
    unsigned int sample_count;
    sPcmBuffer *pcm_buffer;
    
    sample_count = avia_gt_pcm_calc_sample_count(buffer_size);

    if (sample_count > ENX_PCM_MAX_SAMPLES)
        sample_count = ENX_PCM_MAX_SAMPLES;
	
    // If 8-bit mono then sample count has to be even
    if ((!enx_reg_s(PCMC)->W) && (!enx_reg_s(PCMC)->C))
	sample_count &= ~1;

    while (list_empty(&pcm_free_buffer_list)) {

	if (block) {

    	    if (wait_event_interruptible(pcm_wait, !list_empty(&pcm_free_buffer_list)))
	    	return -ERESTARTSYS;

	} else {

	    return -EWOULDBLOCK;
	    
	}
    
    }

    spin_lock_irqsave(&free_buffer_lock, flags);
    
    pcm_buffer = list_entry(pcm_free_buffer_list.next, sPcmBuffer, list);
    list_del(&pcm_buffer->list);
	    
    spin_unlock_irqrestore(&free_buffer_lock, flags);
    
    if ((enx_reg_s(PCMC)->W) && (swab_samples)) {

	copy_from_user(swab_buffer, buffer, avia_gt_pcm_calc_buffer_size(sample_count));

	swab_dest = (unsigned short *)(enx_get_mem_addr() + pcm_buffer->offset);
	swab_src = (unsigned short *)swab_buffer;
	
	for (sample_nr = 0; sample_nr < avia_gt_pcm_calc_buffer_size(sample_count) / 2; sample_nr++)
	    swab_dest[sample_nr] = swab16(swab_src[sample_nr]);
    
    } else {
    
	copy_from_user(enx_get_mem_addr() + pcm_buffer->offset, buffer, avia_gt_pcm_calc_buffer_size(sample_count));
	
    }
    
    pcm_buffer->sample_count = sample_count;

    spin_lock_irqsave(&busy_buffer_lock, flags);
    
    list_add_tail(&pcm_buffer->list, &pcm_busy_buffer_list);

    spin_unlock_irqrestore(&busy_buffer_lock, flags);
    
    avia_gt_pcm_queue_buffer();

    return avia_gt_pcm_calc_buffer_size(sample_count);

}

void avia_gt_pcm_stop(void)
{

//    enx_reg_s(PCMC)->T = 1;

}

static int __init avia_gt_pcm_init(void)
{

    unsigned char buf_nr;

    printk("avia_gt_pcm: $Id: avia_gt_pcm.c,v 1.5 2002/04/10 21:53:31 Jolt Exp $\n");

    for (buf_nr = 0; buf_nr < ENX_PCM_BUFFER_COUNT; buf_nr++) {
    
	pcm_buffer_array[buf_nr].offset = ENX_PCM_MEM_OFFSET + (ENX_PCM_BUFFER_SIZE * buf_nr);
	pcm_buffer_array[buf_nr].queued = 0;
	
	list_add_tail(&pcm_buffer_array[buf_nr].list, &pcm_free_buffer_list);
	
    }
    
    // Reset PCM module
    enx_reg_s(RSTR0)->PCM = 1;
    
    if (enx_allocate_irq(ENX_IRQ_PCM_AD, avia_gt_pcm_irq) != 0) {

	printk("enx_pcm: unable to get pcm-ad interrupt\n");
	
	return -EIO;
	
    }
		
    if (enx_allocate_irq(ENX_IRQ_PCM_PF, avia_gt_pcm_irq) != 0) {

	printk("enx_pcm: unable to get pcm-pf interrupt\n");
	
	enx_free_irq(ENX_IRQ_PCM_AD);
	
	return -EIO;
	
    }

    // Get PCM module out of reset state
    enx_reg_s(RSTR0)->PCM = 0;

    // Use external clock from AViA 500/600
    enx_reg_s(PCMC)->I = 0;

    // Pass through mpeg samples
    avia_gt_pcm_set_mpeg_attenuation(0x80, 0x80);
    
    // Set a default mode
    avia_gt_pcm_set_rate(44100);
    avia_gt_pcm_set_width(16);
    avia_gt_pcm_set_channels(2);
    avia_gt_pcm_set_signed(1);
    avia_gt_pcm_set_endian(1);
    
    return 0;
    
}

static void __exit avia_gt_pcm_cleanup(void)
{

    enx_free_irq(ENX_IRQ_PCM_AD);
    enx_free_irq(ENX_IRQ_PCM_PF);
    
    enx_reg_s(RSTR0)->PCM = 1;

}

#ifdef MODULE
module_init(avia_gt_pcm_init);
module_exit(avia_gt_pcm_cleanup);
#endif
