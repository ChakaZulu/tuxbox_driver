/*
 * AViA GTX/eNX demux and ethernet "busmaster" driver.
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2004 Wolfram Joost <dbox2@frokaschwei.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
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
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/bitops.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <asm/io.h>
#include <asm/checksum.h>
#include <asm/uaccess.h>

#include <dbox/dvb2eth.h>

EXPORT_NO_SYMBOLS;

/*
 * supported file operations
 */

static int dvb2eth_open(struct inode *inode, struct file *file);
static int dvb2eth_release(struct inode *inode, struct file *file);
static int dvb2eth_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg);

static struct file_operations dvb2eth_fops = {
		THIS_MODULE,
		NULL,						/* llseek */
		NULL,						/* read */
		NULL,						/* write */
		NULL,						/* readdir */
		NULL,           			/* poll */
		dvb2eth_ioctl,				/* ioctl */
		NULL,						/* mmap */
		dvb2eth_open,				/* open */
		NULL,						/* flush */
		dvb2eth_release,			/* release */
		NULL,						/* fsync */
		NULL,						/* fasync */
		NULL,						/* lock */
		NULL,						/* readv */
		NULL,						/* writev */
		NULL,						/* sendpage */
		NULL						/* get_unmapped_area */
};

/*
 * UDP header skeleton
 */

unsigned char dvb2eth_udp_header_skel[44] =
{
		0x00, 0x00,								// alignment
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		// destination mac address
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		// source mac address
		0x08, 0x00,								// type ip
		0x45,									// ip-version and header-length
		0x00,									// type-of-service
		0x05, 0x40,								// total length 1344
		0x00, 0x00,								// identification
		0x00, 0x00,								// flags, fragment offset
		0x40,									// time to live
		0x11,									// protocol udp
		0x00, 0x00,								// header checksum
		0x00, 0x00, 0x00, 0x00,					// source ip-address
		0x00, 0x00, 0x00, 0x00,					// destination ip-address
		0x00, 0x00,								// source-port
		0x00, 0x00,								// destination-port
		0x05, 0x2C,								// data-length + udp-header-length
		0x00, 0x00								// udp-checksum, not used
};

#define UDP_HEADERS		32

/*
 * variables
 */

static devfs_handle_t		dvb2eth_device;
static unsigned char		*dvb2eth_header_buf;
static unsigned char		dvb2eth_busy;
static unsigned	char		dvb2eth_header_index;
static unsigned char		dvb2eth_running;
static unsigned char		dvb2eth_setup_done;
static unsigned short		dvb2eth_udp_id = 15796;
static dma_addr_t			dvb2eth_ph_addr[UDP_HEADERS];
static struct net_device 	*dvb2eth_dev;

/*
 * interface to the ethernet-driver
 */


extern int scc_enet_multiple_xmit(struct net_device *dev,unsigned count,
		unsigned first, unsigned first_len, unsigned second,
		unsigned second_len, unsigned third, unsigned third_len);

/*
 * the send-routine
 */

static int dvb2eth_send(unsigned first, unsigned first_len,
								unsigned second, unsigned second_len)
{
	unsigned len = first_len + second_len;
	unsigned sent = 0;
	unsigned char *udp;
	unsigned s_len;

	if (len < 1316)
	{
		return 0;
	}

	while (len >= 1316)
	{
		/*
		 * create udp header
		 */

		udp = dvb2eth_header_buf +
			sizeof(dvb2eth_udp_header_skel) * dvb2eth_header_index;
		*((unsigned short *) (udp + 20)) = dvb2eth_udp_id++;
		*((unsigned short *) (udp + 26)) = 0;
		*((unsigned short *) (udp + 26)) = ip_fast_csum(udp + 16,5);

		if (first_len >= 1316)
		{
			scc_enet_multiple_xmit(dvb2eth_dev,2,
				dvb2eth_ph_addr[dvb2eth_header_index],
				sizeof(dvb2eth_udp_header_skel) - 2,
				first,1316,0,0);
			first += 1316;
			first_len -= 1316;
		}
		else
		{
			scc_enet_multiple_xmit(dvb2eth_dev,3,
				dvb2eth_ph_addr[dvb2eth_header_index],
				sizeof(dvb2eth_udp_header_skel) - 2,
				first,first_len,
				second,(s_len = 1316 - first_len));
			second += s_len;
			second_len -= s_len;
			first_len = 0;
		}
		if (!first_len)
		{
			first_len = second_len;
			first = second;
			second_len = 0;
			second = 0;
		}
		len -= 1316;
		sent += 1316;
		dvb2eth_header_index = (dvb2eth_header_index + 1) & (UDP_HEADERS - 1);
	}

	return sent;
}

/*
 * ioctl handler
 */

