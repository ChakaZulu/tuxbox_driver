/*
 *   avia_oss.c - AViA GTX/eNX oss driver (dbox-II-project)
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
 *   $Log: avia_oss.c,v $
 *   Revision 1.2  2002/04/02 13:57:09  Jolt
 *   Dependency fixes
 *
 *   Revision 1.1  2002/04/01 22:23:22  Jolt
 *   Basic PCM driver for eNX - more to come later
 *
 *
 *
 *   $Revision: 1.2 $
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
#include <asm/uaccess.h>
#include <linux/init.h>

#include <linux/sound.h>
#include <linux/soundcard.h>

#include <dbox/avia_pcm.h>

int dsp_dev;
int mixer_dev;
sAviaPcmOps *pcm_ops;

static ssize_t avia_oss_dsp_write(struct file *file, const char *buf, size_t count, loff_t *offset)
{

//    printk("avia_oss: dsp write (buffer=0x%X, count=%d)\n", (unsigned int)buf, count);

    return pcm_ops->play_buffer((void *)buf, count, 1);

}

static struct file_operations dsp_fops = {

    owner: THIS_MODULE,
    write: avia_oss_dsp_write,
    
};

static struct file_operations mixer_fops = {

    owner: THIS_MODULE,
//    ioctl: avia_oss_mixer_ioctl,
    
};

static int __init avia_oss_init(void)
{

    printk("avia_oss: $Id: avia_oss.c,v 1.2 2002/04/02 13:57:09 Jolt Exp $\n");
    
    if ((pcm_ops = (sAviaPcmOps *)inter_module_get(IM_AVIA_PCM_OPS)) == NULL) {

        printk("avia_oss: error - no soundcore found\n");
        return -EIO;
	
    }

    printk("avia_oss: found %s\n", pcm_ops->name);
    
    pcm_ops->set_pcm_attenuation(0x80, 0x80);
    pcm_ops->set_mode(44100, 16, 2, 1);
    
    dsp_dev = register_sound_dsp(&dsp_fops, -1);
    mixer_dev = register_sound_mixer(&mixer_fops, -1);

    return 0;
    
}

static void __exit avia_oss_cleanup(void)
{

    printk("avia_oss: cleanup\n");
    
    unregister_sound_mixer(mixer_dev);
    unregister_sound_dsp(dsp_dev);

    pcm_ops->set_pcm_attenuation(0x00, 0x00);
    pcm_ops->stop();
    
    inter_module_put(IM_AVIA_PCM_OPS);
    
}

#ifdef MODULE
module_init(avia_oss_init);
module_exit(avia_oss_cleanup);
#endif
