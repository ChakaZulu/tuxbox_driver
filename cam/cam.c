/*
 *   cam.c - CAM driver (dbox-II-project)
 *
 *   Homepage: http://www.tuxbox.org
 *
 *   Copyright (C) 2001 Felix "tmbinc" Domke (tmbinc@gmx.net)
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
 */

#define __KERNEL_SYSCALLS__

#include <linux/string.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <linux/fcntl.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/poll.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>

#include <dbox/fp.h>

static int debug;
static int mio;
static char *firmware;

#define dprintk(fmt,args...) if(debug) printk( fmt,## args)

#define I2C_DRIVERID_CAM	0x6E
#define CAM_INTERRUPT		SIU_IRQ3
#define CAM_CODE_SIZE		0x20000
#define CAM_QUEUE_SIZE		0x800

static int cam_attach_adapter(struct i2c_adapter *adap);
static int cam_detach_client(struct i2c_client *client);

static struct i2c_client *dclient;

static struct i2c_driver cam_driver = {
	.name = "DBox2-CAM",
	.id = I2C_DRIVERID_CAM,
	.flags = I2C_DF_NOTIFY,
	.attach_adapter = cam_attach_adapter,
	.detach_client = cam_detach_client,
	.command = NULL,
	.inc_use = NULL,
	.dec_use = NULL,
};

static struct i2c_client client_template = {
	.name = "DBox2-CAM",
	.id = I2C_DRIVERID_CAM,
	.flags = 0,
	.addr = (0x6E >> 1),
	.adapter = NULL,
	.driver = &cam_driver,
	.data = NULL,
	.usage_count = 0,
};

static DECLARE_MUTEX(cam_busy);
static u8 cam_queue[CAM_QUEUE_SIZE];
static u32 cam_queuewptr, cam_queuerptr;
static wait_queue_head_t cam_wait_queue;

/**
 * I2C functions
 */
