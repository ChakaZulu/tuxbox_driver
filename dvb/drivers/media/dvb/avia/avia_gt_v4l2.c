/*
 * $Id: avia_gt_v4l2.c,v 1.9 2003/07/24 01:14:20 homar Exp $
 *
 * AViA eNX/GTX v4l2 driver (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2002 Florian Schirmer (jolt@tuxbox.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/videodev2.h>
#include <media/video-buf.h>

#include "avia_gt.h"
#include "avia_gt_pig.h"

#define AVIA_GT_V4L2_DRIVER	"avia"
#define AVIA_GT_V4L2_CARD	"AViA eNX/GTX"
#define AVIA_GT_V4L2_BUS_INFO	"AViA core"
#define AVIA_GT_V4L2_VERSION	KERNEL_VERSION(0,1,14)

static
int avia_gt_v4l2_open(struct inode *inode, struct file *file)
{
	dprintk("avia_gt_v4l2: open\n");

	return 0;
}

static
int avia_gt_v4l2_release(struct inode *inode, struct file *file)
{
	dprintk("avia_gt_v4l2: close\n");

	avia_gt_pig_hide(0);

	return 0;
}

static
ssize_t avia_gt_v4l2_read(struct file *file, char *data, size_t count, loff_t *ppos)
{
	dprintk("avia_gt_v4l2: read\n");

	return 0;
}

/*  The arguments are already copied into kernel memory, so don't use copy_from_user() or copy_to_user() on arg.  */
static
int avia_gt_v4l2_ioctl(struct inode *inode, struct file *file, unsigned int cmd, void *arg)
{
	static struct v4l2_format fmt;

	dprintk("avia_gt_v4l2: ioctl\n");

	switch (cmd) {
	case VIDIOC_QUERYCAP:
	{
		struct v4l2_capability *cap = arg;
		memset(cap, 0, sizeof(struct v4l2_capability));

		strcpy(cap->driver, AVIA_GT_V4L2_DRIVER);
		strcpy(cap->card, AVIA_GT_V4L2_CARD);
		strcpy(cap->bus_info, AVIA_GT_V4L2_BUS_INFO);
		cap->version = AVIA_GT_V4L2_VERSION;
		cap->capabilities =
			V4L2_CAP_VIDEO_CAPTURE |
			V4L2_CAP_VIDEO_OVERLAY |
			V4L2_CAP_READWRITE |
			V4L2_CAP_STREAMING;
		break;
	}

	case VIDIOC_RESERVED:
		return -EOPNOTSUPP;

	case VIDIOC_ENUM_FMT:
	{
		struct v4l2_fmtdesc *f = arg;

		switch (f->type) {
		case V4L2_BUF_TYPE_VIDEO_CAPTURE:
			if (f->index != 0)
				return -EINVAL;
			memset(f, 0, sizeof(struct v4l2_fmtdesc));
			strcpy(f->description, "YCrCb");
			f->pixelformat = V4L2_PIX_FMT_NV21;
			break;
		default:
			return -EINVAL;
		}

		break;
	}

	case VIDIOC_G_FMT:
		return -EOPNOTSUPP;

	case VIDIOC_S_FMT:
	{
		struct v4l2_format *f = arg;

		fmt.fmt.win.w.left = f->fmt.win.w.left;
		fmt.fmt.win.w.top = f->fmt.win.w.top;
		fmt.fmt.win.w.width = f->fmt.win.w.width;
		fmt.fmt.win.w.height = f->fmt.win.w.height;

		break;
	}

	case VIDIOC_REQBUFS:
	case VIDIOC_QUERYBUF:
	case VIDIOC_G_FBUF:
	case VIDIOC_S_FBUF:
		return -EOPNOTSUPP;

	case VIDIOC_OVERLAY:
		if ((*((int *)arg))) {
			avia_gt_pig_set_pos(0, fmt.fmt.win.w.left, fmt.fmt.win.w.top);
			avia_gt_pig_set_size(0, fmt.fmt.win.w.width, fmt.fmt.win.w.height, 0);

			avia_gt_pig_set_stack(0, 2);
			return avia_gt_pig_show(0);
		}
		else {
			return avia_gt_pig_hide(0);
		}

	case VIDIOC_QBUF:
	case VIDIOC_DQBUF:
	case VIDIOC_STREAMON:
	case VIDIOC_STREAMOFF:
	case VIDIOC_G_PARM:
	case VIDIOC_S_PARM:
	case VIDIOC_G_STD:
	case VIDIOC_S_STD:
	case VIDIOC_ENUMSTD:
		return -EOPNOTSUPP;

	case VIDIOC_ENUMINPUT:
	{
		struct v4l2_input *input = (struct v4l2_input *)arg;

		if (input->index != 0)
			return -EINVAL;

		strcpy(input->name, "AViA eNX/GTX digital tv picture");
		input->type = V4L2_INPUT_TYPE_TUNER;
		input->audioset = 0;
		input->tuner = 0;
		input->std = V4L2_STD_PAL_BG | V4L2_STD_NTSC_M;
		input->status = 0;
		break;
	}

	case VIDIOC_G_CTRL:
	case VIDIOC_S_CTRL:
	case VIDIOC_G_TUNER:
	case VIDIOC_S_TUNER:
	case VIDIOC_G_AUDIO:
	case VIDIOC_S_AUDIO:
	case VIDIOC_QUERYCTRL:
	case VIDIOC_QUERYMENU:
		return -EOPNOTSUPP;

	case VIDIOC_G_INPUT:
		(*((int *)arg)) = 0;
		break;

	case VIDIOC_S_INPUT:
		if ((*((int *)arg)) != 0)
			return -EINVAL;
		break;

	case VIDIOC_G_OUTPUT:
	case VIDIOC_S_OUTPUT:
	case VIDIOC_ENUMOUTPUT:
	case VIDIOC_G_AUDOUT:
	case VIDIOC_S_AUDOUT:
	case VIDIOC_G_MODULATOR:
	case VIDIOC_S_MODULATOR:
	case VIDIOC_G_FREQUENCY:
	case VIDIOC_S_FREQUENCY:
	case VIDIOC_CROPCAP:
	case VIDIOC_G_CROP:
	case VIDIOC_S_CROP:
	case VIDIOC_G_JPEGCOMP:
	case VIDIOC_S_JPEGCOMP:
	case VIDIOC_QUERYSTD:
	case VIDIOC_TRY_FMT:
		return -EOPNOTSUPP;

	default:
		return -ENOIOCTLCMD;
	}

	return 0;
}

