/*
 * $Id: ds1307.c,v 1.1 2005/08/27 01:59:05 chakazulu Exp $
 *
 * I2C driver for Dallas (Maxim) DS1307 Real Time Clock.
 * Some code 'borrowed' from the DS1302 driver.
 * 
 * Copyright (C) 2002 Brian Kuschak <bkuschak@yahoo.com>
 * (http://archives.andrew.net.au/lm-sensors/msg19357.html)
 *
 * Modifications (mostly based on saa7126 driver)
 * by Michael Schuele 'ChakaZulu' <tuxbox@mschuele.de>
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

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <asm/uaccess.h>
#include <asm/rtc.h>
#include <linux/proc_fs.h>
#include <linux/devfs_fs_kernel.h>

#define RTC_MAJOR_NR 121 /* local major, change later */

#define I2C_DRIVERID_DS1307	1

/* register adresses */
#define DS1307_SECONDS          0x00
#define DS1307_MINUTES          0x01
#define DS1307_HOURS            0x02
#define DS1307_WEEKDAY          0x03
#define DS1307_DAY_OF_MONTH     0x04
#define DS1307_MONTH            0x05
#define DS1307_YEAR             0x06
#define DS1307_CONTROL          0x07

/*
 * module parameters
 */
static int debug;

#define dprintk if(debug) printk

/* save devfs handle to remove devfs entry when module is removed */
struct ds1307
{
	devfs_handle_t devfs_handle;
	struct i2c_client *i2c_client;
};

static int
rtc_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);

/*
 * The various file operations we support.
 */
static struct file_operations ds1307_fops = {
        owner:          THIS_MODULE,
        ioctl:          rtc_ioctl,	
}; 

/* identifier */
static const char ds1307_name[] = "clock";

/* identifier for devfs entry */
static const char ds1307_devfs_name[] = "dbox/clock";

/* The I2C address we scan looking for DS1307
 */
static unsigned short normal_i2c[] = { 0x68, I2C_CLIENT_END };
static unsigned short normal_i2c_range[] = { 0x68, 0x68, I2C_CLIENT_END }; 
I2C_CLIENT_INSMOD;

static struct i2c_driver rtc_i2c_driver;
static struct i2c_client client_template;
static struct i2c_client *ds1307_client;

static unsigned char days_in_mo[] = 
    {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static void rtc_i2c_init_client(struct i2c_client *pclient);

static void inc_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void dec_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

/* We get here after scanning the I2C device at one of the DS1307 addresses.
 */
static int rtc_i2c_detect_rtc(struct i2c_adapter *adap, int addr, 
				unsigned short flags, int kind)
{
	int err;
	char c = 0;
	int found = 0;
	struct i2c_client *pclient;
	struct ds1307 *rtc;

	pclient = kmalloc(sizeof(struct i2c_client) +
			  sizeof(struct ds1307), GFP_KERNEL);

	if (!pclient)
	{
		printk(KERN_ERR "ds1307: Driver failed memory allocation\n");
		return -ENOMEM;
	}

	memcpy(pclient, &client_template, sizeof(struct i2c_client));
	pclient->addr = addr;
	pclient->adapter = adap;
	strcpy(pclient->name, "ds1307");
	pclient->data = pclient + 1;

	rtc = (struct ds1307*)pclient->data;
	memset(rtc,0x00,sizeof(struct ds1307));
	rtc->i2c_client = pclient;

	dprintk("ds1307: probing...\n");
	if(i2c_master_send(pclient, &c, 1) == 1)
	{
		if(i2c_master_recv(pclient, &c, 1) == 1)
		{
			printk("ds1307: I2C Real-Time-Clock detected at addr 0x%x\n", pclient->addr);
			found = 1;
		}
	}
	if(found)
	{
		dprintk("ds1307: registering devfs entry\n");
		rtc->devfs_handle = 
			devfs_register(NULL, ds1307_devfs_name, DEVFS_FL_DEFAULT, 0, 0,
				       S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP 
				       | S_IWGRP | S_IROTH | S_IWOTH,
				       &ds1307_fops, rtc);
		if (!rtc->devfs_handle) {
			printk(KERN_ERR "ds1307: devfs_register failed\n");
			kfree(pclient);
			return -EIO;
		}
		dprintk("ds1307: attaching i2c client\n");
		if((err = i2c_attach_client(pclient)))
		{
			printk(KERN_ERR "ds1307: failed to attach client\n");
			kfree(pclient);
			return err;
		}
		dprintk("ds1307: rtc_i2c_init_client\n");
		rtc_i2c_init_client(pclient);
	}
	else
	{
		printk(KERN_ERR "ds1307: chip not found :(\n");
		kfree(pclient);
	}
	return 0;
}

static int detach_client(struct i2c_client *client)
{
	int ret;
	struct ds1307 *rtc = (struct ds1307*)client->data;
	dprintk("ds1307: unregistering devfs entry\n");
	devfs_unregister(rtc->devfs_handle);
	dprintk("ds1307: detaching i2c client\n");
	if ((ret = i2c_detach_client(client))) {
		printk(KERN_ERR "ds1307: i2c_detach_client failed\n");
		return ret;
	}

	kfree(client);
	return 0;
}

static int i2c_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	return -1;
}

