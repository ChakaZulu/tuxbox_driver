/*
 *   fp.c - FP driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
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

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/tqueue.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <linux/interrupt.h>

#include <asm/8xx_immap.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mpc8xx.h>
#include <asm/signal.h>
#include <asm/uaccess.h>

#include <dbox/event.h>
#include <dbox/fp.h>

#include <linux/devfs_fs_kernel.h>

#ifndef CONFIG_DEVFS_FS
#error no devfs
#endif

#include <dbox/dbox2_fp_core.h>
#include <dbox/dbox2_fp_irkbd.h>
#include <dbox/dbox2_fp_reset.h>
#include <dbox/dbox2_fp_sec.h>
#include <dbox/dbox2_fp_timer.h>
#include <dbox/dbox2_fp_tuner.h>


static devfs_handle_t devfs_handle[2];
static struct dbox_info_struct info;

static int debug = 0;
static int useimap = 1;

#define dprintk(fmt, args...) if (debug) printk(fmt, ##args)

/*
fp:
 03  deep standby
 10 led on
 11 led off
 dez.
 20 reboot
 21 reboot
 42 lcd off / led off ( alloff ;-) )
 ADDR VAL
 18   X0  X = dimm 0=off F=on
 22 off
*/

#define FP_INTERRUPT		SIU_IRQ2
#define I2C_FP_DRIVERID		0xF060
#define RCBUFFERSIZE		16
#define FP_GETID		0x1D

/* Scan 0x60 */
static unsigned short normal_i2c[] = { 0x60 >> 1, I2C_CLIENT_END };
static unsigned short normal_i2c_range[] = { 0x60 >> 1, 0x60 >> 1, I2C_CLIENT_END };
I2C_CLIENT_INSMOD;

static int fp_id=0;
static int rc_bcodes=0;

struct fp_data *defdata=0;

static u16 rcbuffer[RCBUFFERSIZE];
static u16 rcbeg, rcend;
static wait_queue_head_t rcwait;
static DECLARE_MUTEX_LOCKED(rc_opened);

static void fp_task (void * arg);

struct tq_struct fp_tasklet =
{
	routine:	fp_task,
	data:		0
};


static void fp_interrupt(int irq, void *dev, struct pt_regs * regs);




/*****************************************************************************\
 *   Generic Frontprocessor Functions
\*****************************************************************************/


struct dbox_info_struct *
fp_get_info (void)
{
	return &info;
}


struct i2c_client *
fp_get_i2c (void)
{
	return defdata->client;
}

int
fp_cmd (struct i2c_client * client, u8 cmd, u8 * res, u32 size)
{
	int ret;
	struct i2c_msg msg [] = { { addr: client->addr, flags: 0, buf: &cmd, len: 1 },
			   { addr: client->addr, flags: I2C_M_RD, buf: res, len: size } };

	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret != 2)
		printk("%s: i2c_transfer error (ret == %d)\n", __FUNCTION__, ret);

	if (debug) {

		int i;

		printk("fp.o: fp_cmd: %02x\n", cmd);
		printk("fp.o: fp_recv:");

		for (i = 0; i < size; i++)
			printk(" %02x", res[i]);

		printk("\n");
	}

	return 0;
}


int
fp_sendcmd (struct i2c_client * client, u8 b0, u8 b1)
{
	u8 cmd [] = { b0, b1 };

	dprintk("fp.o: fp_sendcmd: %02x %02x\n", b0, b1);

	if (i2c_master_send(client, cmd, sizeof(cmd)) != sizeof(cmd))
		return -1;

	return 0;
}


static int
fp_getid (struct i2c_client * client)
{
	u8 id [] = { 0x00, 0x00, 0x00 };

	if (fp_cmd(client, FP_GETID, id, sizeof(id)))
		return 0;

	return id[0];
}


static void
fp_add_event (int code)
{
	if (atomic_read(&rc_opened.count) >= 1)
		return;

	rcbuffer[rcend] = code;
	rcend++;

	if (rcend >= RCBUFFERSIZE)
		rcend = 0;

	if (rcbeg == rcend)
		printk("fp.o: RC overflow.\n");
	else
		wake_up(&rcwait);
}



/*****************************************************************************\
 *   File Operations
\*****************************************************************************/


