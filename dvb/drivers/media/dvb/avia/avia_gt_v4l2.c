/*
 *   avia_gt_v4l2.c - AViA eNX/GTX v4l2 driver (dbox-II-project)
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
 *   $Log: avia_gt_v4l2.c,v $
 *   Revision 1.3  2002/12/21 13:43:43  Jolt
 *   Fixes
 *
 *   Revision 1.2  2002/12/20 22:30:43  Jolt
 *   Misc fixes / tweaks
 *
 *   Revision 1.1  2002/12/20 22:02:41  Jolt
 *   - V4L2 compatible pig interface
 *   - Removed old pig interface
 *   - Someone needs to fix all the pig users ;-)
 *
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
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/videodev.h>

#include "avia_gt.h"
#include "avia_gt_pig.h"

static struct v4l2_device device_info;

static int avia_gt_v4l2_open(struct v4l2_device	*device, int flags, void **idptr)
{

	dprintk("avia_gt_v4l2: open\n");
	
	*idptr = device;
	
	return 0;

}

static void avia_gt_v4l2_close(void *id)
{

	dprintk("avia_gt_v4l2: close\n");

	return;
	
}

static long avia_gt_v4l2_read(void *id, char *buf, unsigned long count, int noblock)
{

	dprintk("avia_gt_v4l2: read\n");

	return 0;
	
}

static long	avia_gt_v4l2_write(void *id, const char *buf, unsigned long count, int noblock)
{

	dprintk("avia_gt_v4l2: write\n");

	return -EINVAL;

}

/*  The arguments are already copied into kernel memory, so don't use copy_from_user() or copy_to_user() on arg.  */
static int avia_gt_v4l2_ioctl(void *id, unsigned int cmd, void *arg)
{

	dprintk("avia_gt_v4l2: ioctl\n");
	
	switch(cmd) {

		case VIDIOC_ENUMINPUT:
		{
			struct v4l2_input *input = arg;
			
			if (input->index != 0)
				return -EINVAL;
				
			strcpy(input->name, "AViA eNX/GTX digital tv picture");
			input->type = V4L2_INPUT_TYPE_TUNER;
			input->capability = 0;
			
			return 0;
			
		}

		case VIDIOC_G_INPUT:
		
			(*((int *)arg)) = 0;
					
		case VIDIOC_PREVIEW:
		
			if ((*((int *)arg)) != 0)
				return avia_gt_pig_show(0);
			else
				return avia_gt_pig_hide(0);

		case VIDIOC_QUERYCAP:
		{
			struct v4l2_capability *capability = arg;

			strcpy(capability->name, device_info.name);
			capability->type = V4L2_TYPE_CAPTURE;
//			capability->flags = V4L2_FLAG_READ | V4L2_FLAG_WRITE | V4L2_FLAG_STREAMING | V4L2_FLAG_PREVIEW;
			capability->flags = V4L2_FLAG_PREVIEW;
			capability->inputs = 1;
			capability->outputs = 1;
			capability->audios = 0;
			capability->maxwidth = 720;	//CHECKME
			capability->maxheight = 576;	//CHECKME
			capability->minwidth = 32;	//CHECKME
			capability->minheight = 32;	//CHECKME
			capability->maxframerate = 25;
			
			return 0;
			
		}

		case VIDIOC_S_INPUT:
		
			if ((*((int *)arg)) != 0)
				return -EINVAL;
			else
				return 0;

		case VIDIOC_S_WIN:
		{
			struct v4l2_window* window = (struct v4l2_window *)arg; 
	 
			if ((window->clips != NULL) || (window->clipcount != 0))
				return -EINVAL;

			avia_gt_pig_set_pos(0, window->x, window->y);
			avia_gt_pig_set_size(0, window->width, window->height, 0);
			
			return 0;
		
		}
				
		default:
		
			return -EINVAL;
			
	}
	
}

static int avia_gt_v4l2_mmap(void *id, struct vm_area_struct *vma)
{

	dprintk("avia_gt_v4l2: mmap\n");

	return 0;
	
}

static int avia_gt_v4l2_poll(void *id, struct file *file, poll_table *table)
{

	dprintk("avia_gt_v4l2: poll\n");

	return 0;

}

static int avia_gt_v4l2_initialize(struct v4l2_device *v)
{

	dprintk("avia_gt_v4l2: initialize\n");

	return 0;

}

static struct v4l2_device device_info = {

	.name = "AViA eNX/GTX v4l2",
	.type = V4L2_TYPE_CAPTURE,
	.open = avia_gt_v4l2_open,
	.close = avia_gt_v4l2_close,
	.read = avia_gt_v4l2_read,
	.write = avia_gt_v4l2_write,
	.ioctl = avia_gt_v4l2_ioctl,
	.mmap = avia_gt_v4l2_mmap,
	.poll = avia_gt_v4l2_poll,
	.initialize = avia_gt_v4l2_initialize,
	.priv = NULL,
	.busy = 0,
	
};

static int unit_video = 0; 

static int __init avia_gt_v4l2_init(void)
{

    printk("avia_gt_v4l2: $Id: avia_gt_v4l2.c,v 1.3 2002/12/21 13:43:43 Jolt Exp $\n");
	
	device_info.minor = unit_video;

	return v4l2_register_device(&device_info);

}

static void __exit avia_gt_v4l2_exit(void)
{

	v4l2_unregister_device(&device_info);

}

module_init(avia_gt_v4l2_init);
module_exit(avia_gt_v4l2_exit);

#ifdef MODULE
MODULE_PARM(unit_video, "i"); 
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#endif