static int attach_adapter(struct i2c_adapter *adap)
{
	dprintk("ds1307: i2c_probe\n");
	int ret = i2c_probe(adap, &addr_data, rtc_i2c_detect_rtc);
	return ret;
}

static int ds1307_readreg(struct i2c_client *pclient, int reg)
{
	unsigned char c = reg;

	dprintk("ds1307: %s\n",__FUNCTION__);
	
	if(!pclient)
		return 0;

	/* set pointer register */
	if(i2c_master_send(pclient, &c, 1) == 1)
	{
		/* read data register */
		if(i2c_master_recv(pclient, &c, 1) == 1)
		{
			return c;
		}
	}
	return 0;
}

static int ds1307_writereg(struct i2c_client *pclient, int val, int reg)
{
	unsigned char buf[2];

	dprintk("ds1307: %s\n",__FUNCTION__);
	
	if(!pclient)
		return 0;

	buf[0] = reg;
	buf[1] = val;

	if(i2c_master_send(pclient, buf, sizeof(buf)) == sizeof(buf))
	{
		return val;
	}
	return 0;
}

static void rtc_i2c_init_client(struct i2c_client *pclient)
{
	int val;

	dprintk("ds1307: %s\n",__FUNCTION__);

	/* Datasheet says the oscillator may not be enabled on the first powerup. 
	 * Enable it if necessary.
	 */
	if((val = ds1307_readreg(pclient, DS1307_SECONDS)) & 0x80) {
		printk("ds1307: need to activate crystal\n");
		ds1307_writereg(pclient, (val & ~0x80), DS1307_SECONDS);
	}
	ds1307_client = pclient;	 /* hack! */
}