static
int avia_gt_v4l2_ioctl_prepare(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	return video_usercopy(inode, file, cmd, arg, avia_gt_v4l2_ioctl);
}

static int avia_gt_v4l2_mmap(struct file *file, struct vm_area_struct *vma)
{
	dprintk("avia_gt_v4l2: mmap\n");

	return 0;
}

static unsigned int avia_gt_v4l2_poll(struct file *file, struct poll_table_struct *wait)
{
	dprintk("avia_gt_v4l2: poll\n");

	return 0;
}

static struct file_operations device_fops = {
	.owner = THIS_MODULE,
	.open = avia_gt_v4l2_open,
	.release = avia_gt_v4l2_release,
	.read = avia_gt_v4l2_read,
	.poll = avia_gt_v4l2_poll,
	.mmap = avia_gt_v4l2_mmap,
	.ioctl = avia_gt_v4l2_ioctl_prepare,
	.llseek = no_llseek,
};

static struct video_device device_info = {
//	.owner =
	.name = AVIA_GT_V4L2_CARD,
//	.type =
//	.type2 =
//	.hardware =
	.minor = -1,
	.fops = &device_fops,
	.priv = NULL,
};

int __init avia_gt_v4l2_init(void)
{
	dprintk(KERN_INFO "avia_gt_v4l2: $Id: avia_gt_v4l2.c,v 1.9 2003/07/24 01:14:20 homar Exp $\n");

	return video_register_device(&device_info, VFL_TYPE_GRABBER, -1);
}

void __exit avia_gt_v4l2_exit(void)
{
	video_unregister_device(&device_info);
}

module_init(avia_gt_v4l2_init);
module_exit(avia_gt_v4l2_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("AViA eNX/GTX V4L2 driver");
MODULE_LICENSE("GPL");