static int
fp_ioctl (struct inode * inode, struct file * file, unsigned int cmd, unsigned long arg)
{
	int val;

	switch (cmd) {
	case FP_IOCTL_GETID:
		return fp_getid(defdata->client);

	case FP_IOCTL_POWEROFF:
		if (info.fpREV >= 0x80)
			return fp_sendcmd(defdata->client, 0x00, 0x03);
		else
			return fp_sendcmd(defdata->client, 0x00, 0x00);

	case FP_IOCTL_REBOOT:
		dbox2_fp_restart("");
		return 0;

	case FP_IOCTL_LCD_DIMM:
		if (copy_from_user(&val, (void*)arg, sizeof(val)) )
			return -EFAULT;

		if (info.fpREV >= 0x80)
			return fp_sendcmd(defdata->client, 0x18, val & 0xff);
		else
			return fp_sendcmd(defdata->client, 0x06, val & 0xff);

	case FP_IOCTL_LED:
		if (copy_from_user(&val, (void*)arg, sizeof(val)) )
			return -EFAULT;

		if (info.fpREV >= 0x80)
			return fp_sendcmd(defdata->client, 0x00, 0x10 | ((~val) & 1));
		else
			return fp_sendcmd(defdata->client, 0x10 | (val & 1), 0x00);
	
	case FP_IOCTL_GET_WAKEUP_TIMER:
		val = dbox2_fp_timer_get();

		if (val == -1)
			return -EIO;

		if (copy_to_user((void *) arg, &val, sizeof(val)))
			return -EFAULT;

		return 0;

	case FP_IOCTL_SET_WAKEUP_TIMER:
		if (copy_from_user(&val, (void *) arg, sizeof(val)) )
			return -EFAULT;

		return dbox2_fp_timer_set(val);

	case FP_IOCTL_IS_WAKEUP:
	{
		u8 is_wakeup = dbox2_fp_timer_get_boot_trigger();
		if (copy_to_user((void *) arg, &is_wakeup, sizeof(is_wakeup)))
			return -EFAULT;

		return 0;
	}

	case FP_IOCTL_GET_VCR:
		if (copy_to_user((void *) arg, &defdata->fpVCR, sizeof(defdata->fpVCR)))
			return -EFAULT;
		return 0;

	case FP_IOCTL_GET_REGISTER:
	{
		unsigned long foo;

		if (copy_from_user(&val, (void *) arg, sizeof(val)))
	                return -EFAULT;

		fp_cmd(defdata->client, val & 0xFF, (u8 *) &foo, ((val >> 8) & 3) + 1);

		if (copy_to_user((void*)arg, &foo, sizeof(foo)))
			return -EFAULT;
		
		return 0;
	}

	default:
		return -EINVAL;
	}
}


static int
rc_ioctl (struct inode * inode, struct file * file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case RC_IOCTL_BCODES:
		if (arg > 0)
			rc_bcodes = 1;
		else
			rc_bcodes = 0;

		return fp_sendcmd(defdata->client, 0x26, rc_bcodes ? 0x80 : 0x00);

	default:
		return -EINVAL;
	}
}


static ssize_t
rc_read (struct file * file, char * buf, size_t count, loff_t * offset)
{
	int i;
	unsigned int read = 0;
	DECLARE_WAITQUEUE(wait, current);

	for (;;) {

		if (rcbeg == rcend) {

			if (file->f_flags & O_NONBLOCK)
				return read;

			add_wait_queue(&rcwait, &wait);
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
			current->state = TASK_RUNNING;
			remove_wait_queue(&rcwait, &wait);

			if (signal_pending(current))
				return -ERESTARTSYS;

			continue;
		}

		break;
	}

	count &= ~1;

	for (i = 0; i < count; i += 2) {

		if (rcbeg == rcend)
			break;

		*((u16*)(buf+i)) = rcbuffer[rcbeg++];
		read += 2;

		if (rcbeg >= RCBUFFERSIZE)
			rcbeg = 0;
	}

	return read;
}


static int
fp_open (struct inode * inode, struct file * file)
{
	return 0;
}


static int
rc_open (struct inode * inode, struct file * file)
{
	if (file->f_flags & O_NONBLOCK) {
		if (down_trylock(&rc_opened))
			return -EAGAIN;
	}

	else {
		if (down_interruptible(&rc_opened))
			return -ERESTARTSYS;
	}

	return 0;
}


static int
fp_release (struct inode * inode, struct file * file)
{
	return 0;
}