void
ds1307_get_rtc_time(struct rtc_time *rtc_tm) 
{
	unsigned long flags;
	struct i2c_client *pclient = ds1307_client;	/* hack! */
		
	dprintk("ds1307: %s\n",__FUNCTION__);

	save_flags(flags);
	cli();

	rtc_tm->tm_sec  = (ds1307_readreg(pclient, DS1307_SECONDS     ) & 0x7f);
	rtc_tm->tm_min  = (ds1307_readreg(pclient, DS1307_MINUTES     ) & 0x7f);
	rtc_tm->tm_hour = (ds1307_readreg(pclient, DS1307_HOURS       ) & 0x3f);
	rtc_tm->tm_mday = (ds1307_readreg(pclient, DS1307_DAY_OF_MONTH) & 0x3f);
	rtc_tm->tm_mon  = (ds1307_readreg(pclient, DS1307_MONTH       ) & 0x1f);
	rtc_tm->tm_year =  ds1307_readreg(pclient, DS1307_YEAR);
	rtc_tm->tm_wday = (ds1307_readreg(pclient, DS1307_WEEKDAY     ) & 0x07);

	restore_flags(flags);
	
	BCD_TO_BIN(rtc_tm->tm_sec);
	BCD_TO_BIN(rtc_tm->tm_min);
	BCD_TO_BIN(rtc_tm->tm_hour);
	BCD_TO_BIN(rtc_tm->tm_mday);
	BCD_TO_BIN(rtc_tm->tm_mon);
	BCD_TO_BIN(rtc_tm->tm_year);
	BCD_TO_BIN(rtc_tm->tm_wday);

	/*
	 * Account for differences between how the RTC uses the values
	 * and how they are defined in a struct rtc_time;
	 */

	if (rtc_tm->tm_year <= 69)
		rtc_tm->tm_year += 100;

	rtc_tm->tm_mon--;
}

/* ioctl that supports RTC_RD_TIME and RTC_SET_TIME (read and set time/date) */

static int
rtc_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	  unsigned long arg) 
{
        unsigned long flags;
	struct i2c_client *pclient = ds1307_client;	/* hack! */

	dprintk("ds1307: %s\n",__FUNCTION__);

	switch(cmd) {
		case RTC_RD_TIME:	/* Read the time/date from RTC	*/
		{
			struct rtc_time rtc_tm;
						
			ds1307_get_rtc_time(&rtc_tm);						
			if (copy_to_user((struct rtc_time*)arg, &rtc_tm, sizeof(struct rtc_time)))
				return -EFAULT;	
			return 0;
		}

		case RTC_SET_TIME:	/* Set the RTC */
		{
			struct rtc_time rtc_tm;
			unsigned char mon, day, hrs, min, sec, leap_yr, wday;
			unsigned int yrs;
#if 1
			if (!suser())
				return -EACCES;
#endif			
			if (copy_from_user(&rtc_tm, (struct rtc_time*)arg, sizeof(struct rtc_time)))
				return -EFAULT;    	

			yrs  = rtc_tm.tm_year + 1900;
			mon  = rtc_tm.tm_mon + 1;   /* tm_mon starts at zero */
			day  = rtc_tm.tm_mday;
			hrs  = rtc_tm.tm_hour;
			min  = rtc_tm.tm_min;
			sec  = rtc_tm.tm_sec;
			wday = rtc_tm.tm_wday;

			
			if ((yrs < 1970) || (yrs > 2069))
				return -EINVAL;

			leap_yr = ((!(yrs % 4) && (yrs % 100)) || !(yrs % 400));

			if ((mon > 12) || (day == 0))
				return -EINVAL;

			if (day > (days_in_mo[mon] + ((mon == 2) && leap_yr)))
				return -EINVAL;
			
			if ((hrs >= 24) || (min >= 60) || (sec >= 60))
				return -EINVAL;

			if (wday > 6)
				return -EINVAL;

			if (yrs >= 2000)
				yrs -= 2000;	/* RTC (0, 1, ... 69) */
			else
				yrs -= 1900;	/* RTC (70, 71, ... 99) */

			BIN_TO_BCD(sec);
			BIN_TO_BCD(min);
			BIN_TO_BCD(hrs);
			BIN_TO_BCD(day);
			BIN_TO_BCD(mon);
			BIN_TO_BCD(yrs);
			BIN_TO_BCD(wday);

			save_flags(flags);
			cli();
			ds1307_writereg(pclient, yrs,  DS1307_YEAR);
			ds1307_writereg(pclient, mon,  DS1307_MONTH);
			ds1307_writereg(pclient, day,  DS1307_DAY_OF_MONTH);
			ds1307_writereg(pclient, hrs,  DS1307_HOURS);
			ds1307_writereg(pclient, min,  DS1307_MINUTES);
			ds1307_writereg(pclient, sec,  DS1307_SECONDS);
			ds1307_writereg(pclient, wday, DS1307_WEEKDAY);
			restore_flags(flags);

			/* notice that at this point, the RTC is updated but the kernel
			 * is still running with the old time. you need to set that
			 * separately with settimeofday or adjtimex.
			 */
			return 0;
		}
		default:
			//return -ENOIOCTLCMD;
			return -EINVAL;
	}
}