static int dvb2eth_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	struct dvb2eth_dest	dest;
	unsigned i;
	unsigned char *j;

	if (_IOC_TYPE(cmd) != DVB2ETH_IOCTL_BASE)
	{
		return -EINVAL;
	}

	switch (cmd)
	{
		case DVB2ETH_STOP:
			avia_gt_napi_dvr_send = NULL;
			dvb2eth_running = 0;
			break;
		case DVB2ETH_START:
			if (!dvb2eth_setup_done)
			{
				return -EINVAL;
			}
			avia_gt_napi_dvr_send = dvb2eth_send;
			dvb2eth_running = 1;
			break;
		case DVB2ETH_SET_DEST:
			if (dvb2eth_running)
			{
				return -EINVAL;
			}
			if (copy_from_user(&dest,(unsigned char *) arg,sizeof(dest)))
			{
				return -EFAULT;
			}

			dvb2eth_udp_header_skel[24] = dest.ttl;
			*((unsigned long *) (dvb2eth_udp_header_skel + 32)) = dest.dest_ip;
			*((unsigned short *) (dvb2eth_udp_header_skel + 36)) = dest.src_port;
			*((unsigned short *) (dvb2eth_udp_header_skel + 38)) = dest.dest_port;
			memcpy(dvb2eth_udp_header_skel + 2,dest.dest_mac,6);

			memcpy(dvb2eth_udp_header_skel + 8,dvb2eth_dev->dev_addr,6);
			*((unsigned long *) (dvb2eth_udp_header_skel + 28)) =
				((struct in_device *) (dvb2eth_dev->ip_ptr))->ifa_list->ifa_address;
			for (i = 0, j = dvb2eth_header_buf; i < UDP_HEADERS;
				 i++, j += sizeof(dvb2eth_udp_header_skel))
			{
				memcpy(j,dvb2eth_udp_header_skel,sizeof(dvb2eth_udp_header_skel));
			}
			dvb2eth_setup_done = 1;
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

/*
 * device open
 */

static int dvb2eth_open(struct inode *inode, struct file *file)
{
	/*
	 * already in use?
	 */

	if (test_and_set_bit(0,&dvb2eth_busy))
	{
		return -EBUSY;
	}

	/*
	 * opened with write permissions?
	 */

	if (!(file->f_mode & FMODE_WRITE))
	{
		clear_bit(0,&dvb2eth_busy);
		return -EINVAL;
	}

	MOD_INC_USE_COUNT;

	return 0;
}

/*
 * device close
 */

static int dvb2eth_release(struct inode *inode, struct file *file)
{
	/*
	 * stop any operations
	 */

	avia_gt_napi_dvr_send = NULL;
	dvb2eth_setup_done = 0;
	dvb2eth_running = 0;

	/*
	 * reset use-bit
	 */

	clear_bit(0,&dvb2eth_busy);

	MOD_DEC_USE_COUNT;

	return 0;
}

/*
 * module-exit
 */

static void __exit dvb2eth_exit(void)
{
	/*
	 * free memory
	 */


	consistent_free(dvb2eth_header_buf);

	/*
	 * unregister device
	 */

	devfs_unregister(dvb2eth_device);
}

/*
 * module-init
 */

static int __init dvb2eth_init(void)
{
	unsigned 	i;
	dma_addr_t	phy_addr;
	/*
	 * find network device
	 */

	if ( (dvb2eth_dev = dev_get_by_name("eth0")) == NULL)
	{
		printk(KERN_ERR "Cannot find device eth0.\n");
		return -EINVAL;
	}

	/*
	 * register device
	 */

	if ( (dvb2eth_device = devfs_register(NULL,"dbox/dvb2eth", DEVFS_FL_DEFAULT,
		0, 0, S_IFCHR | S_IRUSR | S_IWUSR, &dvb2eth_fops, NULL)) == NULL)
	{
		printk(KERN_ERR "dvb2eth_init: cannot register device in devfs.\n");
		return -EIO;
	}

	/*
	 * alloc memory
	 */

	if ( (dvb2eth_header_buf = (unsigned char *) consistent_alloc(GFP_KERNEL,
			UDP_HEADERS * sizeof(dvb2eth_udp_header_skel),&phy_addr)) == NULL)
	{
		printk(KERN_ERR "dvb2eth_init: cannot allocate memory.\n");
		devfs_unregister(dvb2eth_device);
		return -ENOMEM;
	}

	/*
	 * store physical addresses for udp-headers
	 */

	phy_addr += 2;
	for (i = 0; i < UDP_HEADERS; i++)
	{
		dvb2eth_ph_addr[i] = phy_addr;
		phy_addr += sizeof(dvb2eth_udp_header_skel);
	}

	return 0;
}

/*
 * module stuff
 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wolfram Joost");
MODULE_DESCRIPTION("provides udp-ts-streaming with minimal cpu load");
module_init(dvb2eth_init);
module_exit(dvb2eth_exit);