static int
rc_release (struct inode * inode, struct file * file)
{
	if (rc_bcodes != 0)
		fp_sendcmd(defdata->client, 0x26, 0);

	up(&rc_opened);
	return 0;
}


static unsigned int
rc_poll (struct file * file, poll_table * wait)
{
	poll_wait(file, &rcwait, wait);

	if (rcbeg != rcend)
		return POLLIN | POLLRDNORM;
	
	return 0;
}


static struct
file_operations fp_fops =
{
	owner:			THIS_MODULE,
	llseek:			NULL,
	read:			NULL,
	write:			NULL,
	readdir:		NULL,
	poll:			NULL,
	ioctl:			fp_ioctl,
	mmap:			NULL,
	open:			fp_open,
	flush:			NULL,
	release:		fp_release,
	fsync:			NULL,
	fasync:			NULL,
	lock:			NULL,
	readv:			NULL,
	writev:			NULL,
	sendpage:		NULL,
	get_unmapped_area:	NULL
};


static struct
file_operations rc_fops =
{
	owner:			THIS_MODULE,
	llseek:			NULL,
	read:			rc_read,
	write:			NULL,
	readdir:		NULL,
	poll:			rc_poll,
	ioctl:			rc_ioctl,
	mmap:			NULL,
	open:			rc_open,
	flush:			NULL,
	release:		rc_release,
	fsync:			NULL,
	fasync:			NULL,
	lock:			NULL,
	readv:			NULL,
	writev:			NULL,
	sendpage:		NULL,
	get_unmapped_area:	NULL
};




/*****************************************************************************\
 *   I2C Functions
\*****************************************************************************/


static struct
i2c_driver fp_driver;


static int
fp_detach_client (struct i2c_client * client)
{
	int err;

	free_irq(FP_INTERRUPT, client->data);

	if ((err=i2c_detach_client(client))) {
		dprintk("fp.o: couldn't detach client driver.\n");
		return err;
	}

	kfree(client);
	return 0;
}


static int
fp_detect_client (struct i2c_adapter * adapter, int address, unsigned short flags, int kind)
{
	int err = 0;
	struct i2c_client *new_client;
	struct fp_data *data;
	const char *client_name = "DBox2 Frontprocessor client";

	if (!(new_client = kmalloc(sizeof(struct i2c_client)+sizeof(struct fp_data), GFP_KERNEL)))
		return -ENOMEM;

	new_client->data = new_client+1;
	defdata = data = (struct fp_data *) new_client->data;

	/* init vcr value (off) */
	defdata->fpVCR = 0;
	rcbeg = 0;
	rcend = 0;
	new_client->addr = address;
	data->client = new_client;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &fp_driver;
	new_client->flags = 0;

	if (kind < 0) {

		int fpid;
		/*u8 buf[2];*/
		immap_t *immap = (immap_t *) IMAP_ADDR;

		/*
		 * FP ID
		 * -------------
		 * NOKIA  : 0x5A
		 * SAGEM  : 0x52
		 * PHILIPS: 0x52
		 */

		fpid = fp_getid(new_client);

		if ((fpid != 0x52) && (fpid != 0x5a)) {

			dprintk("fp.o: bogus fpID %d\n", fpid);
			kfree(new_client);
			return -1;

		}

		if (useimap) {

			immap->im_ioport.iop_papar &= ~2;
			immap->im_ioport.iop_paodr &= ~2;
			immap->im_ioport.iop_padir |= 2;
			immap->im_ioport.iop_padat &= ~2;

		}

		/* sagem needs this (71) LNB-Voltage 51-V 71-H */
		/*
		fp_sendcmd(new_client, 0x04, 0x51);
		*/

		/*
		fp_sendcmd(new_client, 0x22, 0xbf);
		fp_cmd(new_client, 0x25, buf, 2);
		fp_sendcmd(new_client, 0x19, 0x04);
		fp_sendcmd(new_client, 0x18, 0xb3);
		fp_cmd(new_client, 0x1e, buf, 2);
		*/

		/* disable (non-working) break code */
		fp_sendcmd(new_client, 0x26, 0x00);

		/*
		fp_cmd(new_client, 0x23, buf, 1);
		fp_cmd(new_client, 0x20, buf, 1);
		fp_cmd(new_client, 0x01, buf, 2);
		*/

	}

	strcpy(new_client->name, client_name);
	new_client->id = fp_id++;

	if ((err = i2c_attach_client(new_client))) {
		kfree(new_client);
		return err;
	}

	if (request_8xxirq(FP_INTERRUPT, fp_interrupt, SA_ONESHOT, "fp", data) != 0)
		panic("Could not allocate FP IRQ!");

	up(&rc_opened);
	return 0;
}