/*
 *	Info exported via "/proc/rtc".
 */

static int proc_read_rtc_status(char *page, char **start, off_t off,
				int count, int *eof, void *data)
{
	char *out = page;
	int len;
	struct rtc_time tm;

	dprintk("ds1307: %s\n",__FUNCTION__);
 
	ds1307_get_rtc_time(&tm);

	/*
	 * There is no way to tell if the luser has the RTC set for local
	 * time or for Universal Standard Time (GMT). Probably local though.
	 */

	out += sprintf(out,
		       "rtc_time (UTC): %04d-%02d-%02d %02d:%02d:%02d\n",
		       tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		       tm.tm_hour, tm.tm_min, tm.tm_sec);

	len = out - page;
	len -= off;
	if (len < count) {
		*eof = 1;
		if (len <= 0)
			return 0;
	} else
		len = count;
	*start = page + off;
	return len;
}

/* just probe for the RTC and register the device to handle the ioctl needed */

static __init int ds1307_init(void) 
{ 
	int             res;

	printk(KERN_INFO "ds1307: $Id: ds1307.c,v 1.1 2005/08/27 01:59:05 chakazulu Exp $\n");

	if ((res = i2c_add_driver(&rtc_i2c_driver)))
	{
		printk(KERN_ERR "ds1307: I2C driver registration failed\n");
		return res;
	}
#if 0
	if (register_chrdev(RTC_MAJOR_NR, ds1307_name, &ds1307_fops)) {
		printk(KERN_ERR "unable to get major %d for %s\n", RTC_MAJOR_NR, ds1307_name);
		return -1;
	}
#endif
	dprintk("ds1307: creating proc entry\n");
	create_proc_read_entry (ds1307_name, 0, NULL, proc_read_rtc_status, NULL);
	return 0;
}

static __exit void ds1307_cleanup(void)
{
	int             res;
	dprintk("ds1307: %s\n",__FUNCTION__);
	dprintk("ds1307: removing proc entry\n");

	remove_proc_entry(ds1307_name, 0);

#if 0
	if (unregister_chrdev(RTC_MAJOR_NR, ds1307_name)) {
		printk(KERN_ERR "unable to unregister %s\n", ds1307_name);
	}
#endif
	dprintk("ds1307: deleting i2c driver\n");

	if ((res = i2c_del_driver(&rtc_i2c_driver)))
	{
		printk(KERN_ERR "ds1307: I2C driver deregistration failed, "
			       "module not removed.\n");
	}
}

static struct i2c_driver rtc_i2c_driver =
{
	name:		"I2C driver for DS1307 RTC",
	id:		I2C_DRIVERID_DS1307,
	flags:		I2C_DF_NOTIFY,
	attach_adapter:	attach_adapter,
	detach_client:	detach_client,
	command:	i2c_command,
	inc_use:	inc_use,
	dec_use:	dec_use
};

static struct i2c_client client_template = 
{
	name:		"DS1307 I2C",
	id:		I2C_DRIVERID_DS1307,
	data:		NULL,
	flags:		0,
	addr:		0,
	adapter:	NULL,
	driver:		&rtc_i2c_driver
};

module_init(ds1307_init);
module_exit(ds1307_cleanup);

MODULE_DESCRIPTION("I2C driver for DS1307 RTC");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Brian Kuschak <bkuschak@yahoo.com>, Michael Schuele 'ChakaZulu' <tuxbox@mschuele.de>");
MODULE_PARM(debug,"i");