static int cam_attach_adapter(struct i2c_adapter *adap)
{
	struct i2c_client *client;
	int ret;

	if (!(client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
		return -ENOMEM;

	memcpy(client, &client_template, sizeof(struct i2c_client));

	client->adapter = adap;
	client->data = NULL;

	if ((ret = i2c_attach_client(client))) {
		kfree(client);
		return ret;
	}

	/* ugly */
	dclient = client;

	return 0;
}

static int cam_detach_client(struct i2c_client *client)
{
	int ret;

	if ((ret = i2c_detach_client(client))) {
		printk(KERN_ERR "cam: i2c_detach_client failed\n");
		return ret;
	}

	kfree(client);
	return 0;
}

/**
 * Exported functions
 */
unsigned int cam_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(file, &cam_wait_queue, wait);

	if (cam_queuerptr != cam_queuewptr)
		mask |= (POLLIN | POLLRDNORM);

	return mask;
}

int cam_read_message(char *buf, size_t count)
{
	int cb;

	cb = cam_queuewptr - cam_queuerptr;

	if (cb < 0)
		cb += CAM_QUEUE_SIZE;

	if (count < cb)
		cb = count;

	if ((cam_queuerptr + cb) > CAM_QUEUE_SIZE) {
		memcpy(buf, &cam_queue[cam_queuerptr], CAM_QUEUE_SIZE - cam_queuerptr);
		memcpy(&buf[CAM_QUEUE_SIZE - cam_queuerptr], cam_queue, cb - (CAM_QUEUE_SIZE - cam_queuerptr));
		cam_queuerptr = cb - (CAM_QUEUE_SIZE - cam_queuerptr);
	}
	else {
		memcpy(buf, &cam_queue[cam_queuerptr], cb);
		cam_queuerptr += cb;
	}

	return cb;
}

int cam_reset(void)
{
	return dbox2_fp_reset_cam();
}

int cam_write_message(char *buf, size_t count)
{
	int res;

	/* ugly */
	if (!dclient)
		return -ENODEV;

	if ((res = down_interruptible(&cam_busy)))
		return res;

	res = i2c_master_send(dclient, buf, count);

	cam_queuewptr = cam_queuerptr;  // mich stoerte der Buffer ....		// ?? was soll das? (tmb)
	up(&cam_busy);
	return res;
}

/**
 * IRQ functions
 */
static void cam_task(void *data)
{
	unsigned char buffer[130];
	unsigned char caid[9] = { 0x50, 0x06, 0x23, 0x84, 0x01, 0x02, 0xFF, 0xFF, 0x00 };
	int len, i;

	if (down_interruptible(&cam_busy))
		goto cam_task_enable_irq;

	if (i2c_master_recv(dclient, buffer, 2) != 2)
		goto cam_task_up;

	len = buffer[1] & 0x7f;

	if (i2c_master_recv(dclient, buffer, len + 3) != len + 3)
		goto cam_task_up;

	if ((buffer[1] & 0x7f) != len) {
		len = buffer[1] & 0x7f;	/* length mismatch - try again */
		if (i2c_master_recv(dclient, buffer, len + 3) != len + 3)
			goto cam_task_up;
	}

	len += 3;

	if ((buffer[2] == 0x23) && (buffer[3] <= 7)) {
		memcpy(&caid[6], &buffer[5], 2);
		caid[8] = 0x6e;	/* checksum */
		for (i = 0; i < 8; i++)
			caid[8] ^= caid[i];
		up(&cam_busy);
		cam_write_message(caid, 9);
	}
	else {
		i = cam_queuewptr - cam_queuerptr;

		if (i < 0)
			i += CAM_QUEUE_SIZE;

		i = CAM_QUEUE_SIZE - i;

		if (i < len) {
			cam_queuewptr = cam_queuerptr;
		}
		else {
			i = 0;
			cam_queue[cam_queuewptr++] = 0x6f;
			if (cam_queuewptr == CAM_QUEUE_SIZE)
				cam_queuewptr = 0;
			while (len--) {
				cam_queue[cam_queuewptr++] = buffer[i++];
				if (cam_queuewptr == CAM_QUEUE_SIZE)
					cam_queuewptr = 0;
			}
		}
	}

	wake_up(&cam_wait_queue);
cam_task_up:
	up(&cam_busy);
cam_task_enable_irq:
	enable_irq(CAM_INTERRUPT);
}

static struct tq_struct cam_tasklet = {
	.routine = cam_task,
	.data = NULL,
};

static void cam_interrupt(int irq, void *dev, struct pt_regs *regs)
{
	schedule_task(&cam_tasklet);
}

/**
 * Firmware functions
 */
static int do_firmwrite(void *firmware, size_t len)
{
	volatile immap_t *immap = (volatile immap_t *)IMAP_ADDR;
	volatile cpm8xx_t *cp = &immap->im_cpm;
	unsigned char *code_base;
	int i;

	if (len > CAM_CODE_SIZE) {
		printk(KERN_ERR "cam: firmware file is too large\n");
		return -EINVAL;
	}

	if (!(code_base = ioremap_nocache(mio, CAM_CODE_SIZE))) {
		printk(KERN_ERR "cam: could not map mio=0x%08X\n", mio);
		return -EFAULT;
	}

	/* 0xabc */
	cp->cp_pbpar &= ~0x0000000f;	// GPIO (not cpm-io)

	/* 0xac0 */
	cp->cp_pbodr &= ~0x0000000f;	// driven output (not tristate)

	/* 0xab8 */
	cp->cp_pbdir |= 0x0000000f;	// output (not input)

	/* 0xac4 */
	(void)cp->cp_pbdat;
	cp->cp_pbdat = 0x00000000;
	cp->cp_pbdat = 0x000000a5;
	cp->cp_pbdat |= 0x000000f;
	cp->cp_pbdat &= ~0x00000002;
	cp->cp_pbdat |= 0x00000002;

	for (i = 0; i <= 8; i++) {
		cp->cp_pbdat &= ~0x00000008;
		cp->cp_pbdat |= 0x00000008;
	}

	cam_reset();

	cp->cp_pbdat &= ~0x00000001;

	if ((firmware) && (len))
		memcpy(code_base, firmware, len);
	if (CAM_CODE_SIZE - len)
		memset(&code_base[len], 0x5a, CAM_CODE_SIZE - len);

	wmb();
	cp->cp_pbdat |= 0x00000001;
	wmb();

	cp->cp_pbdat &= ~0x00000002;
	cp->cp_pbdat |= 0x00000002;

	cam_reset();

	iounmap((void *)code_base);

	return 0;
}

static int errno;

static int do_firmread(const char *fn, char **fp)
{
	/* shameless stolen from sound_firmware.c */
	int fd;
	long l;
	char *dp;

	fd = open(fn, 0, 0);

	if (fd == -1) {
		dprintk(KERN_ERR "cam.o: Unable to load '%s'.\n", firmware);
		return 0;
	}

	l = lseek(fd, 0L, 2);

	if (l <= 0) {
		dprintk(KERN_ERR "cam.o: Firmware wrong size '%s'.\n", firmware);
		sys_close(fd);
		return 0;
	}

	lseek(fd, 0L, 0);

	dp = vmalloc(l);

	if (dp == NULL) {
		dprintk(KERN_ERR "cam.o: Out of memory loading '%s'.\n", firmware);
		sys_close(fd);
		return 0;
	}

	if (read(fd, dp, l) != l) {
		dprintk(KERN_ERR "cam.o: Failed to read '%s'.%d\n", firmware,errno);
		vfree(dp);
		sys_close(fd);
		return 0;
	}

	close(fd);

	*fp = dp;

	return (int) l;
}

/**
 * init/exit functions
 */
static int __init cam_init(void)
{
	int res;
	mm_segment_t fs;
	char *microcode;
	int len;

	printk(KERN_INFO "$Id: cam.c,v 1.29 2003/11/20 21:23:26 obi Exp $\n");

	if (!mio) {
		printk(KERN_ERR "cam: mio address unknown\n");
		return -EINVAL;
	}

	init_waitqueue_head(&cam_wait_queue);

	/* load microcode */
	fs = get_fs();
	set_fs(get_ds());

	/* read firmware */
	len = do_firmread(firmware, &microcode);
	set_fs(fs);

	/* if we don't have any firmware, then we fill up the memory with
	 * a 0x5a5a5a5a bit pattern... */
	if (do_firmwrite(microcode, len) < 0) {
		vfree(microcode);
		return -EIO;
	}

	if (len) {
		vfree(microcode);

		if ((res = i2c_add_driver(&cam_driver))) {
			printk(KERN_ERR "cam: i2c driver registration failed\n");
			return res;
		}
	}
	else {
		printk(KERN_NOTICE "cam: no firmware file found\n");
	}

	if (request_irq(CAM_INTERRUPT, cam_interrupt, SA_ONESHOT, "cam", THIS_MODULE) != 0)
		panic("cam: could not allocate irq");

	return 0;
}

static void __exit cam_exit(void)
{
	free_irq(CAM_INTERRUPT, THIS_MODULE);
	i2c_del_driver(&cam_driver);
	do_firmwrite(NULL, 0);
}

module_init(cam_init);
module_exit(cam_exit);

EXPORT_SYMBOL(cam_poll);
EXPORT_SYMBOL(cam_read_message);
EXPORT_SYMBOL(cam_reset);
EXPORT_SYMBOL(cam_write_message);

MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("DBox2 CAM Driver");
MODULE_PARM(debug,"i");
MODULE_PARM(mio,"i");
MODULE_PARM(firmware,"s");
MODULE_LICENSE("GPL");