static int
fp_attach_adapter (struct i2c_adapter * adapter)
{
	return i2c_probe(adapter, &addr_data, &fp_detect_client);
}


static struct
i2c_driver fp_driver =
{
	name:		"DBox2 Frontprocessor driver",
	id:		I2C_FP_DRIVERID,
	flags:		I2C_DF_NOTIFY,
	attach_adapter:	&fp_attach_adapter,
	detach_client:	&fp_detach_client,
	command:	0,
	inc_use:	0,
	dec_use:	0
};




/*****************************************************************************\
 *   Interrupt Handler Functions
\*****************************************************************************/


static void
fp_handle_frontpanel_button (struct fp_data * dev)
{
	u8 rc;

	fp_cmd(dev->client, 0x25, (u8*)&rc, sizeof(rc));
	fp_add_event(0xFF00 | rc);
}


static void
fp_handle_ir_rc (struct fp_data * dev)
{
	u16 rc;

	switch (info.mID) {
	case DBOX_MID_NOKIA:
		fp_cmd(dev->client, 0x01, (u8*)&rc, sizeof(rc));
		break;
	case DBOX_MID_PHILIPS:
	case DBOX_MID_SAGEM:
		fp_cmd(dev->client, 0x26, (u8*)&rc, sizeof(rc));
		break;
	}

	fp_add_event(rc);
}


static void
fp_handle_vcr (struct fp_data * dev, int fpVCR)
{
	struct event_t event;

	memset(&event, 0x00, sizeof(event));

	if (dev->fpVCR!=fpVCR) {

		dev->fpVCR = fpVCR;
		event.event = EVENT_VCR_CHANGED;
		event_write_message(&event, 1);

	}
}


#if 0
static void
fp_handle_unknown (struct fp_data * dev)
{
	u8 rc;

	fp_cmd(dev->client, 0x24, (u8*)&rc, 1);
	dprintk("fp.o: misterious interrupt source 0x40: %x\n", rc);
}
#endif


static void
fp_check_queues (void)
{
	u8 status;

	dprintk("fp.o: checking queues.\n");
	fp_cmd(defdata->client, 0x23, &status, 1);

	if (defdata->fpVCR != status)
		fp_handle_vcr(defdata, status);

/*
 * fp status:
 *
 * 0x01	ir remote control (dbox1, old dbox2)
 * 0x02	ir keyboard
 * 0x04	ir mouse
 * 0x08	ir remote control (new dbox2)
 *
 * 0x10	frontpanel button
 * 0x20	scart status
 * 0x40	lnb alarm
 * 0x80	timer underrun
 */

	do {
		fp_cmd(defdata->client, 0x20, &status, 1);

		dprintk("status: %02x\n", status);

		if (status & 0x01)
			fp_handle_ir_rc(defdata);

		if (status & 0x02)
			dbox2_fp_handle_ir_keyboard(defdata);

		if (status & 0x04)
			dbox2_fp_handle_ir_mouse(defdata);

		if (status & 0x08)
			fp_handle_ir_rc(defdata);

		if (status & 0x10)
			fp_handle_frontpanel_button(defdata);
#if 0
		if (status & 0x20)
			fp_handle_unknown(defdata);

		if (status & 0x40)
			fp_handle_unknown(defdata);

		if (status & 0x80)
			fp_handle_unknown(defdata);
#endif

	} while (status & 0x1F);

}


static void
fp_task (void * arg)
{
	immap_t *immap=(immap_t*)IMAP_ADDR;

	fp_check_queues();

	if (useimap)
		immap->im_ioport.iop_padat &= ~2;

	enable_irq(FP_INTERRUPT);
}


static void
fp_interrupt(int irq, void * vdev, struct pt_regs * regs)
{
	immap_t *immap = (immap_t *) IMAP_ADDR;

	if (useimap)
		immap->im_ioport.iop_padat |= 2;

	schedule_task(&fp_tasklet);
	return;
}



/*****************************************************************************\
 *   Module Initialization / Module Cleanup
\*****************************************************************************/


