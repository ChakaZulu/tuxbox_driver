/*
 *   enx_oss.c - AViA eNX oss driver (dbox-II-project)
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
 *   $Log: avia_gt_oss.c,v $
 *   Revision 1.1  2002/04/01 22:23:22  Jolt
 *   Basic PCM driver for eNX - more to come later
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

#include <linux/sound.h>
#include <linux/soundcard.h>

#include <dbox/enx.h>
#include <dbox/enx_pcm.h>

int enx_dsp_dev;

static ssize_t enx_oss_write(struct file *file, const char *buf, size_t count, loff_t *offset)
{

//    printk("enx_oss: write (buffer=0x%X, count=%d)\n", (unsigned int)buf, count);

    return enx_play_buffer(buf, count, 1);

}

static struct file_operations enx_oss_fops = {

    owner: THIS_MODULE,
    write: enx_oss_write,
//    ioctl:          pcm_ioctl,
//    open:           pcm_open,
    
};
					

static int enx_oss_init(void)
{

    printk("enx_oss: $Id: avia_gt_oss.c,v 1.1 2002/04/01 22:23:22 Jolt Exp $\n");

    enx_pcm_set_pcm_attenuation(0x80, 0x80);
    enx_pcm_set_mode(44100, 16, 2, 1);
//    enx_pcm_set_mode(44100 / 4, 8, 1, 0);
//    enx_pcm_set_mode(44100, 8, 2, 0);
    
    //enx_play_buffer(NULL, 0, 1);
    
    enx_dsp_dev = register_sound_dsp(&enx_oss_fops, -1);

    return 0;
    
}

static void __exit enx_oss_cleanup(void)
{

    printk("enx_oss: cleanup\n");
    
    unregister_sound_dsp(enx_dsp_dev);

    enx_pcm_set_pcm_attenuation(0x00, 0x00);
    enx_pcm_stop();
    
}

#ifdef MODULE
module_init(enx_oss_init);
module_exit(enx_oss_cleanup);
#endif
