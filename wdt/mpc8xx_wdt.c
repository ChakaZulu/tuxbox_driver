/*
 * mpc8xx_wdt.c - MPC8xx watchdog driver
 *
 * Copyright (C) 2002 Florian Schirmer <jolt@tuxbox.org>
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

static struct semaphore wdt_sem;
static u32 wdt_status = 0;
static u32 wdt_timeout = 0;

static struct watchdog_info ident = {
	.firmware_version = 0,
	.identity = "MPC8xx watchdog",
	.options = WDIOF_KEEPALIVEPING,
};

void mpc8xx_wdt_reset(void)
{
	volatile immap_t *imap = (volatile immap_t *)IMAP_ADDR;

	imap->im_siu_conf.sc_swsr = 0x556C;	/* write magic1 */
	imap->im_siu_conf.sc_swsr = 0xAA39;	/* write magic2 */
}

#if !defined(MODULE)
static void mpc8xx_wdt_interrupt(int irq, void *dev, struct pt_regs *regs)
{
	volatile immap_t *imap = (volatile immap_t *)IMAP_ADDR;

	mpc8xx_wdt_reset();

	imap->im_sit.sit_piscr |= PISCR_PS;	/* clear irq */
}

static void __init mpc8xx_wdt_handler_install(bd_t *binfo)
{
	volatile immap_t *imap = (volatile immap_t *)IMAP_ADDR;
	u32 pitc;
	u32 sypcr;
	u32 pitrtclk;

	sypcr = imap->im_siu_conf.sc_sypcr;

	if (!(sypcr & 0x04)) {
		printk(KERN_NOTICE "mpc8xx_wdt: wdt disabled (SYPCR: 0x%08X)\n", sypcr);
		return;
	}

	mpc8xx_wdt_reset();

	printk(KERN_NOTICE "mpc8xx_wdt: active wdt found (SWTC: 0x%04X, SWP: 0x%01X)\n",
			(sypcr >> 16), sypcr & 0x01);

	wdt_timeout = (sypcr >> 16) & 0xFFFF;

	if (!wdt_timeout)
		wdt_timeout = 0xFFFF;

	if (sypcr & 0x01)
		wdt_timeout *= 2048;

	/*
	 * Fire trigger if half of the wdt ticked down 
	 */

	if (imap->im_sit.sit_rtcsc & RTCSC_38K)
		pitrtclk = 9600;
	else
		pitrtclk = 8192;

	if ((wdt_timeout) > (UINT_MAX / pitrtclk))
		pitc = wdt_timeout / binfo->bi_intfreq * pitrtclk / 2;
	else
		pitc = pitrtclk * wdt_timeout / binfo->bi_intfreq / 2;

	imap->im_sit.sit_pitc = pitc << 16;
	imap->im_sit.sit_piscr = (mk_int_int_mask(PIT_INTERRUPT) << 8);

	if (request_irq(PIT_INTERRUPT, mpc8xx_wdt_interrupt, 0, "watchdog", NULL))
		panic("mpc8xx_wdt: could not allocate watchdog irq!");

	printk(KERN_NOTICE "mpc8xx_wdt: keep-alive trigger installed (PITC: 0x%04X)\n", pitc);

	wdt_timeout /= binfo->bi_intfreq;
}
#endif

static void mpc8xx_wdt_handler_disable(void)
{
	volatile immap_t *imap = (volatile immap_t *)IMAP_ADDR;

	imap->im_sit.sit_piscr &= ~(PISCR_PIE | PISCR_PTE);

	printk(KERN_NOTICE "mpc8xx_wdt: keep-alive handler deactivated");
}

static void mpc8xx_wdt_handler_enable(void)
{
	volatile immap_t *imap = (volatile immap_t *)IMAP_ADDR;

	imap->im_sit.sit_piscr |= PISCR_PIE | PISCR_PTE;

	printk(KERN_NOTICE "mpc8xx_wdt: keep-alive handler activated");
}

static int mpc8xx_wdt_open(struct inode *inode, struct file *file)
{
	switch (MINOR(inode->i_rdev)) {
	case WATCHDOG_MINOR:
		if (down_trylock(&wdt_sem))
			return -EBUSY;

		mpc8xx_wdt_reset();
		mpc8xx_wdt_handler_disable();
		break;

	default:
		return -ENODEV;
	}

	return 0;
}

static int mpc8xx_wdt_release(struct inode *inode, struct file *file)
{
	mpc8xx_wdt_reset();

#if !defined(CONFIG_WATCHDOG_NOWAYOUT)
	mpc8xx_wdt_handler_enable();
#endif

	up(&wdt_sem);

	return 0;
}

static ssize_t mpc8xx_wdt_write(struct file *file, const char *data,
				size_t len, loff_t * ppos)
{
	/* Can't seek (pwrite) on this device */
	if (ppos != &file->f_pos)
		return -ESPIPE;

	if (!len)
		return 0;

	mpc8xx_wdt_reset();

	return 1;
}

static int mpc8xx_wdt_ioctl(struct inode *inode, struct file *file,
			    unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case WDIOC_GETSUPPORT:
		if (copy_to_user((struct watchdog_info *)arg, &ident, sizeof(ident)))
			return -EFAULT;
		break;

	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		if (put_user(wdt_status, (int *)arg))
			return -EFAULT;
		wdt_status &= ~WDIOF_KEEPALIVEPING;
		break;

	case WDIOC_KEEPALIVE:
		mpc8xx_wdt_reset();
		wdt_status |= WDIOF_KEEPALIVEPING;
		break;

	case WDIOC_GETTIMEOUT:
		if (put_user(wdt_timeout, (int *)arg))
			return -EFAULT;
		break;

	default:
		return -ENOTTY;
	}

	return 0;
}

static struct file_operations mpc8xx_wdt_fops = {
	.owner = THIS_MODULE,
	.write = mpc8xx_wdt_write,
	.ioctl = mpc8xx_wdt_ioctl,
	.open = mpc8xx_wdt_open,
	.release = mpc8xx_wdt_release,
};

static struct miscdevice mpc8xx_wdt_miscdev = {
	.minor = WATCHDOG_MINOR,
	.name = "watchdog",
	.fops = &mpc8xx_wdt_fops,
};

static int __init mpc8xx_wdt_init(void)
{
	int ret;

	sema_init(&wdt_sem, 1);

	if ((ret = misc_register(&mpc8xx_wdt_miscdev))) {
		printk(KERN_WARNING "mpc8xx_wdt: cound not register userspace interface\n");
		return ret;
	}

	return 0;
}

static void __exit mpc8xx_wdt_exit(void)
{
	misc_deregister(&mpc8xx_wdt_miscdev);

	mpc8xx_wdt_reset();
	mpc8xx_wdt_handler_enable();
}

module_init(mpc8xx_wdt_init);
module_exit(mpc8xx_wdt_exit);

MODULE_AUTHOR("Florian Schirmer <jolt@tuxbox.org>");
MODULE_DESCRIPTION("MPC8xx watchdog driver");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(mpc8xx_wdt_reset);
