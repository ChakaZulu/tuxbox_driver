/*
 *   mpc8xx_wdt.c - MPC8xx watchdog driver (dbox-II-project)
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
 *   $Log: mpc8xx_wdt.c,v $
 *   Revision 1.1  2002/10/16 13:05:36  Jolt
 *   WDT API driver
 *
 *
 *   $Revision: 1.1 $
 *
 */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/reboot.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/8xx_immap.h>
#include <asm/irq.h>

extern unsigned char __res[];
static bd_t *binfo;
static int wdt_irq_dev;
static struct semaphore wdt_sem;
static u32 wdt_status = 0;
static u32 wdt_timeout = 0;
static u32 wdt_usr_if = 1;

static struct watchdog_info ident = {

	firmware_version: 0,
	identity: "MPC8xx watchdog",
	options: WDIOF_KEEPALIVEPING,

};

static void mpc8xx_wdt_reset(void)
{

	((volatile immap_t *)IMAP_ADDR)->im_siu_conf.sc_swsr = 0x556C;	/* write magic1 */
	((volatile immap_t *)IMAP_ADDR)->im_siu_conf.sc_swsr = 0xAA39;	/* write magic2 */

}

void mpc8xx_wdt_interrupt(int irq, void * dev, struct pt_regs * regs)
{

	mpc8xx_wdt_reset();

	// Clear irq
	((volatile immap_t *)IMAP_ADDR)->im_sit.sit_piscr |= PISCR_PS;

}

static void mpc8xx_wdt_handler_alloc(void)
{

	u32 pitc;
	
#define PITRTCLK 8192

	/*
	 * Fire trigger if half of the wdt ticked down 
	 */
	 
	if ((wdt_timeout) > (UINT_MAX / PITRTCLK))
		pitc = wdt_timeout / binfo->bi_intfreq * PITRTCLK / 2;
	else
		pitc = PITRTCLK * wdt_timeout / binfo->bi_intfreq / 2;

	((volatile immap_t *)IMAP_ADDR)->im_sit.sit_pitc = pitc << 16;
	((volatile immap_t *)IMAP_ADDR)->im_sit.sit_piscr = (mk_int_int_mask(PIT_INTERRUPT) << 8);

	if (request_8xxirq(PIT_INTERRUPT, mpc8xx_wdt_interrupt, 0, "pit", &wdt_irq_dev) != 0)
		panic("mpc8xx_wdt: could not allocate pit irq!");

	printk("mpc8xx_wdt: keep-alive trigger installed (PITC: 0x%04X)\n", pitc);

}

static void mpc8xx_wdt_handler_free(void)
{

	((volatile immap_t *)IMAP_ADDR)->im_sit.sit_piscr = 0;
	
	free_irq(PIT_INTERRUPT, &wdt_irq_dev);
	
}

static void mpc8xx_wdt_handler_disable(void)
{

	((volatile immap_t *)IMAP_ADDR)->im_sit.sit_piscr &= ~(PISCR_PIE | PISCR_PTE);

	printk("mpc8xx_wdt: keep-alive trigger deactivated");

}

static void mpc8xx_wdt_handler_enable(void)
{

	printk("mpc8xx_wdt: keep-alive trigger activated");

	((volatile immap_t *)IMAP_ADDR)->im_sit.sit_piscr |= PISCR_PIE | PISCR_PTE;

}

static int mpc8xx_wdt_open(struct inode *inode, struct file *file)
{

	switch (MINOR(inode->i_rdev)) {

		case WATCHDOG_MINOR:

			if (down_trylock(&wdt_sem))
				return -EBUSY;

			mpc8xx_wdt_reset();
			mpc8xx_wdt_handler_disable();

			return 0;

		default:

			return -ENODEV;

	}

}

static int mpc8xx_wdt_release(struct inode *inode, struct file *file)
{

	mpc8xx_wdt_reset();

#ifndef CONFIG_WATCHDOG_NOWAYOUT	 
	mpc8xx_wdt_handler_enable();
#endif

	up(&wdt_sem);

	return 0;

}

static ssize_t mpc8xx_wdt_write(struct file *file, const char *data, size_t len, loff_t * ppos)
{

	/*
	 * Can't seek (pwrite) on this device  
	 */
	if (ppos != &file->f_pos)
		return -ESPIPE;

	if (!len)
		return 0;

	mpc8xx_wdt_reset();

	return 1;

}

static int mpc8xx_wdt_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{

	switch (cmd) {

		case WDIOC_GETSUPPORT:

			if (copy_to_user((struct watchdog_info *)arg, &ident, sizeof(ident)))
				return -EFAULT;

			return 0;

		case WDIOC_GETSTATUS:
		case WDIOC_GETBOOTSTATUS:

			if (put_user(wdt_status, (int *)arg))
				return -EFAULT;

			wdt_status &= ~WDIOF_KEEPALIVEPING;

			return 0;

		case WDIOC_KEEPALIVE:

			mpc8xx_wdt_reset();

			wdt_status |= WDIOF_KEEPALIVEPING;

			return 0;

		case WDIOC_GETTIMEOUT:

			return put_user((wdt_timeout / binfo->bi_intfreq), (int *)arg);

		default:

			return -ENOTTY;

	}

}

static struct file_operations mpc8xx_wdt_fops = {

	ioctl: mpc8xx_wdt_ioctl,
	open: mpc8xx_wdt_open,
	owner: THIS_MODULE,
	release: mpc8xx_wdt_release,
	write: mpc8xx_wdt_write,

};

static struct miscdevice mpc8xx_wdt_miscdev = {

	fops: &mpc8xx_wdt_fops,
	minor: WATCHDOG_MINOR,
	name: "watchdog",

};

static int __init mpc8xx_wdt_init(void)
{

	u32 sypcr;

	printk("mpc8xx_wdt: $Id: mpc8xx_wdt.c,v 1.1 2002/10/16 13:05:36 Jolt Exp $\n");

	binfo = (bd_t *)__res;

	sypcr = ((volatile immap_t *)IMAP_ADDR)->im_siu_conf.sc_sypcr;

	if (sypcr & 0x04) {
	
		mpc8xx_wdt_reset();

		printk("mpc8xx_wdt: active wdt found (SWTC: 0x%04X, SWP: 0x%01X)\n", (sypcr >> 16), sypcr & 0x01);

		wdt_timeout = (sypcr >> 16) & 0xFFFF;

		if (!wdt_timeout)
			wdt_timeout = 0xFFFF;

		if (sypcr & 0x01)
			wdt_timeout *= 2048;

	} else {

		printk("mpc8xx_wdt: wdt disabled (SYPCR: 0x%08X)\n", sypcr);
	
		return 0;
	
	}
	
	/* HACK HACK HACK BEGIN */
	mpc8xx_wdt_handler_free();
	/* HACK HACK HACK END */
	mpc8xx_wdt_handler_alloc();

	sema_init(&wdt_sem, 1);

	if (misc_register(&mpc8xx_wdt_miscdev)) {
	
		printk(KERN_WARNING "mpc8xx_wdt: cound not register userspace interface\n");
		
		wdt_usr_if = 0;
		
	}
	
	mpc8xx_wdt_reset();

	return 0;

}

static void __exit mpc8xx_wdt_exit(void)
{

	if (wdt_usr_if)
		misc_deregister(&mpc8xx_wdt_miscdev);
		
	mpc8xx_wdt_handler_free();
	mpc8xx_wdt_reset();

}

#ifdef MODULE
MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("MPC8xx watchdog driver");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#endif

module_init(mpc8xx_wdt_init);
module_exit(mpc8xx_wdt_exit);