static int
__init fp_init (void)
{
	int res;
	/*int i;*/

	dbox_get_info(&info);
	init_waitqueue_head(&rcwait);

	if ((res = i2c_add_driver(&fp_driver))) {
		dprintk("fp.o: Driver registration failed, module not inserted.\n");
		return res;
	}

	if (!defdata) {
		i2c_del_driver(&fp_driver);
		dprintk("fp.o: Couldn't find FP.\n");
		return -EBUSY;
	}

	devfs_handle[FP_MINOR] =
		devfs_register(NULL, "dbox/fp0", DEVFS_FL_DEFAULT, 0, FP_MINOR,
			S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
			&fp_fops, NULL);

	if (!devfs_handle[FP_MINOR]) {
		i2c_del_driver(&fp_driver);
		return -EIO;
	}

	devfs_handle[RC_MINOR] =
		devfs_register(NULL, "dbox/rc0", DEVFS_FL_DEFAULT, 0, RC_MINOR,
			S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
			&rc_fops, NULL);

	if (!devfs_handle[RC_MINOR]) {
		devfs_unregister(devfs_handle[FP_MINOR]);
		i2c_del_driver(&fp_driver);
		return -EIO;
	}

	ppc_md.restart			= dbox2_fp_restart;
	ppc_md.power_off		= dbox2_fp_power_off;
	ppc_md.halt			= dbox2_fp_power_off;

	/* keyboard */
	ppc_md.kbd_setkeycode		= dbox2_fp_irkbd_setkeycode;
	ppc_md.kbd_getkeycode		= dbox2_fp_irkbd_getkeycode;
	ppc_md.kbd_translate		= dbox2_fp_irkbd_translate;
	ppc_md.kbd_unexpected_up	= dbox2_fp_irkbd_unexpected_up;
	ppc_md.kbd_leds			= dbox2_fp_irkbd_leds;
	ppc_md.kbd_init_hw		= dbox2_fp_irkbd_init_hw;
#ifdef CONFIG_MAGIC_SYSRQ
	ppc_md.kbd_sysrq_xlate		= keymap;
#endif

	dbox2_fp_irkbd_init();
	dbox2_fp_reset_init();
	dbox2_fp_sec_init();
	dbox2_fp_timer_init();
	dbox2_fp_tuner_init();

	return 0;
}


static void
__exit fp_exit (void)
{
	if (i2c_del_driver(&fp_driver)) {
		dprintk("fp.o: Driver unregistration failed, module not removed.\n");
		return;
	}

	devfs_unregister(devfs_handle[FP_MINOR]);
	devfs_unregister(devfs_handle[RC_MINOR]);

	if (ppc_md.restart == dbox2_fp_restart)
		ppc_md.restart = NULL;

	if (ppc_md.power_off == dbox2_fp_power_off)
		ppc_md.power_off = NULL;

	if (ppc_md.halt == dbox2_fp_power_off)
		ppc_md.halt = NULL;

	if (ppc_md.kbd_setkeycode == dbox2_fp_irkbd_setkeycode)
		ppc_md.kbd_setkeycode = NULL;

	if (ppc_md.kbd_getkeycode == dbox2_fp_irkbd_getkeycode)
		ppc_md.kbd_getkeycode = NULL;

	if (ppc_md.kbd_translate == dbox2_fp_irkbd_translate)
		ppc_md.kbd_translate = NULL;

	if (ppc_md.kbd_unexpected_up == dbox2_fp_irkbd_unexpected_up)
		ppc_md.kbd_unexpected_up = NULL;

	if (ppc_md.kbd_leds == dbox2_fp_irkbd_leds)
		ppc_md.kbd_leds = NULL;

	if (ppc_md.kbd_init_hw == dbox2_fp_irkbd_init_hw)
		ppc_md.kbd_init_hw = NULL;

#ifdef CONFIG_MAGIC_SYSRQ
	if (ppc_md.kbd_sysrq_xlate == dbox2_fp_irkbd_sysrq_xlate)
		ppc_md.kbd_sysrq_xlate = NULL;
#endif
	dbox2_fp_irkbd_exit();
	/*
	dbox2_fp_reset_exit();
	dbox2_fp_sec_exit();
	*/
}




#ifdef MODULE
module_init(fp_init);
module_exit(fp_exit);

MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("DBox2 Frontprocessor");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
MODULE_PARM(debug,"i");
MODULE_PARM(useimap,"i");
#endif

